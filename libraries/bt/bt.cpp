#include <Arduino.h>
#include "bt.h"

// Initialization of USART3 for communication with BT module HC06
//---------------------------------------------------------------------------------------
void btInit(uint32_t baudrate)
//---------------------------------------------------------------------------------------
{
  const uint32_t masterClock = 84000000;

  pmc_enable_periph_clk(ID_PIOD);
  PIO_SetPeripheral(PIOD, PIO_PERIPH_B, 1 << 4); // TXD3
  PIO_SetPeripheral(PIOD, PIO_PERIPH_B, 1 << 5); // RXD3
  pmc_enable_periph_clk(ID_USART3);
  // configure USART0: SPI master mode, 1 MHz clock
  USART_SetTransmitterEnabled(USART3, 0);
  USART_SetReceiverEnabled(USART3, 0);

  USART3->US_CR = US_CR_RSTRX | US_CR_RSTTX | US_CR_RXDIS | US_CR_TXDIS;

  /* Configure mode*/
  USART3->US_MR = 0
                  | US_MR_USART_MODE_NORMAL   // normal asynchronous UART
                  | US_MR_USCLKS_MCK          // use master clock (84 MHz) for clock generation
                  | US_MR_CHRL_8_BIT          // 8 data bits
                  | US_MR_PAR_NO              // no parity
                  | US_MR_NBSTOP_1_BIT        // stop bit
                  | US_MR_CHMODE_NORMAL       // normal channe lmode (no loopback etc)
                  | US_MR_OVER;               // 16x oversampling

  /* Configure baudrate*/
  USART3->US_BRGR = (masterClock / baudrate) / 8;

  USART3->US_CR = US_CR_TXEN | US_CR_RXEN;
}


//---------------------------------------------------------------------------------------
void btWriteString(char const *textPtr)
//---------------------------------------------------------------------------------------
{
  while (*textPtr) {
    while ((USART3->US_CSR & US_CSR_TXEMPTY) == 0)
      ;
    USART3->US_THR = *textPtr++;
  }
}

//---------------------------------------------------------------------------------------
void btWriteChar(char ch)
//---------------------------------------------------------------------------------------
{
  while ((USART3->US_CSR & US_CSR_TXEMPTY) == 0)
    ;
  USART3->US_THR = ch;
}

//---------------------------------------------------------------------------------------
char btReadChar(void)
//---------------------------------------------------------------------------------------
{
  while ((USART3->US_CSR & US_CSR_RXRDY) == 0)
    ;
  return USART3->US_RHR;
}

//---------------------------------------------------------------------------------------
int btReadString(char *cp, int nmax)
//---------------------------------------------------------------------------------------
{
  char ch;
  int n = 1;

  while (n < nmax) {
    ch = btReadChar();
    btWriteChar(ch);
    if (ch == 13) break;
    *cp++ = ch;
    n++;
  }
  *cp = 0;
  return n;
}

//---------------------------------------------------------------------------------------
void btReadData(uint8_t *cp, int len)
//---------------------------------------------------------------------------------------
{
  while (len--) {
    *cp++ = btReadChar();
  }
}

//---------------------------------------------------------------------------------------
int btCharAvailable(void)
//---------------------------------------------------------------------------------------
{
  return USART3->US_CSR & US_CSR_RXRDY;
}

