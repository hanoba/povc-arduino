#include <Arduino.h>

class LPD8806 {
  friend void showAll(LPD8806 &, LPD8806 &, LPD8806 &);


 public:
  LPD8806(uint16_t n, uint8_t dpin, uint8_t cpin); // Configurable pins
  LPD8806(uint16_t n, Usart *p, int32_t clk); // Use USART in SPI mode; specific pins only
  LPD8806(uint16_t n, int32_t clk); // Use SPI hardware; specific pins only 
  LPD8806(void);       // Empty constructor; init pins & strip length later
  void begin(void);
  void setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b);
  void setPixelColor(uint16_t n, uint32_t c);
  void show(void);
  void updatePins(uint8_t dpin, uint8_t cpin); // Change pins, configurable
  void updatePins(Usart *p);                   // Change pins, USART in SPI mode
  //void updatePins(void);                       // Change pins, hardware SPI
  void updateLength(uint16_t n);               // Change strip length
  uint16_t numPixels(void);
  uint32_t Color(byte, byte, byte);
  uint32_t getPixelColor(uint16_t n);

 private:
  Usart *pUsart; // USART to be used as SPI  
  uint32_t spiClock;   // SPI clock in Hz
  uint16_t numLEDs;    // Number of RGB LEDs in strip
  uint16_t numBytes;   // Size of 'pixels' buffer below
  uint8_t *pixels;     // Holds LED color values (3 bytes each) + latch bytes
  uint8_t clkpin; 
  uint8_t datapin;      // Clock & data pin numbers
  void startBitbang(void);
  void spi_out(uint8_t n);
  void spi_out_buf(uint8_t *ptr, size_t len);
  void startUSART(void);
  void startSPI(void);
  uint8_t spiTransfer(uint8_t b);
  
  //boolean begun; // If 'true', begin() method was previously invoked
  enum { SPI_SW, SPI_SPI, SPI_USART, SPI_ALL } spiSelect;  
};

void waitShowAllReady(void);
