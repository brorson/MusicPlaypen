/*
 * This program is the upper-layer wrapper around the SPI stuff.
 * This program runs on the PRU, and deals with communication with
 * the ARM processor.  This program sits in a loop looking at the
 * shared memory area.  When a command comes in, it dispatches to
 * the appropriate function in spi_pru.c.
 *
 */
#include <stdint.h>
#include "pru_cfg.h"
#include "pru_intc.h"
#include "pru_ctrl.h"
#include "resource_table_empty.h"

#include "pru_spi.h"

volatile register uint32_t __R30;
volatile register uint32_t __R31;

/* This is memory for commands from host.  This is the PRU's DRAM.  */
volatile far uint32_t MEM_BASE __attribute__((cregister("PRU_COMM_RAM0", near), peripheral));
//PRU_COMM_RAM
//PRU_DMEM_0_1

/* 
Command memory layout is as follows:
uint32_t flag
uint32_t byte_count
uint32_t data_bytes[4]
*/


/* PRU-to-ARM interrupt */
#define PRU0_ARM_INTERRUPT (19+16)

#define CS 3     /* pr1_pru0_pru_r30_3 P9_28 */
#define CLK 5    /* pr1_pru0_pru_r30_5 P9_27 */
#define MOSI 1   /* pr1_pru0_pru_r30_1 P9_29 */
#define MISO 2   /* pr1_pru0_pru_r31_2 P9_30 */

#define DELAY_CNT 30

    
//------------------------------------------------------------------------
int main(void) {
  // This program works as follows:
  // Runs loop, looking if flag is set.
  // When flag is set, then execute code which emits
  // the corresponding data via SPI.  The SPI is bit-banged.
  // Then clear flag and return to loop.

  // Currently I copy the data between the shared RAM and a private buffer living
  // on the PRU.  Later I should just use the data directly out of the shared
  // RAM.

  // Map MEM into my address space.
  //volatile uint32_t *pMEM;
  //pMEM = &MEM_BASE;

  // Note that the pruss subsystem wants unsigned int types.
  uint32_t flag;
  uint32_t tx_word_cnt;
  uint32_t tx_words[4];
  uint32_t rx_word_cnt;
  uint32_t rx_words[4];
  uint32_t ncnv;
  uint32_t i;
  uint32_t memptr, rxmemptr;

  // Map MEM into my address space.
  // Note RAMOFFSET is defined in pru_spi.h.  It is the amount I offset the 
  // communication memory to get away from the memory error at addr 56.
  volatile uint32_t *pMEM;
  pMEM = (&MEM_BASE)+RAMOFFSET;

  // Always start with CS, CLK in 1 state
  __R30 = __R30 | (1 << CS);
  __R30 = __R30 | (1 << CLK);

  // Start with MOSI in 0 state
  __R30 = __R30 & ~(1 << MOSI);

  // Reset A/D
  // pru_spi_reset();

  while (1) {
    memptr = 0;

    // watch for flag set and dispatch on which flag is sent.
    flag = pMEM[memptr++];
    switch (flag) {

    //--------------------------------------------------
    case NOP:
    case SPI_WAIT_COMMAND:
      // Do nothing, wait for instruction
      break;

    //--------------------------------------------------
    case SPI_TEST:
      pMEM[0] = 0xff;
      // If printf is turned on in spidriver, then use 200000, else use 200
      __delay_cycles(200000);      
      pMEM[0] = 0x00;
      break;

    //--------------------------------------------------
    case SPI_WRITE:
     // Tell ARM caller I am working on it.
      pMEM[0] = (uint32_t) 0xee;

      tx_word_cnt = pMEM[memptr++];
      // fill buffer to pass to PRU
      for (i=0; i<tx_word_cnt; i++) {
        tx_words[i] = pMEM[memptr++];
      }    
 
      // Call fcn which does the bitbanging
      pru_spi_write(tx_words, tx_word_cnt);

      // Tell ARM caller I am done.
      pMEM[0] = 0x00;

      __delay_cycles(DELAY_CNT);
      break;

    //-------------------------------------------------------------
    case SPI_WRITEREAD_SINGLE:
     // Tell ARM caller I am working on it.
      pMEM[0] = (uint32_t) 0xee;

      // Set up Tx buffer
      tx_word_cnt = pMEM[memptr++];
      // fill buffer with tx commands to pass to PRU
      for (i=0; i<tx_word_cnt; i++) {
        tx_words[i] = pMEM[memptr++];
      }

      // Set up rx buffer
      rx_word_cnt = pMEM[memptr++];
      rxmemptr = memptr;
      //for (i=0; i<rx_word_cnt; i++) {
      //  rx_words[i] = pMEM[memptr++];
      //}

      // Call fcn which does bitbanging
      pru_spi_writeread_single(tx_words, tx_word_cnt, rx_words, rx_word_cnt);

      // Copy reply back into pMEM
      pMEM[rxmemptr] = rx_words[0];

      // Tell ARM caller I am done.
      pMEM[0] = (uint32_t) 0x00;

      __delay_cycles(DELAY_CNT);
      break;

    //-------------------------------------------------------------
    case SPI_WRITEREAD_CONTINUOUS:
      // Tell ARM caller I am working on it.
      pMEM[0] = (uint32_t) 0xee;

      tx_word_cnt = pMEM[memptr++];
      // fill buffer with tx commands to pass to PRU
      for (i=0; i<tx_word_cnt; i++) {
        tx_words[i] = pMEM[memptr++];
      }

      // Set up rx buffer
      rx_word_cnt = pMEM[memptr++];   // Number of bytes in one conversion value
      ncnv = pMEM[memptr++];          // Total number of conversions requested
      rxmemptr = memptr;
      //for (i=0; i<ncnv; i++) { // Load rx buffer with zeros.  Can remove this.
      //  rx_words[i] = 0xff;  // pMEM[memptr++];
      //}

      // Call fcn which does bitbanging
      pru_spi_writeread_continuous(tx_words, tx_word_cnt, &(pMEM[rxmemptr]), rx_word_cnt, ncnv);

      // Copy reply back into pMEM
      //for (i=0; i<ncnv; i++) {
      //  pMEM[rxmemptr+i] = rx_words[i];
      //}

      // Tell ARM caller I am done.
      pMEM[0] = (uint32_t) 0x00;

      __delay_cycles(DELAY_CNT);
      break;


    //----------------------------------------------------------
    case SPI_RESET:
     // Tell ARM caller I am working on it.
      pMEM[0] = (uint32_t) 0xee;
      pru_spi_reset();
      // Tell ARM caller I am done.
      pMEM[0] = (uint32_t) 0x00;
      break;

    //----------------------------------------------------------
    default:
      break;

    }    // switch (flag)       
  }   //   while (1) 

  // We'll never get here
  return 0;
}
