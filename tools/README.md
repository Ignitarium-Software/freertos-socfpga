# Building the FreeRTOS images

# Overview
This page contains the instructions to build the complete FreeRTOS images from its individual components which include:-
- hardware design
- Arm Trusted firmware
- FreeRTOS kernel and applications

The instructions are based on the GHRD design.

# 1. Prerequisites
- Altera Agilex 5 FPGA E-Series 065B Premium Development Kit, ordering code DK-A5E065BB32AES1.
- Host PC with
	- Linux OS installed. All the mentioned documentation are based on Ubuntu 20.04.6 LTS.
	- Serial terminal ( minicom/picocom/GtkTerm )
	- Intel Quartus Prime Pro Edition Version 25.3. ( Used to compile hardware design and generate programming files ).
	- TFTP server. (Used to download eMMC binaries). See Section 8 to setup TFTP server.
- Internet access for downloading the files.

## 1.1 Update and upgrade your Ubuntu system
```bash
  sudo apt-get update
  sudo apt-get upgrade
  sudo apt-get install --no-install-recommends git cmake ninja-build gperf
  ccache dfu-util wget python3-dev python3-pip python3-setuptools python3-tk python3-wheel python3-venv xz-utils file libpython3-dev
  make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1 libguestfs-tools  libssl-dev
```
For the build process, a minimum CMake version of 3.24.0 is required. If you have an older CMake version, execute the following commands to add a non-intrusive cmake binary:
```bash
  CURR_FOLDER=$PWD
  mkdir -p $HOME/bin/cmake && cd $HOME/bin/cmake
  wget https://github.com/Kitware/CMake/releases/download/v3.24.0/cmake-3.24.0-Linux-x86_64.sh
  yes | sh cmake-3.24.0-Linux-x86_64.sh | cat
  export PATH=$PWD/cmake-3.24.0-linux-x86_64/bin:$PATH
  cd $CURR_FOLDER
```

# 2. Setting up the environment

## 2.1 Create FreeRTOS top level Directory

Create a top level directory as per your choice and set the TOP_FOLDER environment variable.

```bash
  rm -rf agilex5_rtos
  mkdir agilex5_rtos
  cd agilex5_rtos
  export TOP_FOLDER=$(pwd)
```
## 2.2 Set up the ARM toolchain
Download the compiler toolchain. Define environment variables and append the toolchain path in the environment PATH variable.

```bash
  wget https://developer.arm.com/-/media/Files/downloads/gnu/14.3.rel1/binrel/arm-gnu-toolchain-14.3.rel1-x86_64-aarch64-none-elf.tar.xz
  tar xf arm-gnu-toolchain-14.3.rel1-x86_64-aarch64-none-elf.tar.xz
  rm -rf  arm-gnu-toolchain-14.3.rel1-x86_64-aarch64-none-elf.tar.xz
  export PATH=`pwd`/arm-gnu-toolchain-14.3.rel1-x86_64-aarch64-none-elf/bin:$PATH
```
## 2.3 Enable Quartus tools to be called from command line
Download and install [Quartus]
```bash
  export QUARTUS_ROOTDIR=~/altera_pro/25.3/quartus/
  export PATH=$QUARTUS_ROOTDIR/bin:$QUARTUS_ROOTDIR/linux64:$QUARTUS_ROOTDIR/../qsys/bin:$PATH
```
## 2.4 Clone the ATF
```bash
  cd $TOP_FOLDER
  git clone https://github.com/altera-fpga/arm-trusted-firmware.git -b socfpga_v2.14.0 arm-trusted-firmware
```
## 2.5 Clone the SoC FPGA FreeRTOS SDK repo
```bash
  cd $TOP_FOLDER
  git clone git@github.com:Ignitarium-Technology/freertos-socfpga.git
  cd freertos-socfpga/
  git submodule update --init --recursive
```
## 2.6 Copy SOF file
  Copy the required SOF file to the **$TOP_FOLDER** directory before the build process.

# 3. Building the RTOS application
The following FreeRTOS sample applications are available to test:

- Hello World: FreeRTOS/Demo/SOCFPGA/apps/hello_world/
- Driver sample applications: samples/
- CLI Application: FreeRTOS/Demo/SOCFPGA/apps/samples/cli_app/
- TCP/IP Applications: FreeRTOS/Demo/SOCFPGA/apps/samples/main_freertosplus_basic/
- FreeRTOS blinky test: FreeRTOS/Demo/SOCFPGA/apps/samples/main_blinky/
- FreeRTOS full test:FreeRTOS/Demo/SOCFPGA/apps/samples/main_full/

To get the list of all available applications, run the following command:
```bash
  cd $TOP_FOLDER/freertos-socfpga/
  make help
```
Here, we will focus on building the **hello_world** application. All the other applications can be built in the same manner.

## 3.1 Build the "hello world" application
Make supports following configuration

- CORE=[A76/A55]
- BUILD_TYPE=[Release/Debug]
- SOC=[AGILEX5/AGILEX3]

Default config will be A55, Release and AGILEX5
```bash
  cd $TOP_FOLDER/freertos-socfpga/
  make hello_world CORE=A55 BUILD_TYPE=Release SOC=AGILEX5
```
The output(bin file) is:
**$TOP_FOLDER/build/hello_world/freertos_hello_world.bin**

# 4. Building fiptool
```bash
  cd $TOP_FOLDER/arm-trusted-firmware
  make fiptool
  cp tools/fiptool/fiptool $TOP_FOLDER
  cd $TOP_FOLDER
```

# 5. Building boot images
## 5.1 QSPI boot

- Build ATF
```bash
  cd $TOP_FOLDER
  cd arm-trusted-firmware
  make PLAT=agilex5 clean
  ARCH=arm64 CROSS_COMPILE=aarch64-none-elf- make PLAT=agilex5 SOCFPGA_BOOT_SOURCE_QSPI=1 bl2 bl31 PRELOADED_BL33_BASE=0x82000000 -j${nproc}
```
- Create FIP binary.
```bash
  cd $TOP_FOLDER
  rm -rf qspi_bin && mkdir qspi_bin && cd qspi_bin
  cp $TOP_FOLDER/arm-trusted-firmware/build/agilex5/release/bl2.bin $TOP_FOLDER/qspi_bin
  aarch64-none-elf-objcopy -v -I binary -O ihex --change-addresses 0x0 bl2.bin  bl2.hex
  $TOP_FOLDER/fiptool create --soc-fw $TOP_FOLDER/arm-trusted-firmware/build/agilex5/release/bl31.bin  --nt-fw $TOP_FOLDER/freertos-socfpga/build/hello_world/freertos_hello_world.bin fip.bin
```
- Build qspi jic image<br>
You need to have the SOF file for your design for the next step, We assume that the SOF is in the **$TOP_FOLDER** with name "**ghrd_a5ed065bb32ae6sr0.sof**"<br>
*Note: The steps assume the build is for Agilex 5. For Agilex 3 the pfg needs to be modified with the appropriate SOF file name*
```bash
  cd $TOP_FOLDER/qspi_bin
  cp $TOP_FOLDER/ghrd_a5ed065bb32ae6sr0.sof .
  wget https://releases.rocketboards.org/2024.11/zephyr/agilex5/hps_zephyr/hello_world/qspi_boot/qspi_flash_image_agilex5_boot.pfg
  quartus_pfg -c qspi_flash_image_agilex5_boot.pfg
```
The following output files are generated from the above steps:
- **qspi_image.jic**

## 5.2 SD boot

- Apply the following patch to the ATF code for selecting SDMMC as boot source
```bash
  cd $TOP_FOLDER/arm-trusted-firmware
  sed -i 's/MMC_DEVICE_TYPE[ \t]*0/MMC_DEVICE_TYPE 1/'  plat/intel/soc/agilex5/include/socfpga_plat_def.h
```
- Build ATF
```bash
  cd arm-trusted-firmware
  make PLAT=agilex5 clean
  ARCH=arm64 CROSS_COMPILE=aarch64-none-elf- make PLAT=agilex5 bl2 bl31 PRELOADED_BL33_BASE=0x82000000 -j${nproc}
```
- Create FIP binary.

```bash
  cd $TOP_FOLDER
  rm -rf sd_bin && mkdir sd_bin && cd sd_bin
  cp $TOP_FOLDER/arm-trusted-firmware/build/agilex5/release/bl2.bin $TOP_FOLDER/sd_bin
  aarch64-none-elf-objcopy -v -I binary -O ihex --change-addresses 0x0 bl2.bin  bl2.hex
  $TOP_FOLDER/fiptool create --soc-fw $TOP_FOLDER/arm-trusted-firmware/build/agilex5/release/bl31.bin  --nt-fw $TOP_FOLDER/freertos-socfpga/build/hello_world/freertos_hello_world.bin fip.bin
```
- Create qspi jic to boot from SD card
```bash
  cd $TOP_FOLDER/sd_bin
  wget https://releases.rocketboards.org/2024.11/zephyr/agilex5/hps_zephyr/hello_world/qspi_boot/qspi_flash_image_agilex5_boot.pfg
  sed \
    -e '/^[[:space:]]*<raw_files>/,/^[[:space:]]*<\/raw_files>/d' \
    -e '/^[[:space:]]*<partition[^>]*id="fip"[^>]*\/>[[:space:]]*$/d' \
    -e '/^[[:space:]]*<assignment[[:space:]]\+page="0"[[:space:]]\+partition_id="fip">/,/^[[:space:]]*<\/assignment>/d' \
    qspi_flash_image_agilex5_boot.pfg > qspi_flash_image_agilex5_sdmmc.pfg

  cp $TOP_FOLDER/ghrd_a5ed065bb32ae6sr0.sof .
  quartus_pfg -c qspi_flash_image_agilex5_sdmmc.pfg

  #create core.rbf
  quartus_pfg -c -o hps_path=bl2.hex ghrd_a5ed065bb32ae6sr0.sof fsbl.sof
  quartus_pfg -c fsbl.sof ghrd.rbf -o hps=ON -o hps_path=bl2.hex
```

This will generate **qspi_image.jic** and **ghrd.core.rbf**

- Build SD image<br>
```bash
  cd $TOP_FOLDER/sd_bin
  cp $TOP_FOLDER/freertos-socfpga/tools/make_sdimage.sh .
  mkdir fatfs &&  cd fatfs
  cp ghrd.core.rbf fatfs/core.rbf
  ./make_sdimage.sh -s 128 -o sd.img -p "a2:2:64:fip.bin" -p "c:1::fatfs/core.rbf"
```
The following output files are generated from the above steps:
- **sd.img** <br>
- **qspi_image.jic**

## 5.3 eMMC boot
- Apply the below patch to select eMMC as boot source
```bash
  cd $TOP_FOLDER/arm-trusted-firmware
  sed -i 's/MMC_DEVICE_TYPE[ \t]*1/MMC_DEVICE_TYPE 0/'  plat/intel/soc/agilex5/include/socfpga_plat_def.h
```
- Build ATF
```bash
  cd $TOP_FOLDER/arm-trusted-firmware
  make PLAT=agilex5 clean
  ARCH=arm64 CROSS_COMPILE=aarch64-none-elf- make PLAT=agilex5 bl2 bl31 PRELOADED_BL33_BASE=0x82000000 -j${nproc}
```
- Create FIP binary.
```bash
  cd $TOP_FOLDER
  rm -rf emmc_bin && mkdir emmc_bin && cd emmc_bin
  cp $TOP_FOLDER/arm-trusted-firmware/build/agilex5/release/bl2.bin $TOP_FOLDER/emmc_bin
  aarch64-none-elf-objcopy -v -I binary -O ihex --change-addresses 0x0 bl2.bin  bl2.hex
  $TOP_FOLDER/fiptool create --soc-fw $TOP_FOLDER/arm-trusted-firmware/build/agilex5/release/bl31.bin  --nt-fw $TOP_FOLDER/freertos-socfpga/build/hello_world/freertos_hello_world.bin fip.bin
```
- Create qspi jic to boot from eMMC
```bash
  cd $TOP_FOLDER/emmc_bin
  cp $TOP_FOLDER/ghrd_a5ed065bb32ae6sr0.sof .
  wget https://releases.rocketboards.org/2024.11/zephyr/agilex5/hps_zephyr/hello_world/qspi_boot/qspi_flash_image_agilex5_boot.pfg
  sed \
    -e '/^[[:space:]]*<raw_files>/,/^[[:space:]]*<\/raw_files>/d' \
    -e '/^[[:space:]]*<partition[^>]*id="fip"[^>]*\/>[[:space:]]*$/d' \
    -e '/^[[:space:]]*<assignment[[:space:]]\+page="0"[[:space:]]\+partition_id="fip">/,/^[[:space:]]*<\/assignment>/d' \
    qspi_flash_image_agilex5_boot.pfg > qspi_flash_image_agilex5_sdmmc.pfg

  quartus_pfg -c qspi_flash_image_agilex5_sdmmc.pfg

  #create core.rbf
  quartus_pfg -c -o hps_path=bl2.hex ghrd_a5ed065bb32ae6sr0.sof fsbl.sof
  quartus_pfg -c fsbl.sof ghrd.rbf -o hps=ON -o hps_path=bl2.hex
```
This will create **qspi_image.jic** and **ghrd.core.rbf**

- Build eMMC image<br>
```bash
  cd $TOP_FOLDER/emmc_bin
  cp $TOP_FOLDER/freertos-socfpga/tools/make_sdimage.sh .
  mkdir fatfs &&  cd fatfs
  cp ghrd.core.rbf fatfs/core.rbf
  ./make_sdimage.sh -s 128 -o sd_emmc.img -p "a2:2:64:fip.bin" -p "c:1::fatfs/core.rbf"
```
The following output files are generated from the above steps:
- **sd.img** <br>
- **qspi_image.jic**

# 6. Writing the QSPI flash image

- Configure MSEL to JTAG
- Power cycle the board.
- Write the image using the below command
```bash
  #use the image qspi_image.jic
  quartus_pgm -c 1 -m jtag -o "piv;qspi_image.jic"
```

# 7. Flashing the sd image to SD card
- Insert the SD card on to the host machine
- Use the below command to flash the image to the SD card <br>
```bash
  sudo dd if=sd.img of=/dev/sdx bs=1M
```
- Insert the SD card back onto the device and power cycle the device.

# 8. Flashing the eMMC image to eMMC
To flash eMMC image, refer to the [flash_emmc_image](https://altera-fpga.github.io/rel-24.2/embedded-designs/agilex-5/e-series/premium/boot-examples/ug-linux-boot-agx5e-premium/#boot-from-emmc)

NOTE: Sections **Build U-Boot**, **Create Helper JIC** and **Write eMMC Image** are relevant for FreeRTOS image.

## 8.1 Setting up TFTP server
- Install following packages on the host PC
```bash
  sudo apt-get update && sudo apt-get install xinetd tftpd tftp
```
- Create the file /etc/xinetd.d/tftp with the following content:
```bash
service tftp
{
     protocol = udp
     port = 69
     socket_type = dgram
     wait = yes
     user = nobody
     server = /usr/sbin/in.tftpd
     server_args = /tftpboot
     disable = no
}
```
- Create directory /tftpboot/ (this matches the server-args above) and set its permissions:
```bash
  sudo mkdir /tftpboot/
  sudo chmod -R 777 /tftpboot/
  sudo chown -R nobody /tftpboot/
```

- Restart the xinetd service:

```bash
  sudo service xinetd restart
```

- TFTP server is ready
