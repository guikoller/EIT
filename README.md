# EIT — Electrical Impedance Tomography on STM32F769

> **Demonstration**
> <!-- TODO -->

## Overview

This project implements a real-time **Electrical Impedance Tomography (EIT)** system on the [STM32F769I Discovery kit](https://www.st.com/en/evaluation-tools/32f769idiscovery.html). EIT is a non-invasive imaging technique that reconstructs the internal conductivity distribution of an object from boundary voltage measurements obtained by injecting small AC currents through surface electrodes.

Key features:
- **16-electrode** adjacent-drive measurement protocol
- **Linear Back-Projection (LBP)** reconstruction algorithm running entirely on the MCU
- Reconstruction grid configurable at 16×16, **32×32** (default), or 64×64 pixels
- Real-time colormap display on the board's **4-inch capacitive touch LCD** (LVGL UI)
- Simulated acquisition mode for offline testing without hardware electrodes
- Dataset recording to **SD card** in JSON format
- **WiFi streaming** of live frames and full exam sessions to the companion Flask server
- Sensitivity matrix pre-computed offline (MATLAB/EIDORS) and stored on the SD card

## Repository Structure

| Folder | Description |
|---|---|
| `Firmware/` | C firmware for the STM32F769I-DISCO (STM32CubeIDE project) |
| `Validation/` | MATLAB scripts for batch validation of reconstruction quality (EIDORS) |
| `FIPS Data/` | Python utilities for converting and processing raw measurement datasets |

---

## Hardware

The [STM32F769I Discovery kit](https://www.st.com/en/evaluation-tools/32f769idiscovery.html) provides:
- STM32F769NIH6 microcontroller — 2 MB Flash, 512 KB RAM (BGA216)
- 4-inch capacitive touch LCD display (MIPI-DSI, 800×480)
- 128-Mbit SDRAM
- 512-Mbit Quad-SPI Flash
- microSD card connector
- USB OTG HS, Ethernet, Arduino Uno V3 connectors
- On-board ST-LINK/V2-1

---

## Getting Started

### Firmware (STM32CubeIDE)

1. **Clone** the repository (including submodules):
   ```bash
   git clone github.com/guikoller/EIT
   ```

2. **Open STM32CubeIDE** and import the firmware project:
   - Go to **File → Import → General → Existing Projects into Workspace**
   - Set the root directory to `Firmware/`
   - Make sure the project is checked and click **Finish**

3. **Build** the project:
   - Select the `Debug` or `Release` build configuration from the toolbar
   - Press **Ctrl+B** (or **Project → Build Project**)

4. **Flash and debug**:
   - Connect the STM32F769I-DISCO board via USB (ST-LINK port)
   - Press **F11** (or **Run → Debug**) using the provided launch configuration `TIME_Firmware Debug.launch`
   - The firmware starts automatically after flashing

---

## SD Card Setup

The MCU loads datasets and the sensitivity matrix from the SD card root. Follow the steps below to prepare it before first use.

### Step 1 — Convert `.mat` files to `.bin`

The firmware reads datasets in a packed binary format (`.bin`). The conversion script reads MATLAB `.mat` files from `FIPS Data/data_mat_files/` and writes `.bin` files to `FIPS Data/data_bin_files/`.

```bash
cd "FIPS Data"
pip install numpy scipy        # one-time
python convert_eit_to_bin.py
```

The script converts all `.mat` files found in `data_mat_files/`. The reference file (`datamat_1_0.mat`) must be present — it is used as the reference (homogeneous) measurement for calibration.

> **Alternative:** If you already have pre-converted `.bin` files, skip this step and go directly to Step 2.

### Step 2 — Copy files to the SD card

Copy the entire contents of `FIPS Data/data_bin_files/` to the **root** of the microSD card:

```
SD:/
├── datamat_1_0.bin      ← reference dataset (required for calibration)
├── datamat_1_1.bin
├── datamat_1_2.bin
└── ...
```

Insert the SD card into the STM32F769I-DISCO before powering on.

### Step 3 — Generate the sensitivity matrix on the MCU

The sensitivity matrix is computed **on the MCU** from the reference dataset and saved back to the SD card as `sensitivity_matrix.bin`. This only needs to be done once (or whenever the electrode geometry changes).

1. Boot the board and wait for the **Home** screen to appear.
2. Tap **Settings** (bottom-right of the navigation bar, or the Settings button on the Home screen).
3. On the Settings screen, tap **Generate Sens. Matrix** (Calibrate button).
4. The status label updates in real time:
   - `SENS OPENING` → `SENS READING` → `SENS PATTERNS` → `SENS GENERATING` → `SENS WRITING` → **`SENS DONE`**
5. When `SENS DONE` is shown, `sensitivity_matrix.bin` has been written to the SD card root. The button re-enables and the system is ready for reconstruction.

> If the status shows `SENS ERR NO SD`, the SD card was not detected. Eject, re-insert, and retry.

---

## Generating a Reconstruction Image

Once the SD card is prepared and the sensitivity matrix exists, follow these steps to produce a live EIT image:

1. **Home screen** — Tap **Start** (or the EIT icon in the nav bar). The SD file browser opens.
2. **Browser** — A list of `.bin` dataset files on the SD card is shown. Tap a file to select it (e.g. `datamat_2_1.bin`).
3. **Reconstruction viewer** — The selected dataset is loaded and reconstruction runs automatically:
   - The LBP algorithm reconstructs a conductivity image from the voltage measurements relative to the reference frame.
   - The result is displayed as a colormapped square image on the LCD.
   - Use the **Play / Pause** controls to step through frames if the dataset has multiple injections.
   - Tap **Save BMP** to write the current frame as a bitmap to the SD card (e.g. `datamat_2_1_LBP.bmp`).
4. **Back** — Returns to the file browser to select another dataset.

### Reconstruction settings

On the **Settings** screen you can adjust:

| Setting | Options | Notes |
|---|---|---|
| Algorithm | LBP | Linear Back-Projection (only option currently) |
| Image size | 16 / **32** / 64 | Grid side-length in pixels; affects accuracy and speed |
| Show data table | On / Off | Overlays raw measurement values on the viewer screen |

---

## Validation (MATLAB)

Requires [EIDORS](https://eidors3d.sourceforge.net/) and MATLAB R2021b+.

```matlab
% Set EIDORS path, then run:
run_batch_validate
```

Or use the VS Code task **MATLAB: batch_validate**.
