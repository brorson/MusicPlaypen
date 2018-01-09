#ifndef SPIDRIVER_HOST_H
#define SPIDRIVER_HOST_H

// Global pointer to base of PRU0 RAM.  This is the place
// where commands and data are communicated between the host
// ARM processor and the PRU.  The memory buffer is defined in
// the linker command file AM335x_PRU.cmd.
#pragma DATA_SECTION(pru0_dataram, ".data_buf")
#pragma RETAIN(pru0_dataram)
static uint32_t *pru0_dataram;

// Global pointer to base of PRU1 RAM.  Must fix this later.
//#pragma DATA_SECTION(pru1_dataram, ".data_buf1")
//#pragma RETAIN(pru1_dataram)
//static uint32_t *pru1_dataram;

uint8_t pruss_init(void);
uint8_t pru0_init(void);
uint8_t pru1_init(void);
uint8_t pru_reset(int prunum);

void spi_reset_cmd(void);

// Low level fcns for internal use.
uint32_t pru_read_word(uint32_t offset);
void pru_write_word(uint32_t offset, uint32_t value);

// Debug and diagnostic stuff.
uint32_t pru_test_ram(uint32_t offset, uint32_t value);
uint32_t pru_test_communication(void);

// High level fcns.  Call these from external files.
uint32_t spi_write_cmd(uint32_t *data, int byte_cnt);
uint8_t spi_writeread_single(uint32_t *txdata, int txcnt, uint32_t *rxdata, int rxcnt);
uint8_t spi_writeread_continuous(uint32_t *txdata, int txcnt, uint32_t *rxdata, int rxcnt, int ncnv);

#endif

