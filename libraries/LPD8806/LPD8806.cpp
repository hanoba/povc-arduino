/*
Arduino library to control LPD8806-based RGB LED Strips
Copyright (C) Adafruit Industries
MIT license

Clearing up some misconceptions about how the LPD8806 drivers work:

The LPD8806 is not a FIFO shift register.  The first data out controls the
LED *closest* to the processor (unlike a typical shift register, where the
first data out winds up at the *furthest* LED).  Each LED driver 'fills up'
with data and then passes through all subsequent bytes until a latch
condition takes place.  This is actually pretty common among LED drivers.

All color data bytes have the high bit (128) set, with the remaining
seven bits containing a brightness value (0-127).  A byte with the high
bit clear has special meaning (explained later).

The rest gets bizarre...

The LPD8806 does not perform an in-unison latch (which would display the
newly-transmitted data all at once).  Rather, each individual byte (even
the separate G, R, B components of each LED) is latched AS IT ARRIVES...
or more accurately, as the first bit of the subsequent byte arrives and
is passed through.  So the strip actually refreshes at the speed the data
is issued, not instantaneously (this can be observed by greatly reducing
the data rate).  This has implications for POV displays and light painting
applications.  The 'subsequent' rule also means that at least one extra
byte must follow the last pixel, in order for the final blue LED to latch.

To reset the pass-through behavior and begin sending new data to the start
of the strip, a number of zero bytes must be issued (remember, all color
data bytes have the high bit set, thus are in the range 128 to 255, so the
zero is 'special').  This should be done before each full payload of color
values to the strip.  Curiously, zero bytes can only travel one meter (32
LEDs) down the line before needing backup; the next meter requires an
extra zero byte, and so forth.  Longer strips will require progressively
more zeros.  *(see note below)

In the interest of efficiency, it's possible to combine the former EOD
extra latch byte and the latter zero reset...the same data can do double
duty, latching the last blue LED while also resetting the strip for the
next payload.

So: reset byte(s) of suitable length are issued once at startup to 'prime'
the strip to a known ready state.  After each subsequent LED color payload,
these reset byte(s) are then issued at the END of each payload, both to
latch the last LED and to prep the strip for the start of the next payload
(even if that data does not arrive immediately).  This avoids a tiny bit
of latency as the new color payload can begin issuing immediately on some
signal, such as a timer or GPIO trigger.

Technically these zero byte(s) are not a latch, as the color data (save
for the last byte) is already latched.  It's a start-of-data marker, or
an indicator to clear the thing-that's-not-a-shift-register.  But for
conversational consistency with other LED drivers, we'll refer to it as
a 'latch' anyway.

* This has been validated independently with multiple customers'
  hardware.  Please do not report as a bug or issue pull requests for
  this.  Fewer zeros sometimes gives the *illusion* of working, the first
  payload will correctly load and latch, but subsequent frames will drop
  data at the end.  The data shortfall won't always be visually apparent
  depending on the color data loaded on the prior and subsequent frames.
  Tested.  Confirmed.  Fact.
*/

#include "LPD8806.h"
#include <SPI.h>

static void spiInit(uint8_t spiRate);
static void spiBegin(void);
static void dmac_disable(void);
static void dmac_enable(void);
//static void spiSend(uint8_t b);
static void spiSend(const uint8_t* buf, size_t len, bool w);
//static void usartDmaTX(Usart *p, const uint8_t* src, uint16_t count);
//static void usartSend(Usart *p, uint8_t b, bool w);
static void usartSend(Usart *p, const uint8_t* buf, size_t len, bool w);
static bool dmac_channel_transfer_done(uint32_t ul_num);

#define CS 14
// dmaspi   from SdFat lib

#define USE_ARDUINO_SPI_LIBRARY 0
#define  USE_NATIVE_SAM3X_SPI 1



/** Use SAM3X DMAC if nonzero */
#define USE_SAM3X_DMAC 1
/** Use extra Bus Matrix arbitration fix if nonzero */
#define USE_SAM3X_BUS_MATRIX_FIX 0
/** Time in ms for DMA receive timeout */
#define SAM3X_DMA_TIMEOUT 100
/** chip select register number */
#define SPI_CHIP_SEL 3

// DMAC transmit channels
#define SPI_DMAC_TX_CH    0
#define USART0_DMAC_TX_CH  2
#define USART1_DMAC_TX_CH  3
// DMAC Channel HW Interface Numbers for SPI/USART TX. (see data sheet p.350
#define SPI_TX_IDX     1
#define USART0_TX_IDX  11
#define USART1_TX_IDX  13

// send single byte via SPI or UASRT in SPI mode
//------------------------------------------------------------------------------
  void LPD8806::spi_out(uint8_t n) {
//------------------------------------------------------------------------------
    switch (spiSelect) {
      case SPI_SPI: 
        SPI.transfer(n); 
        break;
        
      case SPI_USART: 
        USART_Write(pUsart, n, 1000);
        break;
        
      case SPI_ALL: 
        USART_Write(USART0, n, 1000);
        USART_Write(USART1, n, 1000);
        SPI.transfer(n);         
        break;
	  case SPI_SW:
	    break;
    }
}

// send len bytes via SPI or UASRT in SPI mode
//------------------------------------------------------------------------------
  void LPD8806::spi_out_buf(uint8_t *ptr, size_t len) {
//------------------------------------------------------------------------------
    switch (spiSelect) {
      case SPI_SPI: 
        spiSend(ptr, len, true); 
        break;
        
      case SPI_USART: 
        usartSend(pUsart, ptr, len, true);
        break;
        
      case SPI_ALL: 
        spiSend(ptr, len, true);         
        usartSend(USART0, ptr, len, true);
        usartSend(USART1, ptr, len, true);
        break;

	  case SPI_SW:
	  break;
    }
}
/*****************************************************************************/

// Constructor for use with hardware SPI (specific clock/data pins):
//------------------------------------------------------------------------------
  LPD8806::LPD8806(uint16_t n, int32_t clk) {
//------------------------------------------------------------------------------
  spiSelect = clk < 0 ? SPI_ALL : SPI_SPI;
  spiClock  = clk < 0 ? -clk : clk; // in Hz
  pixels = NULL;
  //begun  = false;
  updateLength(n);
  //updatePins();
}

// Constructor for use with USART in SPI mode(specific clock/data pins):
//------------------------------------------------------------------------------
  LPD8806::LPD8806(uint16_t n, Usart *p, int32_t clk) {
//------------------------------------------------------------------------------
  spiSelect = clk < 0 ? SPI_ALL : SPI_USART;
  spiClock  = clk < 0 ? -clk : clk; // in Hz
  pUsart = p;
  pixels = NULL;
  //begun  = false;
  updateLength(n);
  //updatePins(pUsart);
}

// Constructor for use with arbitrary clock/data pins:
//------------------------------------------------------------------------------
  LPD8806::LPD8806(uint16_t n, uint8_t dpin, uint8_t cpin) {
//------------------------------------------------------------------------------
  spiSelect = SPI_SW;
  spiClock = 1000000;   // initialized, but not used in SW mode
  pixels = NULL;
  //begun  = false;
  updateLength(n);
  datapin     = dpin;
  clkpin      = cpin;
  spiSelect = SPI_SW;
}

// Activate hard/soft SPI as appropriate:
//------------------------------------------------------------------------------
  void LPD8806::begin(void) {
//------------------------------------------------------------------------------
  switch (spiSelect) {
    case SPI_SPI:    startSPI();       break;
    case SPI_SW:     startBitbang();   break;
    case SPI_USART:  startUSART();     break;
    case SPI_ALL:    startUSART();   startSPI();  break;
  }

  // Issue initial latch/reset to strip:
  for(uint16_t i=((numLEDs+31)/32); i>0; i--) spi_out(0);
  //begun = true;
}


// Enable SPI hardware and set up protocol details:
//----------------------------------------------------------------------------
   void LPD8806::startSPI(void) {
//----------------------------------------------------------------------------
#if 0
  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);             // CPOL=0; NCPHA=1; out falling; in rising; inactive=low
	uint32_t ch = BOARD_PIN_TO_SPI_CHANNEL(BOARD_SPI_DEFAULT_SS);
  
  Serial.print("SPI Channel: ");
  Serial.println(ch);	
  Serial.print("SPI Mode Register: ");
  Serial.println(SPI0->SPI_MR);
  	
  // SPI_CSR_DLYBCT(1) keeps CS enabled for 32 MCLK after a completed
	// transfer. Some device needs that for working properly.
  //	SPI_ConfigureNPCS(spi, ch, SPI_MODE0 | SPI_CSR_CSAAT | SPI_CSR_SCBR(divider[ch]) | SPI_CSR_DLYBCT(1));
  uint32_t scbr = (F_CPU + 1000000L) / spiClock;
  SPI0->SPI_CSR[ch] = 0 
    | SPI_CSR_CPOL&0      // Clock Polarity (0=low)
    | SPI_CSR_NCPHA       // Clock Phase 
    | SPI_CSR_CSNAAT&0    // Chip Select Not Active After Transfer (Ignored if CSAAT = 1) */
    | SPI_CSR_CSAAT       // 1=Chip Select Active After LAST Transfer 
    | SPI_CSR_BITS_8_BIT  // 8 bits for transfer 
    | SPI_CSR_SCBR(scbr)  // Serial Clock Baud Rate 
    | SPI_CSR_DLYBS(0)    // Delay Before SPCK
    | SPI_CSR_DLYBCT(7)   // Delay Between Consecutive Transfers 
    ;
 
  
  // SPI bus is run at 2MHz.  Although the LPD8806 should, in theory,
  // work up to 20MHz, the unshielded wiring from the Arduino is more
  // susceptible to interference.  Experiment and see what you get.
  // SPI.setClockDivider ((F_CPU + 1000000L) / spiClock);  // HBA: spiClock (in Hz) added
#else
  //pinMode(CS,OUTPUT);
	//digitalWrite(CS,HIGH);
	spiBegin();
	spiInit((F_CPU + 1000000L) / spiClock);

#endif  
}

// Enable USART in SPI mode
//----------------------------------------------------------------------------
   void LPD8806::startUSART(void) {
//----------------------------------------------------------------------------
  pmc_enable_periph_clk(ID_PIOA);
  pmc_enable_periph_clk(ID_PIOB);
  PIO_SetPeripheral(PIOA, PIO_PERIPH_A, 1<<11);  // USART0 Data
  PIO_SetPeripheral(PIOA, PIO_PERIPH_B, 1<<17);  // USART0 Clock
  PIO_SetPeripheral(PIOA, PIO_PERIPH_A, 1<<13);  // USART1 Data
  PIO_SetPeripheral(PIOA, PIO_PERIPH_A, 1<<16);  // USART1 Clock
  pmc_enable_periph_clk(ID_USART0);
  pmc_enable_periph_clk(ID_USART1);
  // configure USART0: SPI master mode, 1 MHz clock
  USART_SetTransmitterEnabled(USART0, 0);
  USART_SetTransmitterEnabled(USART1, 0);
  USART_SetReceiverEnabled(USART0, 0);
  USART_SetReceiverEnabled(USART1, 0);
  USART_Configure(USART0, US_MR_USART_MODE_SPI_MASTER | US_MR_USCLKS_MCK | US_MR_CHRL_8_BIT | US_MR_CLKO, spiClock, 84000000);
  USART_Configure(USART1, US_MR_USART_MODE_SPI_MASTER | US_MR_USCLKS_MCK | US_MR_CHRL_8_BIT | US_MR_CLKO, spiClock, 84000000);
  USART_SetTransmitterEnabled(USART0, 1);  
  USART_SetTransmitterEnabled(USART1, 1);
  
  // enable DMAC  
  pmc_enable_periph_clk(ID_DMAC);
  dmac_disable();
  DMAC->DMAC_GCFG = DMAC_GCFG_ARB_CFG_FIXED;
  dmac_enable();

}

// Enable software SPI pins and issue initial latch:
//------------------------------------------------------------------------------
  void LPD8806::startBitbang() {
//------------------------------------------------------------------------------
  pinMode(datapin, OUTPUT);
  pinMode(clkpin , OUTPUT);
  //digitalWrite(datapin, LOW);
  //for(uint16_t i=((numLEDs+31)/32)*8; i>0; i--) {
  //  digitalWrite(clkpin, HIGH);
  //  digitalWrite(clkpin, LOW);
  //}
}

// Change strip length (see notes with empty constructor, above):
//------------------------------------------------------------------------------
  void LPD8806::updateLength(uint16_t n) {
//------------------------------------------------------------------------------
  uint8_t  latchBytes;
  uint16_t dataBytes, totalBytes;

  numLEDs = numBytes = 0;
  if(pixels) free(pixels); // Free existing data (if any)

  dataBytes  = n * 3;
  latchBytes = (n + 31) / 32;
  totalBytes = dataBytes + latchBytes;
  if((pixels = (uint8_t *)malloc(totalBytes))) { // Alloc new data
    numLEDs  = n;
    numBytes = totalBytes;
    //memset( pixels           , 0x80, dataBytes);  // Init to RGB 'off' state
	  memset( pixels           , 0xFF, dataBytes);  // HBA: Init to RGB 'ON' state
    memset(&pixels[dataBytes], 0   , latchBytes); // Clear latch bytes
  }
  // 'begun' state does not change -- pins retain prior modes
}

//------------------------------------------------------------------------------
  uint16_t LPD8806::numPixels(void) {
//------------------------------------------------------------------------------
  return numLEDs;
}

//------------------------------------------------------------------------------
  void LPD8806::show(void) {
//------------------------------------------------------------------------------
  uint8_t  *ptr = pixels;
  uint16_t i    = numBytes;

  // This doesn't need to distinguish among individual pixel color
  // bytes vs. latch data, etc.  Everything is laid out in one big
  // flat buffer and issued the same regardless of purpose.
  if(spiSelect!=SPI_SW) {
    spi_out_buf(ptr, i);
  } else {
    uint8_t p, bit;

    while(i--) {
      p = *ptr++;
      for(bit=0x80; bit; bit >>= 1) {
	  if(p & bit) digitalWrite(datapin, HIGH);
	  else        digitalWrite(datapin, LOW);
	  digitalWrite(clkpin, HIGH);
	  digitalWrite(clkpin, LOW);
      }
    }
  }
}

//----------------------------------------------------------------------------
    void showAll(LPD8806 &stripA, LPD8806 &stripB, LPD8806 &stripC) {
//----------------------------------------------------------------------------
  uint8_t  *ptrA = stripA.pixels;   // must be USART0
  uint8_t  *ptrB = stripB.pixels;   // must be USART1
  uint8_t  *ptrC = stripC.pixels;   // must be SPI
  //int i = stripA.numBytes;     // all 
  
  // This doesn't need to distinguish among individual pixel color
  // bytes vs. latch data, etc.  Everything is laid out in one big
  // flat buffer and issued the same regardless of purpose.
     //uint32_t mask = SPI_PCS(BOARD_PIN_TO_SPI_CHANNEL(BOARD_SPI_DEFAULT_SS)); 
     //uint32_t d = *ptrA++ | SPI_PCS(ch);

  // all three DMA transfers in parallel - don't wait for completion
  usartSend(USART0, ptrA, stripA.numBytes, false);
  usartSend(USART1, ptrB, stripB.numBytes, false);
  spiSend(ptrC, stripC.numBytes, false);
}

// wait until all DMA transfers are completed
//----------------------------------------------------------------------------
    void waitShowAllReady(void) {
//----------------------------------------------------------------------------
  while (!dmac_channel_transfer_done(SPI_DMAC_TX_CH))
    ;
  while (!dmac_channel_transfer_done(USART0_DMAC_TX_CH))
    ;
  while (!dmac_channel_transfer_done(USART1_DMAC_TX_CH))
    ;
}


// Convert separate R,G,B into combined 32-bit GRB color:
//------------------------------------------------------------------------------
  uint32_t LPD8806::Color(byte r, byte g, byte b) {
//------------------------------------------------------------------------------
  return ((uint32_t)(g | 0x80) << 16) |
         ((uint32_t)(r | 0x80) <<  8) |
                     b | 0x80 ;
}

// Set pixel color from separate 7-bit R, G, B components:
//------------------------------------------------------------------------------
  void LPD8806::setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
//------------------------------------------------------------------------------
  if(n < numLEDs) { // Arrays are 0-indexed, thus NOT '<='
    uint8_t *p = &pixels[n * 3];
    *p++ = g | 0x80; // Strip color order is GRB,
    *p++ = r | 0x80; // not the more common RGB,
    *p++ = b | 0x80; // so the order here is intentional; don't "fix"
  }
}

// Set pixel color from 'packed' 32-bit GRB (not RGB) value:
//------------------------------------------------------------------------------
  void LPD8806::setPixelColor(uint16_t n, uint32_t c) {
//------------------------------------------------------------------------------
  if(n < numLEDs) { // Arrays are 0-indexed, thus NOT '<='
    uint8_t *p = &pixels[n * 3];
    *p++ = (c >> 16) | 0x80;
    *p++ = (c >>  8) | 0x80;
    *p++ =  c        | 0x80;
  }
}

// Query color from previously-set pixel (returns packed 32-bit GRB value)
//------------------------------------------------------------------------------
  uint32_t LPD8806::getPixelColor(uint16_t n) {
//------------------------------------------------------------------------------
  if(n < numLEDs) {
    uint16_t ofs = n * 3;
    return ((uint32_t)(pixels[ofs    ] & 0x7f) << 16) |
           ((uint32_t)(pixels[ofs + 1] & 0x7f) <<  8) |
            (uint32_t)(pixels[ofs + 2] & 0x7f);
  }

  return 0; // Pixel # is out of bounds
}















/** Disable DMA Controller. */
//------------------------------------------------------------------------------
  static void dmac_disable() {
//------------------------------------------------------------------------------
  DMAC->DMAC_EN &= (~DMAC_EN_ENABLE);
}

/* Enable DMA Controller. */
//------------------------------------------------------------------------------
  static void dmac_enable() {
//------------------------------------------------------------------------------
  DMAC->DMAC_EN = DMAC_EN_ENABLE;
}

/** Disable DMA Channel. */
//------------------------------------------------------------------------------
  static void dmac_channel_disable(uint32_t ul_num) {
//------------------------------------------------------------------------------
  DMAC->DMAC_CHDR = DMAC_CHDR_DIS0 << ul_num;
}

/** Enable DMA Channel. */
//------------------------------------------------------------------------------
  static void dmac_channel_enable(uint32_t ul_num) {
//------------------------------------------------------------------------------
  DMAC->DMAC_CHER = DMAC_CHER_ENA0 << ul_num;
}
/** Poll for transfer complete. */
//------------------------------------------------------------------------------
  static bool dmac_channel_transfer_done(uint32_t ul_num) {
//------------------------------------------------------------------------------
  return (DMAC->DMAC_CHSR & (DMAC_CHSR_ENA0 << ul_num)) ? false : true;
}

//------------------------------------------------------------------------------
  static void spiBegin() {
//------------------------------------------------------------------------------
  PIO_Configure(
      g_APinDescription[PIN_SPI_MOSI].pPort,
      g_APinDescription[PIN_SPI_MOSI].ulPinType,
      g_APinDescription[PIN_SPI_MOSI].ulPin,
      g_APinDescription[PIN_SPI_MOSI].ulPinConfiguration);
  PIO_Configure(
      g_APinDescription[PIN_SPI_MISO].pPort,
      g_APinDescription[PIN_SPI_MISO].ulPinType,
      g_APinDescription[PIN_SPI_MISO].ulPin,
      g_APinDescription[PIN_SPI_MISO].ulPinConfiguration);
  PIO_Configure(
      g_APinDescription[PIN_SPI_SCK].pPort,
      g_APinDescription[PIN_SPI_SCK].ulPinType,
      g_APinDescription[PIN_SPI_SCK].ulPin,
      g_APinDescription[PIN_SPI_SCK].ulPinConfiguration);
  pmc_enable_periph_clk(ID_SPI0);
#if USE_SAM3X_DMAC
  pmc_enable_periph_clk(ID_DMAC);
  dmac_disable();
  DMAC->DMAC_GCFG = DMAC_GCFG_ARB_CFG_FIXED;
  dmac_enable();
#if USE_SAM3X_BUS_MATRIX_FIX
  MATRIX->MATRIX_WPMR = 0x4d415400;
  MATRIX->MATRIX_MCFG[1] = 1;
  MATRIX->MATRIX_MCFG[2] = 1;
  MATRIX->MATRIX_SCFG[0] = 0x01000010;
  MATRIX->MATRIX_SCFG[1] = 0x01000010;
  MATRIX->MATRIX_SCFG[7] = 0x01000010;
#endif  // USE_SAM3X_BUS_MATRIX_FIX
#endif  // USE_SAM3X_DMAC
}


//  initialize SPI controller
//------------------------------------------------------------------------------
  static void spiInit(uint8_t spiRate) {
//------------------------------------------------------------------------------
  Spi* pSpi = SPI0;
  uint8_t scbr = 255;
  if (spiRate < 14) {
    scbr = (2 | (spiRate & 1)) << (spiRate/2);
  }
  scbr = spiRate;  //thd
  //  disable SPI
  pSpi->SPI_CR = SPI_CR_SPIDIS;
  // reset SPI
  pSpi->SPI_CR = SPI_CR_SWRST;
  // no mode fault detection, set master mode
  pSpi->SPI_MR = SPI_PCS(SPI_CHIP_SEL) | SPI_MR_MODFDIS | SPI_MR_MSTR;
  // mode 0, 8-bit,
  pSpi->SPI_CSR[SPI_CHIP_SEL] = SPI_CSR_SCBR(scbr) | SPI_CSR_NCPHA;
  // enable SPI
  pSpi->SPI_CR |= SPI_CR_SPIEN;
}

#if 0
//------------------------------------------------------------------------------
  static void spiSend(uint8_t b) {
//------------------------------------------------------------------------------
  Spi* pSpi = SPI0;

  while ((SPI0->SPI_SR & SPI_SR_TDRE)==0)
    ;
  pSpi->SPI_TDR = b;
}
#endif

//------------------------------------------------------------------------------
  static void spiSend(const uint8_t* src, size_t count, bool wait) {
//------------------------------------------------------------------------------
  //Spi* pSpi = SPI0;

  // configure SPI TX DMA
  static uint8_t ff = 0XFF;
  uint32_t src_incr = DMAC_CTRLB_SRC_INCR_INCREMENTING;
  if (!src) {
    src = &ff;
    src_incr = DMAC_CTRLB_SRC_INCR_FIXED;
  }
  dmac_channel_disable(SPI_DMAC_TX_CH);
  DMAC->DMAC_CH_NUM[SPI_DMAC_TX_CH].DMAC_SADDR = (uint32_t)src;
  DMAC->DMAC_CH_NUM[SPI_DMAC_TX_CH].DMAC_DADDR = (uint32_t)&SPI0->SPI_TDR;
  DMAC->DMAC_CH_NUM[SPI_DMAC_TX_CH].DMAC_DSCR =  0;
  DMAC->DMAC_CH_NUM[SPI_DMAC_TX_CH].DMAC_CTRLA = count |
    DMAC_CTRLA_SRC_WIDTH_BYTE | DMAC_CTRLA_DST_WIDTH_BYTE;

  DMAC->DMAC_CH_NUM[SPI_DMAC_TX_CH].DMAC_CTRLB =  DMAC_CTRLB_SRC_DSCR |
    DMAC_CTRLB_DST_DSCR | DMAC_CTRLB_FC_MEM2PER_DMA_FC |
    src_incr | DMAC_CTRLB_DST_INCR_FIXED;

  DMAC->DMAC_CH_NUM[SPI_DMAC_TX_CH].DMAC_CFG = DMAC_CFG_DST_PER(SPI_TX_IDX) |
      DMAC_CFG_DST_H2SEL | DMAC_CFG_SOD | DMAC_CFG_FIFOCFG_ALAP_CFG;

  dmac_channel_enable(SPI_DMAC_TX_CH);

  if (wait) {
    while (!dmac_channel_transfer_done(SPI_DMAC_TX_CH))
       ;
  }
  
  //while ((pSpi->SPI_SR & SPI_SR_TXEMPTY) == 0) {}
  // leave RDR empty
  //uint8_t b = pSpi->SPI_RDR;
}

#if 0
//------------------------------------------------------------------------------
  static void usartSend(Usart *pU, uint8_t b) {
//------------------------------------------------------------------------------
  while ((pU->US_CSR & US_CSR_TXRDY)==0)
    ;
  pU->US_THR = b;
}
#endif

//------------------------------------------------------------------------------
  static void usartSend(Usart *pU, const uint8_t* src, size_t count, bool wait) {
//------------------------------------------------------------------------------
  // configure USART TX DMA
  static uint8_t ff = 0XFF;
  uint32_t src_incr = DMAC_CTRLB_SRC_INCR_INCREMENTING;
  uint32_t usart_dmac_ch = pU==USART0 ? USART0_DMAC_TX_CH : USART1_DMAC_TX_CH; 
  uint32_t usart_tx_idx  = pU==USART0 ? USART0_TX_IDX     : USART1_TX_IDX; 
  
  if (!src) {
    src = &ff;
    src_incr = DMAC_CTRLB_SRC_INCR_FIXED;
  }
  
  dmac_channel_disable(usart_dmac_ch);
  DMAC->DMAC_CH_NUM[usart_dmac_ch].DMAC_SADDR = (uint32_t)src;
  DMAC->DMAC_CH_NUM[usart_dmac_ch].DMAC_DADDR = (uint32_t)&pU->US_THR;
  DMAC->DMAC_CH_NUM[usart_dmac_ch].DMAC_DSCR =  0;
  DMAC->DMAC_CH_NUM[usart_dmac_ch].DMAC_CTRLA = count |
    DMAC_CTRLA_SRC_WIDTH_BYTE | DMAC_CTRLA_DST_WIDTH_BYTE;

  DMAC->DMAC_CH_NUM[usart_dmac_ch].DMAC_CTRLB =  DMAC_CTRLB_SRC_DSCR |
    DMAC_CTRLB_DST_DSCR | DMAC_CTRLB_FC_MEM2PER_DMA_FC |
    src_incr | DMAC_CTRLB_DST_INCR_FIXED;

  DMAC->DMAC_CH_NUM[usart_dmac_ch].DMAC_CFG = DMAC_CFG_DST_PER(usart_tx_idx) |
      DMAC_CFG_DST_H2SEL | DMAC_CFG_SOD | DMAC_CFG_FIFOCFG_ALAP_CFG;

  dmac_channel_enable(usart_dmac_ch);

  if (wait) {
    while (!dmac_channel_transfer_done(usart_dmac_ch))
      ;
  }
  //while ((pUsart->SPI_SR & SPI_SR_TXEMPTY) == 0) {}
  // leave RDR empty
  //uint8_t b = pSpi->SPI_RDR;
}

