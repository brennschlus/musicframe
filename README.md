# Music Photo Frame (Nintendo 3DS)

A tiny 3DS project made for one thing only: **vibing in the moment**.

## Purpose

The full flow is intentionally simple:

1. Take a picture
2. Pick a simple filter and frame
3. Pick a music track
4. Enjoy your gorgeous ~0.3MP photo while listening to delightfully downsampled audio

This app is less about perfect quality and more about atmosphere.

## Build requirements

- [devkitPro](https://devkitpro.org/) with **devkitARM**
- 3DS libraries used by this project (via devkitPro): `libctru`, `citro2d`, `citro3d`
- `3dslink` (for Wi-Fi sending to a running 3DS homebrew loader)

Before building, make sure `DEVKITARM` is set (the Makefile enforces this):

```bash
export DEVKITARM=<path to>/devkitARM
```

## Build & Make commands

All commands are in the project `Makefile`.

- `make` or `make all`  
  Build the project and produce `musicframe.3dsx`.

- `make clean`  
  Remove build artifacts (`build/`, `.elf`, `.3dsx`, `.smdh`, generated gfx files).

- `make run`  
  Clean + rebuild + send over Wi-Fi using:
  ```bash
  3dslink musicframe.3dsx
  ```

- `make send`  
  Send current build over Wi-Fi without rebuilding:
  ```bash
  3dslink musicframe.3dsx
  ```

Yes, you can literally build and send to your 3DS over Wi-Fi.

## Music file location

Put audio files here on your SD card:

```text
sdmc:/3ds/musicframe/music/
```

The app scans this folder (non-recursive) and shows `.wav` files.

## WAV format requirements (important)

Your `.wav` files must be valid and reasonably small.

Accepted by the audio loader:

- RIFF/WAVE format
- **PCM only** (`audio_format == 1`)
- **16-bit** samples
- **1 or 2 channels** (mono or stereo)
- PCM data chunk must be **<= 8 MB**

If a file does not match these constraints, it will fail to load.

## Quick vibe checklist

1. Launch app on 3DS
2. Capture photo
3. Choose filter
4. Choose frame
5. Choose `.wav`
6. Press play and vibe
