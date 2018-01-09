//----------------------------------------------------------------------
// spidriver_host -- Host-side A/D wrapper which sends commands to PRUs.
// This code is meant to run on the host (ARM).  The goal of the wrapping
// is to deal with low-level spi issues like inserting the bytecount and flag
// at the beginning of the message, init of PRU communication, etc.
//
//-----------------------------------------------------------------------
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include <prussdrv.h>
#include <pruss_intc_mapping.h>

#include "spidriver_host.h"
#include "pru_spi.h"

#define PRU0 0
#define PRU1 1

//=====================================================================
uint8_t pruss_init(void) {
  // This initializes the PRUSS driver stuff, and sets up the
  // host side of the link.  The code is not specific to one
  // PRU or the other -- it is about configuring the PRUSS stuff
  // on the host.
  uint8_t retval = 0;

  tpruss_intc_initdata pruss_intc_initdata = PRUSS_INTC_INITDATA;
  retval |= prussdrv_init ();
  retval |= prussdrv_open (PRU_EVTOUT_0);
  retval |= prussdrv_pruintc_init(&pruss_intc_initdata);  
  //printf("Just ran pruss_init, retval = %d\n", retval);
  return retval;

}

//-------------------------------------------------------------------
uint8_t pru0_init(void) {
  // This initializes PRU0 -- the spi communication link.  It
  // zeros out the communication memory, then loads and runs the
  // PRU0 program.
  uint8_t retval = 0;

  // printf("===>  Entered pru0_init.\n");

  // Get pointer to PRU0 dataram.
  // printf("Asking for pointer to PRU0 dataram.\n");
  retval = prussdrv_map_prumem(PRUSS0_PRU0_DATARAM, (void **) &pru0_dataram);
  if (retval != 0) {
     printf("prussdrv_map_prumem PRUSS0_PRU0_DATARAM map failed\n");
     exit(-1);
  }

  // printf("Asking for pointer to PRU0 dataram.\n");
  prussdrv_pru_reset(PRU0);

  // Now run program on PRU0
  // printf("Now trying to start program on PRU0.\n");
  retval = prussdrv_exec_program (PRU0, "./text0.bin");
  if (retval != 0) {
     printf("prussdrv_exec_program(PRU0) failed\n");
     exit(-1);
  }

  usleep(500);
  // printf("<===  Done.  Now leaving pru0_init.\n");

  return 0;
}

//-------------------------------------------------------------------
uint8_t pru1_init(void) {
  // This initializes PRU1 -- the spi communication link.  It
  // zeros out the communication memory, then loads and runs the
  // PRU0 program.
  uint8_t retval = 0;

#if 0
  // Get pointer to PRU0 dataram.
  retval = prussdrv_map_prumem(PRUSS0_PRU1_DATARAM, (void **) &pru1_dataram);
  if (retval != 0) {
     printf("prussdrv_map_prumem PRUSS0_PRU1_DATARAM map failed\n");
     exit(-1);
  }
#endif

  prussdrv_pru_reset(PRU1);

#if 0
  // Now run program on PRU1
  retval = prussdrv_exec_program (PRU1, "./text1.bin");
  if (retval != 0) {
     printf("prussdrv_exec_program(PRU1) failed\n");
     exit(-1);
  }
#endif

  usleep(500);
  return 0;
}


//--------------------------------------------------------------
uint8_t pru_reset(int prunum) {
  prussdrv_pru_disable(prunum);
  prussdrv_pru_reset(prunum);
}


//---------------------------------------------------------------------------
void spi_reset_cmd(void) {
  int i;
  int N;
  volatile uint32_t tmp;
  printf("Sending reset command to PRU.....\n");

  pru_write_word(0, SPI_RESET);

  // Now wait until write is complete.
  N = 10000000;
  for (i=0; i<N; i++) {
    tmp = pru_read_word(0x00);
    // printf("...tmp = %d\n", tmp);
    if (!tmp) {
      break;
    }
  }
  // printf("In spi_reset_cmd, at end of waiting, i = %d, tmp = %d\n", i, tmp);
}

//--------------------------------------------------------------
uint32_t pru_read_word(uint32_t offset) {
  // Must make these volatile so compiler evaluates them
  // every time this fcn is called.
  uint32_t *mem_ptr;
  uint32_t *mem_ptr_32;
  const uint32_t ram_offset = RAMOFFSET;
  uint32_t retval;

  mem_ptr = pru0_dataram;
  mem_ptr_32 = mem_ptr + ram_offset + offset;
  msync(mem_ptr_32, 1, MS_SYNC);
  retval = *mem_ptr_32;

  //printf("--> In pru_read_word, just read 0x%08x\n", retval);

  return retval;
}

//--------------------------------------------------------------
void pru_write_word(uint32_t offset, uint32_t value) {
  uint32_t *mem_ptr;
  uint32_t *mem_ptr_32;
  const uint32_t ram_offset = RAMOFFSET;

  mem_ptr = pru0_dataram;
  mem_ptr_32 = mem_ptr + ram_offset + offset;
  *mem_ptr_32 = value;
  msync(mem_ptr_32, 1, MS_SYNC);

  //printf("--> In pru_write_word, just wrote 0x%08x\n", value);

}


//--------------------------------------------------------------
uint32_t pru_test_ram(uint32_t offset, uint32_t value) {
  // Use this fcn to verify that the PRU RAM is configured correctly.
  uint32_t *mem_ptr;
  uint32_t *mem_ptr_32;
  const uint32_t ram_offset = RAMOFFSET;
  uint32_t retval;

  printf("----> Entered pru_test_ram, value to write = %d\n", value);

  // Write input to PRU RAM
  pru_write_word(offset, value);

  // Read value back out PRU RAM
  retval = pru_read_word(offset);

  printf("      In pru_test_ram, value read back = %d\n", retval);

  if (value == retval) {
    printf("      ram test passed!\n");
  } else {
    printf("      ram test faialed!\n");
  }

  printf("<---- Leaving pru_test_ram\n");

  return retval;
}

//--------------------------------------------------------------
uint32_t pru_test_communication(void) {
  int i;
  int N;
  volatile uint32_t tmp;
  
  // Now give it flag so it can go and do its thing
  printf("------> Entered pru_test_communication....\n");
  printf("        send SPI_TEST to PRU to test communication link...\n");
  pru_write_word(0, SPI_TEST);

  // Now wait until write is complete.
  N = 1000000;
  for (i=0; i<N; i++) {
    tmp = pru_read_word(0x00);
    // printf("...tmp = %d\n", tmp);
    if (!tmp) {
      break;
    }
  }
  printf("          In pru_test_communication, at end of waiting, i = %d, tmp = %d\n", i, tmp);
  if (i == N) {
    printf("        Communications test failed!\n");
  } else {
    printf("        Communications test passed!\n");
  }
  printf("<------ Leaving pru_test_communication.\n");
  return i;
}



//===============================================================
// These are high-level fcns which perform a command or a conversion read.
//--------------------------------------------------------------
uint32_t spi_write_cmd(uint32_t *data, int word_cnt) {
  // Pass a word count and a pointer to the data, send command to
  // PRU0 (SPI master) to pass on to A/D.
  // Max word count is 3 -- corresponding to max write len to AD7172.

  uint32_t retval = 0;

  /* Command message structure is:
  uint32_t flag -- specifying what command to do
  uint32_t word_count
  uint32_t data[4]
  */

  uint32_t i;
  volatile uint32_t tmp;
  uint32_t mem_ptr = 0x00;
  uint32_t N;

  // printf("--> In spi_write_cmd, writing %d words\n", word_cnt);

  // First set up PRU0 memory with data I want to transmit
  pru_write_word(mem_ptr++, 0xff);
  pru_write_word(mem_ptr++, word_cnt);

  for (i = 0; i < word_cnt; i++) {
    pru_write_word(mem_ptr++, data[i]);
    // printf("   Tx byte %d = 0x%08x\n", i, data[i]);
  }

  // Now give it flag so it can go and do its thing
  // printf("Send SPI_WRITE to PRU to do execute SPI write...\n");
  pru_write_word(0, SPI_WRITE);

  // Now wait until write is complete.
  N = 10000000;
  for (i=0; i<N; i++) {
    tmp = pru_read_word(0x00);
    if (!tmp) {
      break;
    }
  }
  if (i == N) {
    printf("In spi_write_cmd, timed out waiting for end of transaction!\n");
    pru_reset(PRU0);
    prussdrv_exit();
    exit(-1);
  }
 
  // printf("In spi_write_cmd, at end of waiting, i = %d, tmp = %d\n", i, tmp);
  return retval;

}


//--------------------------------------------------------------
uint8_t spi_writeread_single(uint32_t *txdata, int txcnt, uint32_t *rxdata, int rxcnt) {
  // Pass a byte count and a pointer to the data, send command to
  // PRU0 (SPI master) to pass on to A/D, then read response.
  // This fcn takes care of translating the input data (bytes) into the data sent
  // to the PRU (uint32_t).

  // This version first asserts CS down, then waits until MISO goes up then down
  // before sending the "get data" command.

  /* Command message structure is:
  uint32_t flag -- specifying what command to do
  uint32_t tx_byte_count
  uint32_t tx_data[<tx_byte_count>]
  uint32_t rx_byte_count
  uint32_t rx_data[<rx_byte_count>]
  */

  uint32_t i;
  volatile int tmp;
  uint32_t memptr = 0x00;
  uint32_t rxptr;
  uint32_t N;

  // printf("--> In spi_writeread_single, writing %d words\n", txcnt);

  // Set up transmitted data
  pru_write_word(memptr++, 0xff);      // Put PRU in "Wait for command" mode
  pru_write_word(memptr++, txcnt);     // Number of tx bytes to send.
  for (i = 0; i < txcnt; i++) {
    pru_write_word(memptr++, (uint32_t) txdata[i]);
    // printf("   Tx byte %d = 0x%08x\n", i, txdata[i]);
  }

  // Set up receive buffer
  pru_write_word(memptr++, rxcnt);
  rxptr = memptr;                  // This points to begin of rx data.
  for (i = 0; i < rxcnt; i++) {
    pru_write_word(memptr++, (uint32_t) 0x00);
  }

  // Now send the instruction flag.
  pru_write_word(0, SPI_WRITEREAD_SINGLE);

  // Wait for transaction to complete.
  N = 10000000;
  // N = 100;
  for (i=0; i<N; i++) {
    tmp = pru_read_word(0x00);
    // printf("In spi_writeread_single, tmp = 0x%08x\n", tmp);
    if (!(tmp)) {
      break;
    }
  }
  if (i == N) {
    printf("In spi_writeread_single, timed out waiting for end of transaction!\n");
    pru_reset(PRU0);
    prussdrv_exit();
    exit(-1); 
  } 

  // printf("After waiting for SPI_WRITEREAD_SINGLE, i = %d, tmp = 0x%02x\n", i, tmp);


  // At end of transaction, the received data should be placed into
  // rx_data.
  rxdata[0] = pru_read_word(rxptr);
  // printf("   Rx data %d = 0x%08x\n", i, rxdata[0]);

  // May want to return number of received words here
  return rxcnt;
}


//---------------------------------------------------------------------------
uint8_t spi_writeread_continuous(uint32_t *txdata, int txcnt, uint32_t *rxdata, int rxcnt, int ncnv) {
  // The only difference between this fcn and writeread_single is 
  // that this fcn invokes the SPI_WRITEREAD_CONTINUOUS method
  // on the PRU.

  /* Command message structure is:
  uint32_t flag -- specifying what command to do
  uint32_t tx_byte_count
  uint32_t tx_data[<tx_byte_count>]
  uint32_t rx_byte_count
  uint32_t rx_data[<rx_byte_count>]
  */

  uint32_t i;
  volatile int tmp;
  uint32_t memptr = 0x00;
  uint32_t rxptr;
  uint32_t N;

  // printf("--> In spi_writeread_continuous, writing %d bytes\n", txcnt);

  // Set up transmitted data
  pru_write_word(memptr++, 0xff);      // Put PRU in "Wait for command" mode
  pru_write_word(memptr++, txcnt);     // Number of tx bytes to send.
  for (i = 0; i < txcnt; i++) {
    pru_write_word(memptr++, txdata[i]);
    // printf("   Tx word %d = 0x%08x\n", i, txdata[i]);
  }

  // Set up receive buffer
  pru_write_word(memptr++, rxcnt);  // Number of bytes in one conversion value
  pru_write_word(memptr++, ncnv);   // Total number of conversions requested
  rxptr = memptr;                   // This points to begin of rx data.
  for (i = 0; i < ncnv; i++) {
    pru_write_word(memptr++, 0x00);
  }

  // Now send the instruction flag.
  pru_write_word(0, SPI_WRITEREAD_CONTINUOUS);

  // Wait for transaction to complete.
  N = 10000000;
  for (i=0; i<N; i++) {
    tmp = pru_read_word(0x00);
    // printf("In spi_writeread_continuous, tmp = 0x%08x\n", tmp);
    if (!(tmp)) {
      break;
    }
  }
  if (i == N) {
    printf("In spi_writeread_continuous, timed out waiting for end of transaction!\n");
    pru_reset(PRU0);
    prussdrv_exit();
    exit(-1); 
  } 
 
  // printf("After waiting for SPI_WRITEREAD_CONTINUOUS, i = %d, tmp = 0x%02x\n", i, tmp);


  // At end of transaction, the received data should be placed into
  // rx_data.
  for (i = 0; i < ncnv; i++) {    
    rxdata[i] = pru_read_word(rxptr+i);
    // printf("   Rx word %d = 0x%08x\n", i, rxdata[i]);
  } 

  // May want to return number of received words here
  return ncnv;
}

