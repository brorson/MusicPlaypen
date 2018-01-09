#ifndef ADCDRIVER_HOST_H
#define ADCDRIVER_HOST_H

// Allowed sample rates.  These are set by the AD7172 hardware.
// Consult the AD7172 datasheet for more info.  
#define SAMP_RATE_31250 5
#define SAMP_RATE_15625 6
#define SAMP_RATE_10417 7
#define SAMP_RATE_5208  8
#define SAMP_RATE_2604  9
#define SAMP_RATE_1008  10
#define SAMP_RATE_504   11
#define SAMP_RATE_400P6 12
#define SAMP_RATE_200P3 13
#define SAMP_RATE_100P2 14
#define SAMP_RATE_59P98 15
#define SAMP_RATE_50    16


// High level fcns which abstract away the need to know much about
// interfacing to the A/D.
float adc_GetVoltage(uint32_t buf);
void adc_config(void);
uint32_t adc_get_id_reg(void);
void adc_quit(void);
void adc_reset(void);
void adc_set_samplerate(int rate);
void adc_set_chan0(void);
void adc_set_chan1(void);


// Data acquisition fcns.
float adc_read_single(void);
void adc_read_multiple(uint32_t read_cnt, float *volts);

//--------------------------------------------------
// Low level fcns
void adc_write(uint32_t *tx_buf, int byte_cnt);
void adc_writeread(uint32_t *tx_buf, int tx_cnt, uint32_t *rx_buf, int rx_cnt);

#endif

