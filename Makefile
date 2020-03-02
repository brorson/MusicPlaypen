#
# Run:
#  $ make
# to build all the example programs.
#

#----------------------------------------------------
# ARM code
CC := gcc
CFLAGS := -O3 -mfpu=vfpv3 -mfloat-abi=hard -march=armv7 -I./include
LDFLAGS := /usr/lib/arm-linux-gnueabihf/libgfortran.so.3 -l:liblapacke.a -l:liblapack.a -l:libcblas.a -l:libblas.a -lm

SRCS := main.c prussdrv.c adcdriver_host.c spidriver_host.c matrix_utils.c
OBJS := main.o prussdrv.o adcdriver_host.o spidriver_host.o matrix_utils.o
EXES := main
INCLUDEDIR := ./include
INCLUDES := $(addprefix $(INCLUDEDIR)/, prussdrv.h pru_types.h __prussdrv.h pruss_intc_mapping.h spidriver_host.h adcdriver_host.h matrix_utils.h)

#----------------------------------------------------
# PRU code
DEVICE=am335x
PRU_CC := clpru
PRU_CC_FLAGS := --silicon_version=3 -I./include -I/usr/share/ti/cgt-pru/include/ -D$(DEVICE) -i/usr/share/ti/cgt-pru/lib
PRU_LINKER_SCRIPT := AM335x_PRU.cmd
PRU_INCLUDES := resource_table_empty.h pru_ctrl.h pru_intc.h pru_cfg.h pru_spi.h

# PRU0 is the SPI stuff.
PRU0_SRCS := pru0.c pru_spi.c
PRU0_OBJS := pru0.obj pru_spi.obj
PRU0_MAP := pru0.map
PRU0_EXES := data0.bin text0.bin

PRU_HEXPRU_SCRIPT := bin.cmd

#=================================================
all: main pru0.bin ADC_001-00A0.dtbo

bins: main pru0.bin

#--------------------------------
# Compile ARM sources for host.
main.o: $(SRCS)
	echo "--> Building main.o"
	$(CC) $(CFLAGS) -c $< -o $@

adcdriver_host.o: adcdriver_host.c ./include/adcdriver_host.h
	echo "--> Building adcdriver_host.o"
	$(CC) $(CFLAGS) -c $< -o $@

spidriver_host.o: spidriver_host.c ./include/spidriver_host.h
	echo "--> Building spidriver_host.o"
	$(CC) $(CFLAGS) -c $< -o $@

prussdrv.o: prussdrv.c # $(DEPS)
	echo "--> Building prussdrv.o"
	$(CC) $(CFLAGS) -c $< -o $@

# Link the ARM objects
main: $(OBJS) 
	echo "--> Linking ARM stuff...."
	$(CC) $(CFLAGS) $^ $(LIBLOCS) $(LDFLAGS) -o $@ 

$(OBJS): $(INCLUDES)

#--------------------------------
# Compile and link the PRU sources to create ELF executable
pru0.out: pru0.c pru_spi.c
	echo "--> Building and linking PRU0 stuff..."
	$(PRU_CC) $^ $(PRU_CC_FLAGS) -z $(PRU_LINKER_SCRIPT) -o $@ -m $(PRU0_MAP)

# Build PRU .bin file from ELF
pru0.bin: pru0.out $(PRU_HEXPRU_SCRIPT)
	echo "--> Running hexpru for PRU0..."
	hexpru $(PRU_HEXPRU_SCRIPT) $<
	-mv text.bin text0.bin
	-mv data.bin data0.bin

#--------------------------------
# Build and install device tree overlay
ADC_001-00A0.dtbo: ADC_001.dts
	dtc -O dtb -o ADC_001-00A0.dtbo -b 0 -@ ADC_001.dts
	cp ADC_001-00A0.dtbo /lib/firmware

#-------------------------------
# Clean up directory -- remove executables and intermediate files.
clean:
	-rm -f *.o *.obj *.out *.map $(EXES) $(OBJS) \
	 $(PRU0_OBJS) $(PRU0_EXES) $(PRU1_OBJS) $(PRU1_EXES) *~ *.dtbo



