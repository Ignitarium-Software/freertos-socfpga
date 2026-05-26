# RSU QSPI Sample

This sample demonstrates RSU (Remote System Update) and ROS (Remote OS Update) using images stored directly in the QSPI flash, without relying on an SD card or DDR-embedded binaries.

---

## Overview

Two images are required before programming the initial QSPI flash:

| Image | Description | Target QSPI Slot |
|-------|-------------|-----------------|
| `app2.rpd` | Application image (bitstream + BL2) | `SSBL.P2` |
| `fip2.bin` | SSBL image (ATF FIP binary) | `SSBL.P3` |

Both images are written directly into the QSPI flash as part of the initial flash programming step, and at runtime `librsu` reads them from those slots to update the active RSU/ROS slots.

---

## Flash Layout (defined in `initial_image.pfg`)

The `.pfg` file programs the following QSPI partition layout:

| Partition | Address | Role |
|-----------|---------|------|
| `FACTORY_IMAGE` | `0x00210000` | Factory bitstream (always present) |
| `P1` | `0x01000000` | Application slot 0 |
| `P2` | `0x02000000` | Application slot 1 (RSU target) |
| `SSBL.P1` | `0x03C00000` | SSBL for P1 (active `fip.bin`) |
| `P3` | `0x05000000` | Application slot 3 |
| `SSBL.P2` | `0x06000000` | Stores `app2.rpd` — source for RSU update |
| `SSBL.P3` | `0x07000000` | Stores `fip2.bin` — source for ROS update |

`SSBL.P2` and `SSBL.P3` are used as buffer slots: the pfg writes the update images into them at flash programming time, and librsu reads from them at runtime.

---

## Initial Flash Programming

Use Quartus Programming File Generator with the provided `initial_image.pfg` to create and write the initial QSPI image:

```bash
quartus_pfg -c initial_image.pfg
```

The pfg requires the following files to be present alongside it:

- `ghrd_a5ed065bb32ae6sr0.sof` — bitstream
- `bl2.hex` — BL2 (HPS early bootloader)
- `fip.bin` — active SSBL for P1
- `app2.bin` — `app2.rpd` renamed to `.bin` (application image source, placed into `SSBL.P2`)
- `fip2.bin` — SSBL update image (placed into `SSBL.P3`)

> CMake copies `bin/app2.rpd` → `app2.bin` and `bin/fip2.bin` into the `qspi_atf_binaries/` build directory automatically. `app2.rpd` is also regenerated from the SOF during the QSPI image build step.

---

## Runtime Behaviour (librsu)

At runtime the sample application (`rsu_sample.c`) performs the following steps using `librsu`:

1. **Init** — calls `librsu_init("")` to initialise librsu over the QSPI flash handle.
2. **RSU update** — reads the application image from `SSBL.P2` via `flash_read_sync`, erases slot `P2`, programs it with `rsu_slot_program_buf`, and verifies it.
3. **ROS update** — reads the SSBL image from `SSBL.P3` via `flash_read_sync`, erases the corresponding SSBL slot, programs it with `rsu_slot_program_buf_raw` (raw mode for FIP binaries), and verifies it.
4. **Reboot** — calls `rsu_slot_load_after_reboot` to request the updated slot on next boot.

The key slot macros in `rsu_sample.c`:

| Macro | Value | Meaning |
|-------|-------|---------|
| `RSU_SLOT` | `1` | Destination slot for app image (`P2`) |
| `RSU_SRC_SLOT` | `4` | Source slot for app image (`SSBL.P2`) |
| `ROS_SLOT` | `4` | Destination slot for SSBL image (`SSBL.P2`) |
| `ROS_SRC_SLOT` | `3` | Source slot for SSBL image (`P3` / `SSBL.P3` by name) |

---

## Prerequisites

- A QSPI flash programmed with the initial image generated from `initial_image.pfg`.
- The initial image must include at least the `SSBL.P2` and `SSBL.P3` slots populated with the update images.
- ATF version 2.12.0 or later.
- The bitstream `hps_path` in the pfg must point to `bl2.hex` (not `u-boot-spl-dtb.hex`).

> **Important:** The files `app2.rpd` and `fip2.bin` must be present in the `samples/rsu/rsu_qspi/bin/` directory before building. CMake copies these files into the `qspi_atf_binaries/` build directory during configuration — if either file is missing the build will fail.

---

## Build

Follow the common build instructions in the top-level README. The QSPI variant enables `FREERTOS_LIBRSU` and `FREERTOS_FATFS` and generates a QSPI JIC image via `add_qspi_image`.

```bash
cmake -B build samples/rsu/rsu_qspi
cmake --build build --target qspi-image
```

The final JIC image is written to QSPI flash using the Quartus programmer.
