# DM CAN Control GUI

Qt 6 GUI app for Damiao USB-CANFD devices. It sends control frames (0x3FE/0x4FE) with four int16 values and displays received motor feedback (0x301-0x308).

## OS support

- SDK provides Windows and Linux binaries only.
- No macOS binary is included in the SDK, so this project targets Windows and Linux.

## Directory layout

- `sdk/include` — SDK headers
- `sdk/lib/windows` — SDK DLL + import libs
- `sdk/lib/linux` — SDK .so
- `app/src` — Qt 6 GUI source

## Build

### Windows (MSVC)

```bash
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

### Windows (MinGW)

```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

### Linux

```bash
sudo apt update
sudo apt install qt6-base-dev qt6-base-dev-tools qt6-tools-dev qt6-tools-dev-tools libusb-1.0-0-dev
cmake -S . -B build
cmake --build build
```

## Run

The SDK library is copied next to the executable during build. Connect the device, then run the app from the build directory.

## Packaging

```bash
cmake --build build --target package
```

This produces a ZIP on Windows and TGZ on Linux in the build directory.

## GitHub Actions

Workflow: `.github/workflows/package.yml`

- Manual trigger: Actions tab -> package
- Tag trigger: push a tag like `v0.1.0`

## Notes

- Default baud: arbitration 1,000,000; data 5,000,000.
- Channel defaults to 0. For dual devices, select channel 1 if needed.
