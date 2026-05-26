# RSU Sample

This folder contains RSU (Remote System Update) sample applications for FreeRTOS on Agilex 5.

Three variants are available, selected at configure time via the `RSU_TYPE` CMake variable:

| `RSU_TYPE` | Description |
|------------|-------------|
| `rsu_ddr` | Images embedded into the executable via `objcopy` and loaded from DDR |
| `rsu_qspi` | Images pre-programmed into QSPI flash slots and read at runtime |
| `rsu_sdcard` | Images read from the FAT partition of an SD card at runtime |

---

## Building

Configure from the `samples/rsu/` directory, passing `RSU_TYPE` to select the variant:

```bash
cmake -B <build-dir> \
    -DCMAKE_BUILD_TYPE=Debug \
    -DSOC=AGILEX5 \
    -DCORE=A55 \
    -DRSU_TYPE=rsu_qspi \
    -DSOF_PATH=<path-to-sof> \
    -DPFG_QSPI=<path-to-pfg>
```

`RSU_TYPE` defaults to `rsu_ddr` if not specified. Valid values are `rsu_ddr`, `rsu_qspi`, and `rsu_sdcard`.

`-DPFG_QSPI` should point to the `initial_image.pfg` for the chosen variant (e.g. `samples/rsu/rsu_qspi/initial_image.pfg`). This overrides the default pfg path in the variant's CMakeLists.

For build targets (`sd-image`, `qspi-image`, ninja usage, etc.) refer to the [samples README](../README.md).

---

## Variant-specific notes

- **rsu_ddr** — See [rsu_ddr/readme.md](rsu_ddr/readme.md) for details on the required pre-built object files in `obj/`.
- **rsu_qspi** — See [rsu_qspi/readme.md](rsu_qspi/readme.md) for details on the required binary files in `bin/` and the QSPI flash layout.
- **rsu_sdcard** — Images (`app2.rpd`, `fip2.bin`) must be present on the FAT partition of the SD card before running.
