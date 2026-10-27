<p align="right"><a href="windows.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Windows 11 handoff — PassportOS 0.1.2

**Current owner preference: fresh installation without preserving settings.** Use the [browser flashing guide](web-flashing.md) instead. The capture/export steps below remain optional for users who want legacy settings migration.

Your AI-Passport connects to your Windows laptop. This host cannot access it. The firmware and host tests pass; the helper has been tested with simulated device responses on Linux, **not yet on Windows or a physical board**. QSO Vision is a separate `.passport` app; the launcher contains its settings-import support, not the QSO radio/UI.

## 1. Extract and prepare

Extract the entire ZIP into a local folder you control. Open Windows Terminal/PowerShell in that folder. Use Python 3.11–3.13 (3.13 matches the development environment). If `py` is unavailable, use your existing ESP-IDF Python terminal or install Python from [python.org](https://www.python.org/downloads/windows/); tell us the error rather than changing drivers or security settings blindly.

```powershell
py -3 -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements-windows.txt
.\.venv\Scripts\python.exe tools\windows_bundle.py verify .
```

The isolated environment pins esptool 4.12.0 and leaves other flashing installations alone. These commands invoke its interpreter directly; no PowerShell activation script or execution-policy change is needed. See [Espressif's installation guide](https://docs.espressif.com/projects/esptool/en/release-v4/esp32/installation.html). `build-validation.json` retains build-host provenance paths; local bundle files are listed in `bundle-manifest.json`.

## 2. Identify the COM port first

Turn on AI-Passport, connect a USB **data** cable, close any flashing tool or serial monitor using the device, and run:

```powershell
.\.venv\Scripts\python.exe tools\windows_capture.py ports
```

This lists ports without opening them. Compare Windows Device Manager → **Ports (COM & LPT)** before/after connection to identify the correct device. Do not choose another connected board's port. The C3 native USB Serial/JTAG interface normally appears as a COM port, as described by [Espressif](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32c3/api-guides/usb-serial-jtag-console.html). Use your laptop's already-working driver setup; no driver installer is included.

## 3. Capture read-only evidence

Replace `COM5` with the identified port. Use a new output folder each run:

```powershell
.\.venv\Scripts\python.exe tools\windows_capture.py capture --port COM5 --output private\capture-001
```

The helper verifies the bundle, requires the pinned tool version, resets that explicit port into the ROM loader, and uses only `get_security_info`, `flash_id` and bounded `read_flash` commands. It disables RAM-stub upload and leaves the device in ROM mode between reads so the application cannot change settings during capture. Reads can be slower than normal flashing. It never writes flash/NVS/eFuses, erases a range, flashes a firmware image, or executes a migration plan.

It stops if security status is enabled/unclear, capacity is not 8 MB, or the silicon revision is outside the firmware header's supported range. It next reads and validates the old 4 KiB partition-table sector, then the 288-byte application header. **Only the audited QSO Vision 2.3.3 build and its old single-app table are supported automatically.** A different firmware/table stops the helper before it reads credentials. No full firmware backup is required.

For the supported combination it reads only the 24 KiB NVS region, rechecks the partition table, exports the 23 supported settings fields when present and generates a review-only migration plan bound to the supplied firmware. This is evidence for review, not permission to flash.

When done, power the device off/on with its hardware power button to resume its current firmware. A dark/stopped screen while it remains in the ROM loader is expected. If a connection fails, keep the error; check cable/port ownership rather than guessing a BOOT-button wiring sequence.

## 4. Share only the summary

Send **`private\capture-001\report-to-share.json`** or paste its contents. It contains status, COM port, chip/revision, capacity, security flags and hashes, with no MAC, serial number, SSID, password or raw logs. If capture stopped, share that summary/error first.

Keep `legacy-nvs.bin`, `qso.qso-settings`, raw connection logs and `migration-plan.json` private. The NVS/settings files contain Wi-Fi credentials. Do not upload the whole capture folder. On Windows, these files inherit folder permissions; this helper does not claim POSIX mode `0600` sets Windows ACLs.

## 5. Flashing and acceptance come after review

No automatic flashing command/script is included. First installation changes the old partition layout and can overwrite settings. The reviewed plan may initialize future app/data ranges, so it must match the actual device and preserved data before any write. After the device report and exact plan are reviewed, the next handoff can specify the exact approved image, ranges and Windows commands.

After an approved installation: confirm the menu, install Counter A and Counter B from `apps/` through the phone browser, save different counter values, alternate apps and confirm retention/return. QSO can then be installed separately; import its exported settings into its empty private storage before using it. Check auto-start cancellation and reset behavior. A successful download alone is not device acceptance; power-cut tests should follow basic restoration/switching acceptance on suitable test hardware.

`firmware/PassportOS-full.bin` is the provisioning image at `0x0`; `apps/*.passport` are packages for the running launcher and must never be flashed at `0x0`. Matching launcher debug artifacts are retained under `firmware/firmware/<sha256>/`. No personal capture or synthetic non-executable lifecycle fixture is included in this ZIP.
