#ifndef SPI_PRU_H
#define SPI_PRU_H

#include <stdint.h>

// This is offset of data buffer from base RAM
// Adjust this to move the physical location of the
// communication RAM around.
#define RAMOFFSET 0x80


// The flags sent have the following meaning:
// 0x00 -- no command
// 0x01 -- SPI write
// 0x02 -- SPI writeread
// 0x03 -- SPI reset
enum {
  NOP,
  SPI_TEST,
  SPI_WRITE,
  SPI_WRITEREAD_SINGLE,
  SPI_WRITEREAD_CONTINUOUS,
  SPI_RESET,
  SPI_WAIT_COMMAND = 0xff,
};


void pru_spi_config0(void);
void pru_spi_reset(void);
void pru_spi_write(volatile uint32_t *pData, volatile int byte_cnt); 
uint8_t pru_spi_writeread_single(volatile uint32_t *pTxbuf, volatile int tx_cnt, uint32_t *pRxbuf, volatile int rx_cnt);
uint8_t pru_spi_writeread_continuous(uint32_t *pTxbuf, int tx_cnt, volatile uint32_t *pRxbuf, int rx_cnt, int ncnv);

#endif

