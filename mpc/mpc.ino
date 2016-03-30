// Control program for "Magic POV Cylinder"
// Harald Bauer, 2015-01-26
//
#include <avr/pgmspace.h>
#include "MemoryFree.h"
#include "LPD8806.h"
#include "SPI.h"
#include "mpcgif.h"
#include "pictures.h"
#include "bt.h"
#include "trace.h"

#pragma GCC optimize ("-O0")

#define NO_INTERRUPT 0
static int MOTOR_OFF;

// Mapping of LED strips to frameBuffer (for wheel=0)
#define STRIP0X     8
#define STRIP1X    12
#define STRIP2X     0
#define STRIP3X     4

#define STRIP0Y     0
#define STRIP1Y     1
#define STRIP2Y     2
#define STRIP3Y     3

#define STRIP0DY    4
#define STRIP1DY    4
#define STRIP2DY    4
#define STRIP3DY    4

#define MAXFILESIZE 5000

// download GIF image is stored here
unsigned int gifFileDataLen;
unsigned char gifFileData[MAXFILESIZE];




#define SPI_CLOCK_RATE    6000000  // 6 MHz
#define USART_CLOCK_RATE  5000000  // 5 MHz

// definitions for RPM measurement (for TC0)
// Paramters table:
// TC0, 0, TC0_IRQn  =>  TC0_Handler()
// TC0, 1, TC1_IRQn  =>  TC1_Handler()
// TC0, 2, TC2_IRQn  =>  TC2_Handler()
// TC1, 0, TC3_IRQn  =>  TC3_Handler()
// TC1, 1, TC4_IRQn  =>  TC4_Handler()
// TC1, 2, TC5_IRQn  =>  TC5_Handler()
// TC2, 0, TC6_IRQn  =>  TC6_Handler()
// TC2, 1, TC7_IRQn  =>  TC7_Handler()
// TC2, 2, TC8_IRQn  =>  TC8_Handler()
#define TCCHAN 1
#define NMAX 9
#define TCIRQ TC1_IRQn

// digital test outputs
#define TEST_RED    20
#define TEST_BLUE   21

GifDisplay gifDisplay;

// the screen has 151x40 pixels
#define XSIZE 151
#define YSIZE  40

// text buffer for sprintf
#define TEXTSIZE 128
static char text[TEXTSIZE];

Trace trace;

//---------------------------------------------------------------------------------------
  void picProst(void)
//---------------------------------------------------------------------------------------
{
  int x,y,i=0;
  
  gifDisplay.init(); // set default color palette
  for (y=0; y<YSIZE; y++) 
    for (x=0; x<XSIZE; x++) 
	   gifDisplay.setThisPixel(x, y, pic_prost_neujahr[i++]); 
}

// Example to control LPD8806-based RGB LED Modules in a strip

/*****************************************************************************/

// Number of RGB LEDs in one LED strip:
// Note: SPI has too strip connected
int nLEDs = 10;    // HBA: was 32


LPD8806 strip0[2]  = { LPD8806(nLEDs, USART0, USART_CLOCK_RATE),
                       LPD8806(nLEDs, USART0, USART_CLOCK_RATE)
                     };
LPD8806 strip1[2] = { LPD8806(nLEDs, USART1, USART_CLOCK_RATE),
                      LPD8806(nLEDs, USART1, USART_CLOCK_RATE)
                    };
LPD8806 strip23[2] = { LPD8806(2*nLEDs, SPI_CLOCK_RATE),
                       LPD8806(2*nLEDs, SPI_CLOCK_RATE)
                     };

// initialization of timer counter (TC)
//----------------------------------------------------------------------------------------
void tcInit(void)
//----------------------------------------------------------------------------------------
{
	pmc_enable_periph_clk(ID_PIOA);
	PIO_SetPeripheral(PIOA, PIO_PERIPH_A, 1 << 2); // TIOA1 --> A2
	pmc_enable_periph_clk(ID_TC1);

	// configure TC channel 1:
	TC_Configure(TC0, TCCHAN,
	TC_CMR_TCCLKS_TIMER_CLOCK3 |    // 3=MCK/32  (2=MCK/8)
	TC_CMR_BURST_NONE          |    // The clock is not gated by an external signal
	TC_CMR_ETRGEDG_NONE        |    // External trigger signal not used
	TC_CMR_ABETRG              |    // TIOA External Trigger Selection
	TC_CMR_LDRA_FALLING        |    // Load RA with falling edge of TIOA
	TC_CMR_LDRB_RISING         |    // Load RB rising edge of TIOA
	0);
	// start channel 1
	TC_Start(TC0, TCCHAN);
}

// the TC is defined in the following include file:
// C:\Program Files (x86)\Arduino\hardware\arduino\sam\system\CMSIS\Device\ATMEL\sam3xa\include\component\component_tc.h
//
// read CV register of timer counter (TC)
//----------------------------------------------------------------------------------------
inline uint32_t tcReadCounter(void)
//----------------------------------------------------------------------------------------
{
	return TC0->TC_CHANNEL[TCCHAN].TC_CV;
}

// read RA register of timer counter (TC)
//----------------------------------------------------------------------------------------
uint32_t tcReadRA(void)
//----------------------------------------------------------------------------------------
{
	if (MOTOR_OFF) {
		static uint32_t ra;
		return ra += 150000;
		} else {
		// wait until register RA has be loaded
		while (tcReadStatusBit(TC_SR_LDRAS) == 0)
		;

		return TC0->TC_CHANNEL[TCCHAN].TC_RA;
	}
}

// read status register of timer counter (TC)
//----------------------------------------------------------------------------------------
uint32_t tcReadStatusBit(uint32_t bitMask)
//----------------------------------------------------------------------------------------
{
	static uint32_t lastStatus;
	if (lastStatus & bitMask) {
		lastStatus &= ~bitMask;
		return bitMask;
	}
	lastStatus |= TC0->TC_CHANNEL[TCCHAN].TC_SR;
	if (lastStatus & bitMask) {
		lastStatus &= ~bitMask;
		return bitMask;
	}
	return 0;
}

// write register RC of timer counter (TC)
//----------------------------------------------------------------------------------------
inline void tcWriteRC(uint32_t value)
//----------------------------------------------------------------------------------------
{
  TC0->TC_CHANNEL[TCCHAN].TC_RC = value;
}


//----------------------------------------------------------------------------------------
inline Colour rgb2led(Colour x)   // from 3*8bit RGB to 3*7bit BRG
//----------------------------------------------------------------------------------------
{
  // gamma correction as described in 
  // https://learn.adafruit.com/led-tricks-gamma-correction/the-issue
  
  const uint8_t gamma[] PROGMEM = {
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
	1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
	2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
	5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
	10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
	17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
	25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
	37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
	51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
	69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
	90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
	115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
	144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
	177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
    215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };	
	
  return (gamma[x>>16]      <<  7 |    // R
          gamma[x>>8 & 255] >>  1 |    // G
          gamma[x & 255]    << 15)     // B
		  & 0x7F7F7F;
}

//----------------------------------------------------------------------------------------
void updateStrip(LPD8806 & strip, int t, int i, int x, int y, int dy, int n)   //, Colour *colours)
//----------------------------------------------------------------------------------------
{
  int x1 = x + t;
  int x2 = x1 - XSIZE;
  x = x2 < 0 ? x1 : x2;

  while (n--) {
    //strip.setPixelColor(i, colorPalette[frameBuffer[x][y]&COLORMASK]);
    strip.setPixelColor(i, rgb2led(gifDisplay.getThisPixelRGB(x, y)));
    y += dy;
    i++;
  }

}


//----------------------------------------------------------------------------------------
void fillColumn(int x, int color)
//----------------------------------------------------------------------------------------
{
  int y;
  
  gifDisplay.init(); // set default color palette
  for (y = 0; y < YSIZE; y++)
    gifDisplay.setThisPixel(x, y, color);
}


//----------------------------------------------------------------------------------------
void fillScreen(int color)
//----------------------------------------------------------------------------------------
{
  int x;
  for (x = 0; x < XSIZE; x++) {
    fillColumn(x, color);
  }
}

// create triangle curve
//----------------------------------------------------------------------------------------
void drawTriangleCurve(void)
//----------------------------------------------------------------------------------------
{
  int x, color = 1;
  int y = 1;
  int ystep = 1;

  fillScreen(BLACK);

  for (x = 0; x < XSIZE; x++) {
    gifDisplay.setThisPixel(x, y, color);
    color = color == 4 ? 1 : color + 1;
    if ((y == (YSIZE - 1)) || (y == 0)) {
      ystep = -ystep;
    }
    y += ystep;
  }
}


//----------------------------------------------------------------------------------------
int readInt(const char *msg, int defaultValue=0) {
  //----------------------------------------------------------------------------------------
  int x;
  sprintf(text, "%s [%d]: ", msg, defaultValue);
  btWriteString(text);
  btReadString(text, TEXTSIZE);
  btWriteString("\n");
  if (*text) sscanf(text, "%d", &x);
  else x=defaultValue;
  return x;
}


//----------------------------------------------------------------------------------------
void drawRow(void)
//----------------------------------------------------------------------------------------
{
  int x;
  uint32_t row = 0, color = 1;

  row = readInt("\nEnter row (0-39)", 0);
  color = readInt("Enter color (0-7)", 1);

  if (row >= YSIZE) return;
  if (color > WHITE) return;

  gifDisplay.init(); // set default color palette
  for (x = 0; x < XSIZE; x++) {
    gifDisplay.setThisPixel(x, row, color);
  }
}


//----------------------------------------------------------------------------------------
void drawColumn(void)
//----------------------------------------------------------------------------------------
{
  uint32_t col = 0, color = 1;

  col = readInt("\nEnter column (0-150)", 0);
  color = readInt("Enter color (0-7)", 1);

  if (col >= XSIZE) return;
  if (color > WHITE) return;
  fillColumn(col, color);
}


// CRC according to CCITT. See: http://automationwiki.com/index.php?title=CRC-16-CCITT
//-----------------------------------------------------------------------------
uint16_t crc(uint8_t *data, uint32_t length)
//-----------------------------------------------------------------------------
{
  static unsigned short crc_table [256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5,
    0x60c6, 0x70e7, 0x8108, 0x9129, 0xa14a, 0xb16b,
    0xc18c, 0xd1ad, 0xe1ce, 0xf1ef, 0x1231, 0x0210,
    0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c,
    0xf3ff, 0xe3de, 0x2462, 0x3443, 0x0420, 0x1401,
    0x64e6, 0x74c7, 0x44a4, 0x5485, 0xa56a, 0xb54b,
    0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6,
    0x5695, 0x46b4, 0xb75b, 0xa77a, 0x9719, 0x8738,
    0xf7df, 0xe7fe, 0xd79d, 0xc7bc, 0x48c4, 0x58e5,
    0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969,
    0xa90a, 0xb92b, 0x5af5, 0x4ad4, 0x7ab7, 0x6a96,
    0x1a71, 0x0a50, 0x3a33, 0x2a12, 0xdbfd, 0xcbdc,
    0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03,
    0x0c60, 0x1c41, 0xedae, 0xfd8f, 0xcdec, 0xddcd,
    0xad2a, 0xbd0b, 0x8d68, 0x9d49, 0x7e97, 0x6eb6,
    0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a,
    0x9f59, 0x8f78, 0x9188, 0x81a9, 0xb1ca, 0xa1eb,
    0xd10c, 0xc12d, 0xf14e, 0xe16f, 0x1080, 0x00a1,
    0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c,
    0xe37f, 0xf35e, 0x02b1, 0x1290, 0x22f3, 0x32d2,
    0x4235, 0x5214, 0x6277, 0x7256, 0xb5ea, 0xa5cb,
    0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447,
    0x5424, 0x4405, 0xa7db, 0xb7fa, 0x8799, 0x97b8,
    0xe75f, 0xf77e, 0xc71d, 0xd73c, 0x26d3, 0x36f2,
    0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9,
    0xb98a, 0xa9ab, 0x5844, 0x4865, 0x7806, 0x6827,
    0x18c0, 0x08e1, 0x3882, 0x28a3, 0xcb7d, 0xdb5c,
    0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0,
    0x2ab3, 0x3a92, 0xfd2e, 0xed0f, 0xdd6c, 0xcd4d,
    0xbdaa, 0xad8b, 0x9de8, 0x8dc9, 0x7c26, 0x6c07,
    0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba,
    0x8fd9, 0x9ff8, 0x6e17, 0x7e36, 0x4e55, 0x5e74,
    0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
  };

  size_t count;
  unsigned int crc = 0; // seed=0
  unsigned int temp;


  for (count = 0; count < length; ++count)
  {
    temp = (*data++ ^ (crc >> 8)) & 0xff;
    crc = crc_table[temp] ^ (crc << 8);
  }

  return (unsigned short)(crc ^ 0);  // final=0
}
//----------------------------------------------------------------------------------------
void halt(char *text)
//----------------------------------------------------------------------------------------
{
  btWriteString(text);
  btWriteString("\nSystem halted\n");
  while (1)
    ;
}


//----------------------------------------------------------------------------------------
void download_gif_file(void)
//----------------------------------------------------------------------------------------
{
  uint32_t fileSize;
  uint16_t crcValue, crcCheck;

  // format: '&'         - 1 byte start character
  //         size        - 4 bytes
  //         data[size]  - size byte
  //         crc         - 2 bytes
  if (btReadChar() != '&') {
    btWriteString("\nGIF file download aborted!\n");
    return;
  }
  btReadData((uint8_t *)&fileSize, 4);
  sprintf(text, "fileSize (0x%08lX) > MAXFILESIZE (0x%08X)", fileSize, MAXFILESIZE);
  if (fileSize > MAXFILESIZE) halt(text);

  btReadData(gifFileData, fileSize);

  btReadData((uint8_t *)&crcValue, 2);
  crcCheck = crc(gifFileData, fileSize);
  sprintf(text, "CRC failure (0x%04X versus expected 0x%04X\n", crcCheck, crcValue);
  if (crcCheck != crcValue) halt(text);

  btWriteString("CRC OK\n");
  gifFileDataLen = fileSize;
}

#if 1
  unsigned int top2()
  {
	  char top2=0;
	  unsigned int addr;
	  char dummy[5000];
	  dummy[99]=top2;
	  addr = (unsigned int) &dummy[50];
	  return addr;
  }

extern "C" char* sbrk(int incr);
extern volatile int tickc;
  
  void check_stack()
  {
	  static unsigned int size, heap;
	  char top;

	  sprintf(text, "\ntickc: 0x%08X\n", (unsigned) &tickc);
	  btWriteString(text);
	  
	  sprintf(text, "top:   0x%08X\n", (unsigned) &top);
	  btWriteString(text);

	  sprintf(text, "top2:  0x%08X\n", top2());
	  btWriteString(text);

	  heap=(unsigned int) reinterpret_cast<char*>(sbrk(0));
	  sprintf(text, "heap:  0x%08X\n", heap);
	  btWriteString(text);

	  size = (unsigned int)&top - heap;
	  sprintf(text, "Free: %d\n", size);
	  btWriteString(text); 
  }
#endif


//----------------------------------------------------------------------------------------
void setup()
//----------------------------------------------------------------------------------------
{
  int x;
  const char *animation[4] = { "\b-", "\b/", "\b|", "\b\\" };
  char ch;
  uint32_t period;
  uint32_t ra[NMAX];

  btInit(9600);
  
  check_stack();

  btWriteString("\nPress any key to start:  ");

  x = 0;
  while (btCharAvailable() == 0) {
    btWriteString(animation[x]);
    x = (x + 1) & 3;
    delay(125);
  }
  btReadChar();

  btWriteString("\n\n\n\nPOV Cylinder - 2015-04-22\n");
  while (1) {
    btWriteString("\nPlease ensure that LED power supply switched on!\n");
    btWriteString("  Press 'n' to start in NORMAL mode\n");
    btWriteString("  Press 'd' to start in DEBUG mode (MOTOR_OFF)\n");
    ch = btReadChar();
    if (ch == 'n') {
      MOTOR_OFF = 0;
      break;
    }
    if (ch == 'd') {
      MOTOR_OFF = 1;
      break;
    }
  }

  // Start up the LED strips
  strip0[0].begin();
  strip1[0].begin();
  strip23[0].begin();
  strip0[1].begin();
  strip1[1].begin();
  strip23[1].begin();

  for (x = 0; x < nLEDs; x++) {
    strip0[0].setPixelColor(x, 0, 0, 127);
    strip1[0].setPixelColor(x, 0, 127, 0);
    strip23[0].setPixelColor(x, 127, 0, 0);
    //strip23[1].setPixelColor(x, 127, 127, 0);
  }

  // Update the strip
  showAll(strip0[0], strip1[0], strip23[0]);

  fillScreen(BLACK);
  //fillColumn(0, RED);
  //fillColumn(90, BLUE);

  drawTriangleCurve();

  tcInit();

  pinMode(TEST_RED, OUTPUT);
  pinMode(TEST_BLUE, OUTPUT);

  for (x = 0; x < NMAX; x++) {
    ra[x] = tcReadRA();
    sprintf(text, "Counter: %lu   RA: %lu\n", tcReadCounter(), ra[x]);
    btWriteString(text);
  }

  for (x = 1; x < NMAX; x++) {
    period = (ra[x] - ra[x - 1]) * 32 / 84; // in us
    sprintf(text, "%d: Period = %lu us = %lu RPM = %lu Hz\n", x, period, 1000000 * 60 / period, 1000000 / period);
    btWriteString(text);
  }

  isrColumnTickInit();
}

// variables used by interrupt routine
static uint32_t lastCapture;
static uint32_t nextColumnTimeInt;
static uint32_t nextColumnTimeFrac;
static uint32_t period;
static uint32_t columnDurationInt;
static uint32_t columnDurationFrac;
static int wheel;  // wheel is incremented modulo XSIZE
static int rotVal = 0;
static int rotInc = 0;
static int toggle = 0;      // index for toggle buffer - toggles between 0 and 1
static int numColumnsSkipped = 0;
uint32_t rotationCounter;


//----------------------------------------------------------------------------------------
void setRotation(void)
//----------------------------------------------------------------------------------------
{
	static int rotValInit;
	
	rotInc = readInt("\nEnter rotation increment (0, 1)", rotInc);
	if (rotInc==0) rotValInit = readInt("Enter rotation value (0-150)", rotValInit);
	rotVal = rotValInit;
}

//----------------------------------------------------------------------------------------
void printInfo(int repeatFlag)
//----------------------------------------------------------------------------------------
{
  //const double C = 84.*1000000. / 32;
  int lastToggle = -1;
  do {
    //sprintf(text, "\rRotation: %5.2f Hz / %5d us (%d columns skipped)", C / (double) period, (period*32+41)/84, numColumnsSkipped);
    if (lastToggle != toggle) {
		sprintf(text, "{c%lu}{p%lu}{s%d}", rotationCounter, (period*32+41)/84, numColumnsSkipped);
        btWriteString(text);
		lastToggle = toggle;
	}
  } while (repeatFlag && btCharAvailable() == 0);
}


//----------------------------------------------------------------------------------------
void playGifInRom(void)
//----------------------------------------------------------------------------------------
{
  unsigned int i, select;
 
  
  btWriteString("\nPlease select GIF file in ROM:\n");
  for (i=0; gifFiles[i].length>0; i++) {
      sprintf(text, "%d %20s %d\n", i, gifFiles[i].name, gifFiles[i].length);
      btWriteString(text);
  }
  select = readInt("Select GIF file");
  if (select >= i) {
        btWriteString("Wrong input\n");
        return;
  }

  trace.log('Y', 0);
  rotInc = readInt("\nEnter rotation increment (0, 1)", gifFiles[select].rotinc);
  trace.log('Y', 1);
  if (rotInc==0) rotVal = readInt("\nEnter rotation value (0-150)", gifFiles[select].rotval);
  //btWriteString("Rotation Frequency in Hz/us:      ");
  trace.log('Y', 2);
  
  while (!btCharAvailable()) {
    //btWriteString("g");
    //printInfo(0);
    //read_gif_file("dummy");
	trace.log('Y', 3);
    gifDisplay.showGif(gifFiles[select].length, gifFiles[select].data);
	trace.log('Y', 4);
    //btWriteString("G");
  }
  btWriteString("\n");
}

//----------------------------------------------------------------------------------------
void playGif(void)
//----------------------------------------------------------------------------------------
{
  if (gifFileDataLen == 0) {
	  btWriteString("No GIF filed loaded!\n");
	  return;
  }
  btWriteString("\nPlaying downloaded GIF file...\n");
  //btWriteString("Rotation Frequency in Hz:      ");
  
  while (!btCharAvailable()) {
    //btWriteString("g");
    //printInfo(0);
    //read_gif_file("dummy");
    gifDisplay.showGif(gifFileDataLen, gifFileData);
    //btWriteString("G");
  }
  btWriteString("\n");
}

//----------------------------------------------------------------------------------------
void toggleSingleStepMode(void)
//----------------------------------------------------------------------------------------
{
	if (gifDisplay.singleStepMode) {
		gifDisplay.singleStepMode = false;
		btWriteString("\nSingle step mode disabled\n");
	} else {
		gifDisplay.singleStepMode = true;
		btWriteString("\nSingle step mode enabled\n");		
	}
}

//----------------------------------------------------------------------------------------
void loop(void)
//----------------------------------------------------------------------------------------
{
  char ch;

  while (1) {
	sprintf(text, "\nFree memory: %d bytes\nTrace: %s\n", freeMemory(), trace.isStopped() ? "stopped" : "running");
	btWriteString(text);
	btWriteString("0-7=fill screen 0=black/1=red/2=yellow/3=green/4=cyan/5=blue/6=violet/7=white\n");
	btWriteString("t=draw triangle curve          s=set rotation increment and value\n");
	btWriteString("r=draw row                     c=draw column\n");
	btWriteString("p=Prost Neujahr!               y=play GIF in Flash\n");
	btWriteString("f=download GIF file            x=play downloaded GIF\n");
	btWriteString(":=toggle single step mode      %=print trace\n");

    trace.log('L', 0);
	printInfo(1);
	ch = btReadChar();
    trace.log('L', ch);
	switch (ch) {
		case 'x': playGif();               break;
		case 'y': playGifInRom(); 
		          trace.stop();            break;
		case 'f': download_gif_file();     break;
		case 't': drawTriangleCurve();     break;
		case '0': fillScreen(BLACK);       break;
		case '1': fillScreen(RED);         break;
		case '2': fillScreen(YELLOW);      break;
		case '3': fillScreen(GREEN);       break;
		case '4': fillScreen(CYAN);        break;
		case '5': fillScreen(BLUE);        break;
		case '6': fillScreen(VIOLET);      break;
		case '7': fillScreen(WHITE);       break;
		case 'p': picProst();              break;
		case 'r': drawRow();               break;
		case 'c': drawColumn();            break;
		case 's': setRotation();           break;
		case ':': toggleSingleStepMode();  break;
		case '%': trace.stop();
		          trace.print();
				  trace.start();           break;
	}
  };

#if NO_INTERRUPT
  isrColumnTickInit();

  while (btCharAvailable() == 0) {
    isrColumnTick();
  }
#endif
}

//----------------------------------------------------------------------------------------
static void prepareNextRotation(void) {
  //----------------------------------------------------------------------------------------
  uint32_t newCapture;
  //GifPicture VOLATILE *tmp;
  if (MOTOR_OFF) {
    newCapture = tcReadCounter();
    period = 150000;
  } else {
    newCapture = tcReadRA();
    period = newCapture - lastCapture;
  }
  columnDurationInt = period / XSIZE;
  columnDurationFrac = period % XSIZE;

  lastCapture = newCapture;
  nextColumnTimeInt = newCapture;
  nextColumnTimeFrac = 0;

  rotationCounter++;
  gifDisplay.nextPictureTick();
}

//----------------------------------------------------------------------------------------
static void prepareNextColumn(void)
//----------------------------------------------------------------------------------------
{  
  int w;
  int timeAvailable;
  int timeAvailableMin = 50; // columnDurationInt >> 2;
  //VOLATILE GifPalette *cmap;

  // compute time for next column interrupt and ensure
  // that this is in the future, otherwise skip columns
  while (1) {
      if (++wheel >= XSIZE) {
        wheel = 0;
        prepareNextRotation();
        rotVal += rotInc;
        if (rotVal >= XSIZE) rotVal -= XSIZE;
      }
            
      nextColumnTimeInt += columnDurationInt;
      nextColumnTimeFrac += columnDurationFrac;
      if (nextColumnTimeFrac >= XSIZE) {
        nextColumnTimeFrac -= XSIZE;
        nextColumnTimeInt++;
      }
      timeAvailable = nextColumnTimeInt - tcReadCounter();
      if (timeAvailable >= timeAvailableMin) break;
	  ++numColumnsSkipped;
  }
  
  tcWriteRC(nextColumnTimeInt);
  tcReadStatusBit(TC_SR_CPCS);   // dummy read to status register

  //  update all strips for next wheel position in parallel to DMA output for last wheel position
  //digitalWrite(TEST_RED, HIGH); // Red wire
  toggle = !toggle;
  w = wheel + rotVal;
  if (w >= XSIZE) w -= XSIZE;

  updateStrip(strip0[toggle],  w,  0, STRIP0X, STRIP0Y, STRIP0DY, 10);
  updateStrip(strip1[toggle],  w,  0, STRIP1X, STRIP1Y, STRIP1DY, 10);

  updateStrip(strip23[toggle], w,  0, STRIP2X, STRIP2Y, STRIP2DY, 10);
  updateStrip(strip23[toggle], w, 10, STRIP3X, STRIP3Y, STRIP3DY, 10);
  //digitalWrite(TEST_RED, LOW);
}

//----------------------------------------------------------------------------------------
void isrColumnTick(void) {
  //----------------------------------------------------------------------------------------
#if NO_INTERRUPT
  while (((int) (tcReadCounter() - nextColumnTimeInt) < 0)  && !btCharAvailable())
    ;
#else
  // while(!tcReadStatusBit(TC_SR_CPCS) && !btCharAvailable());
  tcReadStatusBit(TC_SR_CPCS);
#endif
  // wait until all DMAs from previous showAll() are completed
  waitShowAllReady();
  showAll(strip0[toggle], strip1[toggle], strip23[toggle]);
  prepareNextColumn();
}


//----------------------------------------------------------------------------------------
void isrColumnTickInit(void) 
//----------------------------------------------------------------------------------------
{  
  //showAll(strip01[toggle], strip23[toggle], strip45[toggle]);
  showAll(strip0[toggle], strip1[toggle], strip23[toggle]);
  tcReadRA(); // dummy read for synchronization
  lastCapture = tcReadRA();
  wheel = XSIZE-1;
  //prepareNextRotation();
  prepareNextColumn();
#if NO_INTERRUPT==0
  TC0->TC_CHANNEL[TCCHAN].TC_IER = TC_IER_CPCS;
  TC0->TC_CHANNEL[TCCHAN].TC_IDR = ~TC_IER_CPCS;
  NVIC_EnableIRQ(TCIRQ);
#endif
}

//----------------------------------------------------------------------------------------
void TC1_Handler(void) {
  //----------------------------------------------------------------------------------------
  isrColumnTick();
}
