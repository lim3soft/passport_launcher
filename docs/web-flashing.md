<p align="right"><a href="web-flashing.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Fresh browser installation — PassportOS 0.1.2

The owner has chosen a fresh installation: no existing app configuration needs to be backed up or exported. Use the Windows laptop and a browser flasher. The earlier Python capture/migration workflow is optional and is not required for this fresh installation.

Download `delivery/PassportOS-0.1.2-fresh-8MB.bin`. This is an **8,388,608-byte complete first-install image for the audited ESP32-C3 / 8 MiB configuration**, written at **0x0**. It replaces every flash location from `0x000000` through `0x7fffff`, including all existing applications and settings. Do not use it for ordinary app switching or updates when settings should survive.

The image preserves every byte of the verified 1,406,208-byte merged build and extends it with `0xff` bytes to the end of flash. This also initializes the future app slots, registry and private storage; writing only the smaller image does not clear those regions. It does not change eFuses. There is no separate erase step when writing this entire file.

## Install

1. On the Windows laptop, open the [official AI-Passport web flasher](https://ai-passport.folotoy.cn/en/tools/web-flasher/) in Chrome or Edge. Select **Install local firmware**. The official [development guide](https://ai-passport.folotoy.cn/en/guides/create-your-own-play/) documents local firmware installation.
2. Turn on the AI-Passport, connect it with a USB data cable, and close any other serial monitor/flasher. In the browser's device chooser, select this AI-Passport's port.
3. Choose `PassportOS-0.1.2-fresh-8MB.bin`. Use address **0x0** if an address field is shown. The target is **ESP32-C3, 8 MB**; stop if the reported hardware differs. Physical revision/capacity/security have not been checked on this host. Do not force an image past a chip, capacity, revision or security error.
4. Install the complete file and wait for successful completion/verification. Keep power and USB connected throughout. Restart the device after completion if the tool does not restart it.
5. Check that the PassportOS menu appears and UP/DOWN/OK work. Installed-app positions should be empty. Record the outcome before installing anything else.

If installation fails, retain the error and report it. Do not improvise wiring/BOOT sequences or modify eFuses. A successful host build is not physical acceptance.

## Install apps afterward

`counter-a.passport`, `counter-b.passport` and `qso-vision.passport` are separate packages in `delivery/`. On PassportOS choose **Install New App**, connect the phone to the displayed Wi-Fi access point, open `http://192.168.4.1` and upload a package. These packages are not raw images for the USB flasher. Test different values in Counter A and Counter B and confirm that switching preserves each value. QSO Vision is optional and is not built into the launcher.

## Verification

Fresh image SHA-256: `259c592d3beca37db0100452a80710dbe6ba0fddf33998d433d9ba0517cee7d5`.

The matching debug archive is `build/firmware/259c592d3beca37db0100452a80710dbe6ba0fddf33998d433d9ba0517cee7d5/`. Archive/ELF identity, layout, original image bytes and blank storage regions pass host checks. Firmware compilation and host tests passed for the underlying 0.1.2 build. **Windows flashing and physical-device tests: NOT RUN.** This host has not written a device. See [fresh-install evidence](fresh-install-results.json).
