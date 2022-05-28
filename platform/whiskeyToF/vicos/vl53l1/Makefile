#######################################################################
# Makefile for VL53L1 bare driver with linux userspace platform.
# @author Daniel Casner <daniel@anki.com>
#######################################################################

BIN_DIR ?= $(INSTALL_ROOT)/usr/bin

CORE_DIR = core
PLAT_DIR = platform
PROT_DIR = protected

CORE_SRC = $(CORE_DIR)/src/*.c
CORE_INC = $(CORE_DIR)/inc/
PLAT_SRC = $(PLAT_DIR)/src/*.c
PLAT_INC = $(PLAT_DIR)/inc/
PROT_SRC = $(PROT_DIR)/src/*.c
PROT_INC = $(PROT_DIR)/inc/

INCLUDE_FLAGS = -I$(CORE_INC) -I$(PLAT_INC) -I$(PROT_INC)

CFLAGS = -fPIC -O2

.PHONEY: clean

ALL_BINS = vl53l1_platform_test vl53l1_mz_test

all: $(ALL_BINS)


vl53l1_platform_test: vl53l1_platform_test.c
	$(CC) $(CFLAGS) $(INCLUDE_FLAGS) $(CORE_SRC) $(PLAT_SRC) $(PROT_SRC) $< -o $@

vl53l1_mz_test: vl53l1_MultiZones_Scanning_Example.c
	$(CC) $(CFLAGS) $(INCLUDE_FLAGS) $(CORE_SRC) $(PLAT_SRC) $(PROT_SRC) $< -o $@

$(BIN_DIR): $(INSTALL_ROOT)
	mkdir -p $@

install: $(ALL_BINS) $(BIN_DIR)
	cp -v $(ALL_BINS) $(BIN_DIR)/$(exe)

clean:
	rm -f ./core/src/*.o
	rm -f ./protected/src/*.o
	rm -f ./platform/src/*.o
	rm -f ./*.o *.so
	rm -f $(ALL_BINS)
