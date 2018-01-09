//----------------------------------------------------------------------
// spi_pru -- provides commands for spi transactions.
// This code is meant to run on the PRU.
// This is the bit-banging stuff which performs the SPI
// bus communication.  It is meant to be called from a
// higher-level program.
//-----------------------------------------------------------------------
#include "pru_spi.h"

volatile register uint32_t __R30;  // write reg
volatile register uint32_t __R31;  // read reg

// This defines the positions of the SPI signals
// in the IO registers R30 and R31.  The mapping
// of the reg bits to external pins on the device
// is done in the device tree overlay file.
#define CS 3     /* pr1_pru0_pru_r30_3 */
#define CLK 5    /* pr1_pru0_pru_r30_5 */
#define MOSI 1   /* pr1_pru0_pru_r30_1 */
#define MISO 2   /* pr1_pru0_pru_r31_2 */

// This defines the delay betwen transitions in the data bits.
#define DELAY_CNT 20

//================================================================
// Local fcns

//---------------------------------------------------------------
void wait_miso_high() {
  uint8_t rx_bit;

  while(1) {
    rx_bit = (__R31>>MISO) & 0x01;
    if (rx_bit) {
      break;
    } 
  } 
}

//---------------------------------------------------------------
void wait_miso_low() {
  uint8_t rx_bit;  

  while(1) {
    rx_bit = (__R31>>MISO) & 0x01;
    if (!rx_bit) {
      break;
    } 
  } 
}

//================================================================
// Exported fcns.

//-----------------------------------------------------------------
void pru_spi_reset(void) {
  // To reset the A/D, send SCK with MOSI high.  
  // Send at least 64 clocks with MOSI high.  I
  // send 72 clocks for good measure, but probably
  // overkill.
  int i, j;
  uint8_t byte_cnt = 9;

  // Set MOSI high
  __R30 = __R30 | (1 << MOSI); // Transmit 1 bit

  // Assert CS down
  __R30 = __R30 & ~(1 << CS);

  // Delay before sending bytes
  __delay_cycles(DELAY_CNT);

  // Loop over data bytes
  for (i=0; i<byte_cnt; i++) {

    // Loop over bits
    for (j=0; j<8; j++) {

      // set clk down
      __R30 = __R30 & ~(1 << CLK); // Turn off clk

      // Continue sending 1 bit.
      __R30 = __R30 | (1 << MOSI); // Transmit 1 bit

      // Delay
      __delay_cycles(DELAY_CNT);

      // Now set clk up
      __R30 = __R30 | (1 << CLK); // Turn on clk

      // Delay
      __delay_cycles(DELAY_CNT);

    } // loop over bits

    // Quick delay before next byte
    __delay_cycles(DELAY_CNT);

  } // loop over bytes

  // Bring CS back up at end of transaction
  __R30 = __R30 | (1 << CS);

  // Set MOSI low
  __R30 = __R30 & ~(1 << MOSI); // Transmit 0 bit

}


//-----------------------------------------------------------------
void pru_spi_write(volatile uint32_t *pData, volatile int byte_cnt) {
  // This performs a SPI write operation.
  int i, j;
  uint8_t data_byte;

  // First assert CS down
  __R30 = __R30 & ~(1 << CS);

  // Delay before transmitting bytes
  __delay_cycles(DELAY_CNT);

  // Loop over data words *pData
  for (i=0; i<byte_cnt; i++) {
    data_byte = (uint8_t) *(pData+i);

    // Loop over bits
    for (j=0; j<8; j++) {

      // set clk down
      __R30 = __R30 & ~(1 << CLK); // Turn off clk

      // Now set bit
      if ( (data_byte<<j) & 0x80 ) {
        // 1 bit
        __R30 = __R30 | (1 << MOSI); // Turn on data bit
      } else { 
        // 0 bit
        __R30 = __R30 & ~(1 << MOSI); // Turn off data bit
      }

      // Delay to let bit settle
      __delay_cycles(DELAY_CNT);

      // Now set clk up
      __R30 = __R30 | (1 << CLK); // Turn on clk

      // Delay to let bit latch in
      __delay_cycles(DELAY_CNT);

    } // loop over bits

    // Quick delay before next byte
    __delay_cycles(DELAY_CNT);

  } // loop over bytes

  // Bring CS back up at end of transaction
  __R30 = __R30 | (1 << CS);

  // Set MOSI low when not transmitting data
  __R30 = __R30 & ~(1 << MOSI); // Transmit 0 bit

  return;

}



//---------------------------------------------------------------
uint8_t pru_spi_writeread_single(volatile uint32_t *pTxbuf, volatile int tx_cnt, uint32_t *pRxbuf, volatile int rx_cnt) {
  // This first waits for MISO to go high, then low.  Once MISO is low, this fcn
  // performs a write, then keeps clocking in order to read and
  // store the input data.  It does not raise CS between the write and the read.
  int i, j;
  uint8_t data_byte;
  uint8_t rx_bit, rx_byte;
  uint8_t rx_word[4];   // Max wordsize is 32 bits.
  uint32_t tmp;

  // Set MOSI high
  __R30 = __R30 | (1 << MOSI); // Turn on data bit

  // First assert CS down
  __R30 = __R30 & ~(1 << CS);

  // set clk down
  __R30 = __R30 & ~(1 << CLK); // Turn off clk

  // Now wait for MISO to go high
  // wait_miso_high();

  // ---->   Clock out Tx command from MOSI
  // Loop over data words *pTxbuf
  for (i=0; i<tx_cnt; i++) {
    data_byte = (uint8_t) *(pTxbuf+i);

    // Loop over bits in byte
    for (j=0; j<8; j++) {

      // set clk down
      __R30 = __R30 & ~(1 << CLK); // Turn off clk

      // Now set bit
      if ( (data_byte<<j) & 0x80 ) {
        // 1 bit
        __R30 = __R30 | (1 << MOSI); // Turn on data bit
      } else {  
        // 0 bit
        __R30 = __R30 & ~(1 << MOSI); // Turn off data bit
      } 
      
      // Delay to let bit settle
      __delay_cycles(DELAY_CNT);     
 
      // Now set clk up
      __R30 = __R30 | (1 << CLK); // Turn on clk
      
      // Delay to let bit latch in
      __delay_cycles(DELAY_CNT);

    } // loop over bits
    
    // Quick delay before next byte
    __delay_cycles(DELAY_CNT);

  } // loop over bytes

  // Set MOSI low while clocking in reply
  __R30 = __R30 & ~(1 << MOSI); // Transmit 0 bit


  // ---->    Now read incoming bytes from MISO
  // Loop over data bytes
  for (i=0; i<rx_cnt; i++) {
    rx_bit = 0x00;

    // Loop over bits
    for (j=0; j<8; j++) {

      // set clk down
      __R30 = __R30 & ~(1 << CLK); // Turn off clk

      // Delay to let things settle
      __delay_cycles(DELAY_CNT);
 
      // Now set clk up and read incoming MISO bit
      __R30 = __R30 | (1 << CLK); // Turn on clk

      rx_bit = (__R31>>MISO) & 0x01;
      rx_byte = (rx_byte<<1) | rx_bit;      

      // Now wait for next 1/2 period.
      __delay_cycles(DELAY_CNT);

    } // loop over bits

    // Must do some conversion since Rxbuf is a uint32_t word.
    rx_word[i] = rx_byte;

    // Quick delay before next byte
    __delay_cycles(DELAY_CNT);

  }

  // Now convert rx_word to Rxbuf
  tmp = 0x00;
  for (i=0; i<rx_cnt; i++) {
    tmp = ((tmp<<8) | rx_word[i]);  // Shift in bytes
  }
  pRxbuf[0] = tmp;


  // Bring CS back up at end of transaction
  __R30 = __R30 | (1 << CS);

  return 0x00;

}


//---------------------------------------------------------------
uint8_t pru_spi_writeread_continuous(uint32_t *pTxbuf, int tx_cnt, volatile uint32_t *pRxbuf, int rx_cnt, int ncnv) {
  // This first waits for MISO to go high, then low.  Once MISO is low, this fcn
  // performs a write, then keeps clocking in order to read and
  // store the input data.  It does not raise CS until it has read all
  // bytes requested.

  // Inputs:
  // pTxbuf = pointer to input buffer (cmd bytes)
  // tx_cnt = number of bytes in cmd
  // pRxbuf = pointer to output buffer (A/D code bytes)
  //          This buffer should be rx_cnt*ncnv bytes long.
  // rx_cnt = number of bytes per A/D reading.  Usually 3.
  // ncnv = total number of A/D readings to make.

  int ccnt;
  int i, j;
  uint8_t data_byte;
  uint8_t rx_bit, rx_byte;
  uint8_t rx_word[4];   // Max wordsize is 32 bits.
  uint32_t tmp;

  // Set MOSI high
  __R30 = __R30 | (1 << MOSI); // Turn on data bit

  // Next assert CS down
  __R30 = __R30 & ~(1 << CS);

  //**************************
  // Now we enter big loop over A/D readings.
  for (ccnt=0; ccnt<ncnv; ccnt++) {

    wait_miso_high();
    wait_miso_low();

    // ---->   Clock out Tx command from MOSI
    // Loop over data words *pTxbuf
    for (i=0; i<tx_cnt; i++) {
      data_byte = (uint8_t) *(pTxbuf+i);

      // Loop over bits
      for (j=0; j<8; j++) {

        // set clk down
        __R30 = __R30 & ~(1 << CLK); // Turn off clk

        // Now set bit
        if ( (data_byte<<j) & 0x80 ) {
          // 1 bit
          __R30 = __R30 | (1 << MOSI); // Turn on data bit
        } else {
          // 0 bit
          __R30 = __R30 & ~(1 << MOSI); // Turn off data bit
        }

        // Delay to let bit settle
        __delay_cycles(DELAY_CNT);

        // Now set clk up
        __R30 = __R30 | (1 << CLK); // Turn on clk

        // Delay to let bit latch in
        __delay_cycles(DELAY_CNT);

      } // loop over bits

      // Quick delay before next byte
      __delay_cycles(DELAY_CNT);

    } // loop over bytes

    // Set MOSI high while clocking in reply
    __R30 = __R30 | (1 << MOSI); // Transmit 1 bit
  
    // ---->    Now read incoming bytes from MISO
    // Loop over data bytes
    for (i=0; i<rx_cnt; i++) {
      rx_bit = 0x00;
     
      // Loop over bits
      for (j=0; j<8; j++) {
    
        // set clk down
        __R30 = __R30 & ~(1 << CLK); // Turn off clk
      
        // Delay to let things settle
        __delay_cycles(DELAY_CNT);
      
        // Now set clk up and read incoming MISO bit
        __R30 = __R30 | (1 << CLK); // Turn on clk
      
        rx_bit = (__R31>>MISO) & 0x01;
        rx_byte = (rx_byte<<1) | rx_bit;
      
        // Now wait for next 1/2 period.
        __delay_cycles(DELAY_CNT);
      
      } // loop over bits
    
     // Must do some conversion since Rxbuf is a uint32_t word.
     rx_word[i] = rx_byte;
   
      // Quick delay before next byte
      __delay_cycles(DELAY_CNT);
    }  // Loop over data bytes i

    // Now convert rx_word to Rxbuf
    tmp = 0x00;
    for (i=0; i<rx_cnt; i++) {
      tmp = ((tmp<<8) | rx_word[i]);  // Shift in bytes
    }
    pRxbuf[ccnt] = tmp;

    // Wait a little bit until next loop.    
    __delay_cycles(10*DELAY_CNT);

    //wait_miso_high();

  }  // for (ccnt=0; ccnt<ncnv; ccnt++) {
  //**************************

  // Bring CS back up at end of transaction
  __R30 = __R30 | (1 << CS);

  // Set MOSI low after transaction is over.
  __R30 = __R30 & ~(1 << MOSI); // Transmit 0 bit


  return 0x00;
}


