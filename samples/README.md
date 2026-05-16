## Sample APPs for drivers
This folder contains various sample applications for all the available drivers in this platform

**1. Building the app**

The project uses cmake to setup the build<br>

Each sample application needs to be built from the respective project folder.

**1.1 Building using standard make build system**

Configure cmake build. Specify the desired build directory.
```bash
cmake -B <build-dir> . -DCMAKE_BUILD_TYPE=Release -DSOC=AGILEX5 -DCORE=A55 -DSOF_PATH=<path to .sof file> -DPFG_SDMMC=<.pfg for sd/mmc> -DPFG_QSPI=<.pfg for qspi>
```
Build .elf alone
```bash
cmake --build <build-dir>
```
Build SD card image
```bash
cmake --build <build-dir> -t sd-image
```
Build QSPI image
```bash
cmake --build <build-dir> -t qspi-image
```
**1.2 Building using ninja build system**

Configure cmake build
```bash
cmake -B <build-dir> -G Ninja . -DCMAKE_BUILD_TYPE=Release -DSOC=AGILEX5 -DCORE=A55 -DSOF_PATH=<.sof file> -DPFG_SDMMC=<.pfg for sd/mmc> -DPFG_QSPI=<.pfg for qspi>
```
Build .elf alone
```bash
ninja -C <build-dir>
```
Build SD card image
```bash
ninja -C <build-dir> sd-image
```
Build qspi image
```bash
ninja -C <build-dir> qspi-image
```

**2. Notes**

**2.1 I2C DMA sample buffer requirements**

For DMA-mode I2C samples, write buffers must be 32-bit DATA_CMD words with any
required STOP/command bits already applied by the caller. DMA read buffers and
all interrupt-mode buffers remain byte arrays.
