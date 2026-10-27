<p align="right"><a href="interface.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Passport Launcher 0.4.1 interface

The owner confirmed that 0.2.0 installs BINs through the portal. Version 0.4.0 removes the community downloader, keeps the local-BIN-only portal, and replaces fixed slots with shared flash. The redesigned screen, management and About pages remain. New physical-device acceptance is still pending.

## Library and controls

The full 240×320 display now has a compact header/battery indicator, a scrollable card list and a control/status footer. Only installed, bootable ROMs appear in **Your library**. A larger bold **Passport Launcher** heading replaces Your library on the main menu. A nonselectable line separates ROMs from launcher actions, and the About row omits developer credit; the credit remains inside About. A count shows installed apps and remaining KiB instead of selectable empty slots. With no ROMs, an explicit empty-library card explains how to add one.

UP/DOWN moves the full-width rounded highlight; OK opens or launches the selected item. A selected ROM's entire title remains in its label and scrolls from right to left when it exceeds the available width. Polling, battery updates and countdown changes do not reset that animation. Lists scroll vertically to keep the selected card visible. Hold OK returns to the previous page.

Main actions are **Install a ROM**, **Manage apps**, **Auto start: On/Off**, and **About**. Installation instructions show the network, password and address in separate cards. Hotspot settings remain **Passport Launcher**, **aipassport**, and **http://192.168.168.1**.

## BIN-only portal

The application picker accepts `.bin` only. `.passport` and `.passportos` application files are rejected; the old public package installation endpoint is removed. Native SDK app-only BINs still work and keep their declared identity. Browser extraction, integrity checks and partition-dependency and image-integrity checks remain in place. Code allocations use shared free space; see [shared storage](shared-storage.md). The internal HTTP transport is not a user-facing ROM file format.

The portal displays total/free shared space and the largest new ROM that fits, including its settings reservation, then performs a 512-byte metadata-only allocation check before sending application code. Updates can reclaim their own code and preserve their private data. The actual installer rechecks all conditions under its lock. ROMs that do not fit, exhausted metadata entries and incompatible layouts also display their reasons on the device's installation screen. Validation notifications and preflight do not write app/NVS data.

The older SDK/package tools and prior delivery archives are retained for developer compatibility, not offered as another installation format. Optional legacy settings import remains separate from ROM installation.

The portal displays real upload percentage. After upload, an indeterminate bar says the device is installing/verifying; **Installation complete** appears only after its successful final response. A rejected preflight, HTTP error or interrupted/timeout connection never claims success. Upload controls are disabled during the operation. The old QSO legacy-import advice paragraph is removed; its optional compatibility action remains. About links to the developer's GitHub repository, and the portal footer credits 9W3SPY with a GitHub hyperlink.

## Uninstall and saved data

Manage apps lists installed ROMs and unfinished entries that need cleanup. There are no empty-slot actions. Selecting an app opens **Uninstall app?**, showing its full name and the warning **All saved data and settings will be permanently erased**. **Cancel is selected first**. UP/DOWN chooses Cancel or Uninstall; OK confirms. Cancel or hold OK leaves the app/data unchanged.

Confirming Uninstall removes both code and the complete private data partition, including saved default-NVS snapshots for community apps. The matching browser action uses the same semantics and warning; the old “remove but keep settings” action is no longer exposed. Both device and browser bind confirmation to the selected stable app ID, so a reused position cannot silently delete a different app. A partial failure keeps a non-bootable cleanup entry; select it to finish uninstalling and release its code and private-data space.

## Auto start

The default target is **Last booted app**. Open **Auto start → Startup app** to choose that default or pin any READY installed app. Choosing a target does not change On/Off. A fixed target persists through manual launches of other apps and ordinary updates, using its stable ID. Uninstalling that target returns the preference to Last booted app. Interrupted/missing targets do not auto-launch. The countdown highlights the actual selected startup app.

**On** resumes the configured installed app on the next clean power-on, after a **five-second countdown**. **Off** opens the library. The Auto start page retains the On/Off toggle, shows the full last-app name in a highlighted card, and explains the countdown. The startup screen repeats the highlighted name with a large timer and progress bar. **Any button cancels** and leaves the library open, focused on the last app when available. Queued cancellation input keeps priority at the deadline.

Community BINs now support this cold-start policy without requiring an SDK health callback. Native SDK apps retain their health acknowledgement requirement. Software reset, panic/watchdog reset and deep-sleep wake still enter the launcher without auto-start. Storage recovery must complete before a countdown can be armed. With no last app, On is stored but waits until an app has been launched.

## About and open source

About is available in the main menu and at the web portal's **About** link. It uses the QSO Vision credit treatment: small **Developed by** text above a prominent **9W3SPY** callsign. It displays the running app version, **ESP-IDF 5.5.3**, **ESP32-C3 / 8 MB**, the MIT application license, FoloToy board-support attribution, ESP-IDF/Apache-2.0 and LVGL/MIT notices, and **Inspired by M5 Launcher** with the upstream repository credit.

The portal serves the complete retained application/component notices at `/licenses.txt`, including the original FoloToy copyright/license. Deterministic gzip keeps the full text in the resident firmware without decompressing it into MCU RAM. The device directs users to this offline portal copy for full legal text. M5 Launcher is credited as design inspiration; its code has not been copied. The [license overview](third-party.md) and retained root [LICENSE](../LICENSE) remain authoritative.

## Update and verify

For a device already on 0.4.0, power-cycle, cancel Auto start and wait for the menu to capture the last app's default-NVS snapshot. Flash **Passport-Launcher-0.4.1-update.bin at 0x0** with **whole-chip erase OFF**. Its verified range ends before `0x190000`; app extents, private NVS and metadata are preserved, and the SDK 2/layout contract is unchanged. The merged prefix resets default/platform NVS. Earlier fixed-slot versions require a deliberate fresh migration instead of this update.

Check the full-row highlight and marquee, vertical navigation, BIN upload, Cancel retaining the app/data, confirmed uninstall of a test app, On/Off persistence, cold-start countdown/cancellation, and About on device and browser. Test uninstall using data you intentionally want to erase.

Host validation includes ten C executables with ASan/UBSan, 24 Python tests, browser/parser tests and supplied QSO/Internet Radio bytes through the production installer model. A 2.5 MiB fixture exercises actual production streaming/selection/context handling in the host backend, and a fourth app retains private data. Allocation tests cover 10,000 operations, exact capacity and fragmentation; boot tests cover interrupted table operations before/after writes. A separate native build uses pinned LVGL, RGB565 and a **48 KiB pool**, including the full 16-entry library, marquee, confirmation/countdown/About and page changes. Preview captures are host renders, not device screenshots. RF/heap/stack, real execution, endurance and electrical power-cut recovery remain unverified.
