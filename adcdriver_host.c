/*
 * adcdriver_host.c -- This is the C stuff running on the ARM which asks for
 * and receives SPI data from the AD7172.  It uses the SPI peripherial
 * running on PRU0 to communicate with the A/D, and runs the stable clock
 * on PRU1 to trigger conversions.
 * 
 * This stuff is meant to be an abstraction layer for the A/D.  It deals with
 * A/D specific stuff like the exact A/D commands, provides commands for
 * A/D write, single writeread, and streaming read.  Later it can also handle
 * the double-buffer scheme.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>

#include <prussdrv.h>
#include <pruss_intc_mapping.h>

#include "spidriver_host.h"
#include "adcdriver_host.h"

#define SPI_PRU	0
#define CLK_PRU 1

// Commands to AD7172
#define READ_ID_REG 0x47
#define READ_DATA_REG 0x44
#define READ_STATUS_REG 0x40
#define WRITE_CH0_REG 0x10
#define WRITE_CH1_REG 0x11
#define WRITE_SETUPCON0_REG 0x20
#define WRITE_ADCMODE_REG 0x01
#define WRITE_IFMODE_REG 0x02
#define WRITE_GPIOCON_REG 0x06
#define WRITE_FILTERCON0_REG 0x28

// Default values used in computing voltage from A/D code
#define OFFSET 0x800000
#define GAIN 0x555555
#define TWO_23 8388608.0f
#define VREF 4.096f


//================================================================
// Helper fcns

//---------------------------------------------------------
/**
 * Given A/D code, return voltage as float
 *
 * Note that A/D output is "bipolar offset binary"
 *
 */
float adc_GetVoltage(uint32_t buf) {
  int32_t uiCode=0x00;
  int32_t iTmp;
  float deltaV = VREF/TWO_23;
  float volts;

  // printf("---> Entering adc_GetVoltage, buf = 0x%08x\n", buf);

  uiCode = (int32_t) buf;
  iTmp = (uiCode) - 0x800000;     // Subtract offset
  volts = deltaV*((float) iTmp);  // Get voltage

  // printf("<--- Leaving adc_GetVoltage, volts = %f\n", volts);

  return volts;
}


//=================================================================
// High level fcns -- these wrap the A/D details completely.

//---------------------------------------------------
// Configure A/D to run in desired mode.
void adc_config(void) {
  uint32_t tx_buf[3];
  uint32_t retval;

  // printf("Entered adc_config.....\n");

  // Initialize PRUSS subsystem and PRU0
  pruss_init();
  pru0_init();
  pru1_init();
  usleep(1000);  // let PRU start functioning before doing anything

  // Send reset message using spi_write_cmd 
  // printf("Commanding A/D reset....\n");
  adc_reset();
  
  // Configure channel 0 to use +AIN0, -AIN1,
  // and also enable this channel.
  // This is default -- read data only on channel 0.
  // Reset val 0x8001
  // printf("WRITE_CH0_REG\n");
  tx_buf[0] = WRITE_CH0_REG;
  tx_buf[1] = 0x80; // 0x80;
  tx_buf[2] = 0x01; // 0x01;
  retval |= spi_write_cmd(tx_buf, 3);

  usleep(5);

  // Configure channel 1 to use +AIN2, -AIN3,
  // but don't enable this channel.  User must
  // enable manually.
  // Rest val 0x0001
  // printf("WRITE_CH1_REG\n");
  tx_buf[0] = WRITE_CH1_REG;
  tx_buf[1] = 0x00;
  tx_buf[2] = 0x43;
  retval |= spi_write_cmd(tx_buf, 3);

  usleep(5);

  // Set up config0 reg
  // Rest val 0x1000
  // printf("WRITE_SETUPCON0_REG\n");
  tx_buf[0] = WRITE_SETUPCON0_REG;
  tx_buf[1] = 0x13;
  tx_buf[2] = 0x00;
  retval |= spi_write_cmd(tx_buf, 3);

  usleep(5);

  // Set up ADC mode reg
  //printf("WRITE_ADCMODE_REG\n");
  tx_buf[0] = WRITE_ADCMODE_REG;
  tx_buf[1] = 0x00;
  tx_buf[2] = 0x0c;  
  //retval |= spi_write_cmd(tx_buf, 3);

  usleep(5);

  // Set up interface mode reg
  // Reset val 0x0000
  // printf("WRITE_IFMODE_REG\n");
  tx_buf[0] = WRITE_IFMODE_REG;
  tx_buf[1] = 0x00;
  tx_buf[2] = 0x00;
  retval |= spi_write_cmd(tx_buf, 3);

  usleep(5);

  // Set up gpio config reg
  // Reset val 0x0800
  // printf("WRITE_GPIOCON_REG\n");
  tx_buf[0] = WRITE_GPIOCON_REG;
  tx_buf[1] = 0x00;  // Turn off SYNC_N feature
  tx_buf[2] = 0x00;
  retval |= spi_write_cmd(tx_buf, 3);

  return;  // retval;
}


//----------------------------------------------
uint32_t adc_get_id_reg(void) {
  uint32_t tx_buf;
  uint32_t rx_buf[2];
  tx_buf = READ_ID_REG;
  spi_writeread_single(&tx_buf, 1, rx_buf, 2);
  return rx_buf[0];
}


//----------------------------------------------
void adc_quit(void) {
  pru_reset(SPI_PRU);
  prussdrv_exit();
}


//----------------------------------------------
void adc_reset(void) {
  spi_reset_cmd();
  usleep(1000);  // Must wait at least 0.5mS after reset.
}


//----------------------------------------------
void adc_set_chan0(void) {
  uint32_t tx_buf[3];

  // Disable chan1 reg.
  tx_buf[0] = WRITE_CH1_REG;
  tx_buf[1] = 0x00;
  tx_buf[2] = 0x43;
  spi_write_cmd(tx_buf, 3);

  // Now enable chan0 reg.
  tx_buf[0] = WRITE_CH0_REG;
  tx_buf[1] = 0x80; // 0x80;
  tx_buf[2] = 0x01; // 0x01;
  spi_write_cmd(tx_buf, 3);
}


//----------------------------------------------
void adc_set_chan1(void) {
  uint32_t tx_buf[3];
  
  // Disable chan0 reg.
  tx_buf[0] = WRITE_CH0_REG;
  tx_buf[1] = 0x00;
  tx_buf[2] = 0x01;
  spi_write_cmd(tx_buf, 3);
  
  // Now enable chan1 reg.
  tx_buf[0] = WRITE_CH1_REG;
  tx_buf[1] = 0x80; // 0x80;
  tx_buf[2] = 0x43; // 0x01;
  spi_write_cmd(tx_buf, 3);
} 


//----------------------------------------------
void adc_set_samplerate(int rate) {
  uint32_t tx_buf[3];

  if (rate > 0x16) {
    return;
  }

  // Set up FILTERCON0 reg
  //printf("WRITE_FILTERCON0_REG\n");
  tx_buf[0] = WRITE_FILTERCON0_REG;
  tx_buf[1] = 0x00;
  tx_buf[2] = 0x1f & rate;
  tx_buf[2] = tx_buf[2] | 0x60;
  spi_write_cmd(tx_buf, 3);
}



//============================================================================
// These fcns implement the different types of conversions supported by the
// AD7172.
//----------------------------------------------
float adc_read_single(void) {
  // This fcn implements a single conversion per the 7172 datasheet
  // p. 39. 
  // This fcn reads the diff between ch 1 & 0.  It uses the 
  // level on MISO to know when it is valid to read the value
  // in the data reg.
  uint32_t tx_buf[3];
  uint32_t rx_buf;
  float volts;

  // First set up A/D for single conversion mode by writing mode reg.
  //printf("WRITE_ADCMODE_REG\n");
  tx_buf[0] = WRITE_ADCMODE_REG;
  tx_buf[1] = 0x00;
  tx_buf[2] = 0x1c;
  spi_write_cmd(tx_buf, 3);


  // Now do read
  tx_buf[0] = READ_DATA_REG;
  spi_writeread_single(tx_buf, 1, &rx_buf, 3);

  // Now convert to float and return
  volts = adc_GetVoltage(rx_buf);
  return volts;
}


//---------------------------------------------
void adc_read_multiple(uint32_t read_cnt, float *volts) {
  // This fcn reads rx_cnt float values from the A/D in continous 
  //read mode and sticks them into
  // the buffer pointed to by rx_buf.
  uint32_t tx_buf[3];
  uint32_t rx_buf[3072];  // = 1024 words, wordlength = 3.
  int rx_byte_cnt;
  uint32_t val_buf[3];
  int i, j;

  // Sanity check read_cnt
  if (read_cnt >1024) {
    printf("User requested too much PRU RAM.  Exiting....\n");
    exit(-1);
  }

  // Set up ADC mode reg for continuous conversation
  //printf("WRITE_ADCMODE_REG\n");
  tx_buf[0] = WRITE_ADCMODE_REG;
  tx_buf[1] = 0x00;
  tx_buf[2] = 0x0c;
  spi_write_cmd(tx_buf, 3);

  tx_buf[0] = READ_DATA_REG;
  spi_writeread_continuous(tx_buf, 1, rx_buf, 3, read_cnt);  

  // Now convert readings to float and return
  for (i=0; i<read_cnt; i++) {
    volts[i] = adc_GetVoltage(rx_buf[i]);
  }
  return;
}


//============================================================================
// These are low-level fcns allowing the caller to send any command desired.
//----------------------------------------------
void adc_write(uint32_t *tx_buf, int byte_cnt) {
  spi_write_cmd(tx_buf, byte_cnt);
}


//----------------------------------------------
void adc_writeread_single(uint32_t *tx_buf, int tx_cnt, uint32_t *rx_buf, int rx_cnt) {
  spi_writeread_single(tx_buf, tx_cnt, rx_buf, rx_cnt);
}



