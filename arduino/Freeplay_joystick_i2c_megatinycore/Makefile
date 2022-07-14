# Makefile for Arduino based scketches
#
# Copyright 2020 Valerio Di Giampietro http://va.ler.io v@ler.io
# MIT License - see License.txt file
#
# This Makefile uses the arduino-cli, the Arduino command line interface
# and has been designed and tested to run on Linux, not on Windows.
# Probably it will run on a Mac, but it has not been tested.
#
# Please note that:
#
#   1. each sketch must reside in his own folder with this Makefile
#
#   2. the main make targets are:
#      - all     compiles and upload
#      - compile compiles only
#      - upload  upload via serial port, compile if the binary file is
#                not available
#      - clean   clean the build directory
#      - requirements it the file "requirements.txt" exists it will
#                     install the libraries listed in this file
#
#      default is "all"
#
#   3. it gets the name of the sketch using the wildcard make command;
#      the name is *.ino; this means that you must have ONLY a file
#      with .ino extension, otherwise this makefile will break.  This
#      also means that you can use this Makefile, almost unmodified,
#      for any sketch as long as you keep a single .ino file in each
#      folder
#
#   4. you can split your project in multiple files, if you wish,
#      using a single .ino file and multiple .h files, that you can
#      include in the .ino file with an '#include "myfile.h"'
#      directive
#
# Optionally some environment variables can be set:
#
#   FQBN        Fully Qualified Board Name; if not set in the environment
#               it will be assigned a value in this makefile
#
#   SERIAL_DEV  Serial device to upload the sketch
#
#   V           verbose flag; can be 0 (quiet) or 1 (verbose); if not set
#               in the environment it will be assigned a default value
#               in this makefile


ifeq ($(origin CHIP), undefined)
CHIP = 1627
#echo "Setting chip to 1627.  Use CHIP=817 to override!"
endif

ifeq "$(CHIP)" ""
CHIP = 1627
endif

BUILD_DIR   := Freeplay_joystick_i2c_megatinycore_build_$(CHIP)

#CHIP       ?= 1627
#CHIP       := 817
FQBN       ?= megaTinyCore:megaavr:atxy7:chip=$(CHIP),clock=10internal,bodvoltage=1v8,bodmode=disabled,eesave=enable,millis=enabled,resetpin=UPDI,startuptime=0
V          ?= 0
VFLAG      = -t

ifeq "$(V)" "1"
VFLAG      =-v
endif

#bin/arduino-cli upload -b megaTinyCore:megaavr:atxy7:chip=817,clock=10internal,bodvoltage=1v8,bodmode=disabled,eesave=enable,millis=enabled,resetpin=UPDI,startuptime=0 -p /dev/ttyAMA0 -P serialupdi -t --input-dir ./Freeplay_joystick_i2c_megatinycore_build/

SERIAL_DEV = /dev/ttyAMA0
PROGRAMMER = serialupdi

ARDUINO_CLI = bin/arduino-cli
SRC        := $(wildcard *.ino)
HDRS       := $(wildcard *.h)
BIN        := $(BUILD_DIR)/$(SRC).bin
ELF        := $(BUILD_DIR)/$(SRC).elf
HEX        := $(BUILD_DIR)/$(SRC).hex

$(info FQBN       is [${FQBN}])
$(info IOT_NAME   is [${IOT_NAME}])
$(info OTA_PORT   is [${OTA_PORT}])
$(info OTA_PASS   is [${OTA_PASS}])
$(info V          is [${V}])
$(info VFLAG      is [${VFLAG}])
$(info MAKE_DIR   is [${MAKE_DIR}])
$(info BUILD_DIR  is [${BUILD_DIR}])
$(info SRC        is [${SRC}])
$(info HDRS       is [${HDRS}])
$(info BIN        is [${BIN}])
$(info SERIAL_DEV is [${SERIAL_DEV}])

all: upload
.PHONY: all

compile: $(HEX)
.PHONY: compile

requirements: $(ARDUINO_CLI)
.PHONE: requirements

$(HEX): $(SRC) $(HDRS) $(ARDUINO_CLI)
	bin/arduino-cli compile -b $(FQBN) $(VFLAG) --output-dir $(BUILD_DIR)

upload: $(HEX)
	@if [ ! -c $(SERIAL_DEV) ] ; \
	then echo "---> ERROR: Serial Device not available, please set the SERIAL_DEV environment variable" ; \
	else echo "---> Uploading sketch\n"; \
	sudo systemctl stop hciuart.service 
	sudo systemctl stop bluetooth.service 
	sudo rmmod hci_uart 
	sudo rmmod bnep btbcm bluetooth
	raspi-gpio set 30-33 ip
	raspi-gpio set 14 a0
	raspi-gpio set 15 a0 pu
	raspi-gpio set 11 op dh
	bin/arduino-cli upload -b $(FQBN) -p $(SERIAL_DEV) -P $(PROGRAMMER) $(VFLAG) --input-dir $(BUILD_DIR); \
	fi

clean:
	@echo "---> Cleaning the build directory"
	rm -rf $(BUILD_DIR)

$(ARDUINO_CLI):
	curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
	bin/arduino-cli config init --overwrite
	bin/arduino-cli config add board_manager.additional_urls http://drazzy.com/package_drazzy.com_index.json
	bin/arduino-cli core update-index --additional-urls http://drazzy.com/package_drazzy.com_index.json
	bin/arduino-cli core search megaTinyCore
	bin/arduino-cli core install megaTinyCore:megaavr
