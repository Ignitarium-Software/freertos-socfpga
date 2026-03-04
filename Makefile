THIS_MK_ABSPATH := $(abspath $(lastword $(MAKEFILE_LIST)))
THIS_MK_DIR := $(dir $(THIS_MK_ABSPATH))

# Enable pipefail for all commands
SHELL=/bin/bash -o pipefail

# Enable second expansion
.SECONDEXPANSION:

# Clear all built in suffixes
.SUFFIXES:

NOOP :=
SPACE := $(NOOP) $(NOOP)
COMMA := ,
HOSTNAME := $(shell hostname)

##############################################################################
# Environment check
##############################################################################


##############################################################################
# Configuration
##############################################################################
PYTHON3 ?= python3
VENV_DIR := venv
VENV_PY := $(VENV_DIR)/bin/python
VENV_PIP := $(VENV_DIR)/bin/pip
ifneq ($(https_proxy),)
PIP_PROXY := --proxy $(https_proxy)
else
PIP_PROXY :=
endif
VENV_PIP_INSTALL := $(VENV_PIP) install $(PIP_PROXY) --no-cache-dir --timeout 120 --retries 20 --trusted-host pypi.org --trusted-host files.pythonhosted.org

CMAKE_COMMAND = cmake

#Doxygen output path
DOXYGEN_PATH := docs

#Specify build type
BUILD_TYPE ?= Release

#Specify boot core
CORE ?= A55

#Specify SOC
SOC ?= AGILEX5

#SMP config
SMP ?= OFF

#demo path for hello world and demo applications
DEMO_PATH := FreeRTOS/Demo/SOCFPGA/apps
SAMPLES_PATH := samples/

##############################################################################
# Makefile starts here
##############################################################################

##############################################################################
# Makefile starts here
##############################################################################

# Default target executed when no arguments are given to make.
default_target: all
.PHONY : default_target

###############################################################################
#                          UTILITY TARGETS
###############################################################################
# Deep clean using git
.PHONY: dev-clean
dev-clean : clean
	git clean -dfx --exclude=/.vscode --exclude=.lfsconfig

# Using git
.PHONY: dev-update
dev-update :
	git pull
	git submodule update --init --recursive

# Prep workspace
venv:
	$(PYTHON3) -m venv $(VENV_DIR)
	$(VENV_PIP_INSTALL) --upgrade pip
	$(VENV_PIP_INSTALL) -r requirements.txt


.PHONY: venv-freeze
venv-freeze:
	$(VENV_PIP) freeze > requirements.txt
	sed -i -e 's/==/~=/g' requirements.txt

.PHONY: prepare-tools
prepare-tools : venv


###############################################################################
#                          Samples Targets
###############################################################################

hello_world:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(DEMO_PATH)/hello_world -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@
	@echo "hello world build completed Successfully.\nOutput Directory : build/$@"
.PHONY : hello_world

cli_app:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(DEMO_PATH)/samples/$@ -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "cli app build completed Successfully.\nOutput Directory : build/$@"
.PHONY : cli_app

main_full:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(DEMO_PATH)/samples/$@ -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "os tests build completed Successfully.\nOutput Directory : build/$@"
.PHONY : main_full

main_blinky:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(DEMO_PATH)/samples/$@ -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "blinky app build completed Successfully.\nOutput Directory : build/$@"
.PHONY : main_blinky

enet_demo:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(DEMO_PATH)/samples/main_freertosplus_basic -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@  -j${nproc}
	@echo "enet app build completed Successfully.\nOutput Directory : build/$@"
.PHONY : enet_demo

samples: dma_sample bridge_sample fatfs_sample fpga_manager_sample gpio_sample i2c_sample i3c_sample iossm_sample qspi_sample sdmmc_sample reboot_manager_sample seu_sample spi_sample timer_sample uart_sample fcs_sample rsu_sample sdm_mailbox_sample usb3_sample wdt_sample multi_thread_sample ecc_sample usb_otg_sample
.PHONY : samples

dma_sample:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(SAMPLES_PATH)/dma -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "$@ build completed Successfully.\nOutput Directory : build/$@"
.PHONY : dma_sample

ecc_sample:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(SAMPLES_PATH)/ecc -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "$@ build completed Successfully.\nOutput Directory : build/$@"
.PHONY : ecc_sample

bridge_sample:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(SAMPLES_PATH)/bridge -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "$@ build completed Successfully.\nOutput Directory : build/$@"
.PHONY : bridge_sample

fatfs_sample:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(SAMPLES_PATH)/fatfs -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "$@ build completed Successfully.\nOutput Directory : build/$@"
.PHONY : fatfs_sample

fpga_manager_sample:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(SAMPLES_PATH)/fpga_manager -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "$@ build completed Successfully.\nOutput Directory : build/$@"
.PHONY : fpga_manager_sample

gpio_sample:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(SAMPLES_PATH)/gpio -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "$@ build completed Successfully.\nOutput Directory : build/$@"
.PHONY : gpio_sample

i2c_sample:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(SAMPLES_PATH)/i2c -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "$@ build completed Successfully.\nOutput Directory : build/$@"
.PHONY : i2c_sample

i3c_sample:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(SAMPLES_PATH)/i3c -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "$@ build completed Successfully.\nOutput Directory : build/$@"
.PHONY : i3c_sample

iossm_sample:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(SAMPLES_PATH)/iossm -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "$@ build completed Successfully.\nOutput Directory : build/$@"
.PHONY : iossm_sample

qspi_sample:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(SAMPLES_PATH)/qspi -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "$@ build completed Successfully.\nOutput Directory : build/$@"
.PHONY : qspi_sample

reboot_manager_sample:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(SAMPLES_PATH)/reboot_mngr -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "$@ build completed Successfully.\nOutput Directory : build/$@"
.PHONY : reboot_manager_sample

sdm_mailbox_sample:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(SAMPLES_PATH)/sdm_mailbox -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "$@ build completed Successfully.\nOutput Directory : build/$@"
.PHONY : sdm_mailbox_sample

sdmmc_sample:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(SAMPLES_PATH)/sdmmc -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "$@ build completed Successfully.\nOutput Directory : build/$@"
.PHONY : sdmmc_sample

seu_sample:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(SAMPLES_PATH)/seu -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "$@ build completed Successfully.\nOutput Directory : build/$@"
.PHONY : seu_sample

spi_sample:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(SAMPLES_PATH)/spi -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "$@ build completed Successfully.\nOutput Directory : build/$@"
.PHONY : spi_sample

timer_sample:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(SAMPLES_PATH)/timer -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "$@ build completed Successfully.\nOutput Directory : build/$@"
.PHONY : timer_sample

uart_sample:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(SAMPLES_PATH)/uart -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "$@ build completed Successfully.\nOutput Directory : build/$@"
.PHONY : uart_sample

fcs_sample:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(SAMPLES_PATH)/fcs -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "$@ build completed Successfully.\nOutput Directory : build/$@"
.PHONY : fcs_sample

rsu_sample:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(SAMPLES_PATH)/rsu -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "$@ build completed Successfully.\nOutput Directory : build/$@"
.PHONY : rsu_sample

usb3_sample:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(SAMPLES_PATH)/usb3 -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "$@ build completed Successfully.\nOutput Directory : build/$@"
.PHONY : usb3_sample

wdt_sample:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(SAMPLES_PATH)/wdt -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "$@ build completed Successfully.\nOutput Directory : build/$@"
.PHONY : wdt_sample

multi_thread_sample:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(SAMPLES_PATH)/multithread_samples -B build/$@ -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "$@ build completed successfully.\nOutput directory: build/$@"
.PHONY : multi_thread_sample

usb_otg_sample:
	rm -rf build/$@
	@$(CMAKE_COMMAND) -S $(SAMPLES_PATH)/usb_otg -B build/$@ -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCORE=$(CORE) -DSOC=$(SOC) -DSMP=$(SMP)
	@make -C build/$@ -j${nproc}
	@echo "$@ build completed Successfully.\nOutput Directory : build/$@"
.PHONY : usb3_sample

all: hello_world cli_app enet_demo main_full main_blinky samples

help:
	@echo "The following are some of the valid targets for this Makefile:"
	@echo "... all (the default if no target is provided)"
	@echo "... clean"
	@echo "... hello_world"
	@echo "... cli_app"
	@echo "... enet_demo"
	@echo "... main_blinky"
	@echo "... main_full"
	@echo "... samples (builds all the sample applications)"
	@echo "... dma_sample"
	@echo "... bridge_sample"
	@echo "... ecc_sample"
	@echo "... fatfs_sample"
	@echo "... fpga_manager_sample"
	@echo "... gpio_sample"
	@echo "... i2c_sample"
	@echo "... i3c_sample"
	@echo "... iossm_sample"
	@echo "... qspi_sample"
	@echo "... reboot_manager_sample"
	@echo "... sdm_mailbox_sample"
	@echo "... sdmmc_sample"
	@echo "... seu_sample"
	@echo "... spi_sample"
	@echo "... timer_sample"
	@echo "... uart_sample"
	@echo "... fcs_sample"
	@echo "... usb3_sample"
	@echo "... wdt_sample"
	@echo "... sdk_doc (build doxygen docs)"
	@echo "... usb_otg_sample"
	@echo "Note : Dont run make with multiple job at once."
.PHONY : help

clean:
	rm -rf build
	rm -rf $(DOXYGEN_PATH)
	rm -rf RTOSApp.elf

sdk_doc:
	doxygen Doxyfile
	@echo "Doxygen build completed. Docs generated at : $(DOXYGEN_PATH)"


-include not_shipped/Makefile.mk
