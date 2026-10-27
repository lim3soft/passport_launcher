<p align="right"><a href="validation.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Validation, migration and device acceptance

Actual build identities and check results are recorded in `validation-results.json`. Build/debug bundles are content addressed under `build/firmware/`. `Build`, `Host tests`, `Device tests` and `Unverified` must be reported independently. This prototype has not been flashed or accepted on hardware.

## Automated checks

`bash tools/validate.sh` compiles and runs registry/ticket tests and the actual production installer against I/O fault stubs under AddressSanitizer/UBSan, runs package corruption tests, builds with ESP-IDF 5.5.3, merges the provisioning image, checks partition-table MD5, offsets, bounds, non-overlap, configured images and ELF binding, then archives matching artifacts. LeakSanitizer is disabled because this environment's tracing prevents it from running; address and undefined-behavior checks remain enabled.

Installer tests fail each receive/save/erase/write/validation-read boundary, verify that durable INSTALLING precedes destructive writes, keep other app/private partitions intact, reject wrong-chip/digest/ABI images, and exercise retained versus deleted data. Mock SDK image validation and SHA are intentional in those tests; independent package tests verify real image checksum/SHA. None establishes physical NVS atomicity, flash timing or power integrity.

The gate is adapted for this new project from the existing FoloToy image verifier/archive workflow. Final defaults and pinned dependencies must be checked against the built sdkconfig. The archive includes OTA initialization bytes as well as bootloader/table/app/full image, flash arguments, ELF and MAP. Generated QSO/counter packages must be independently checked by `tools/package_app.py verify`. Example project bootloaders and tables are build intermediates and are never installation payloads.

## First installation is a migration

The existing layout's 24 KiB default NVS overlaps the new OTA selector. The merged provisioning image pads gaps, and writing it at `0x0` can reset settings. Existing code may also occupy future app slots. Therefore segmented flashing alone does not establish compatibility or clean empty slots.

Before any approved flash, establish the exact board/port, JEDEC flash capacity and current partition layout; identify settings that must survive. QSO stores volume/talkgroup/dimming/brightness and Wi-Fi profiles in the shared `monitor` namespace (including `pN_s`, `pN_p`, `wifi_last_ssid` and legacy Wi-Fi keys). Its in-memory history is not a persistent flash database. Export app records using a supported, verified tool while the old layout is still usable, and verify import into the same-ID private partition. The data-only export/staging/retry implementation is supplied and host-tested in 0.1.1; see [migration](migration.md). **Reading and restoring a real device have not been validated.** Do not claim existing QSO settings will survive initial provisioning.

No original-firmware readback or full-flash backup is made a prerequisite. User data export is different from firmware backup. No full-chip erase is routine or authorized. After a concrete migration/export plan and explicit permission, provision the exact verified image and initialize only reviewed, intended ranges. Avoid flashing a stale build, an app-only image at `0x0`, or an example table. Existing PassportOS app/data positions should be preserved during launcher maintenance with a compatible segmented update; that is not a guarantee for the first layout conversion.

## Physical acceptance matrix

| Check | Required result |
| --- | --- |
| Identification | Correct C3 revision/8 MiB JEDEC capacity, USB enumeration and real board pin mapping |
| Fresh boot | Menu is default, correct 240×320 rendering, rounded corners/byte order, all buttons, battery fallback |
| Phone installation | Offline phone connects to displayed AP credentials; uploads valid package; service closes on leaving Install |
| Validation | Wrong chip, merged/oversize/truncated/corrupt/ABI-mismatched images rejected; no protected data touched |
| Independent execution | Counter A, Counter B and adapted QSO launch as separate firmware; physical reboot always reaches launcher |
| Data ownership | Counters using identical keys remain distinct; QSO profiles/settings survive switching; same-ID update/reinstall preserves them |
| User removal | Ordinary remove retains settings/reservation; explicit app-and-settings delete frees only the chosen ID |
| Return actions | Counter hold OK and QSO Options double OK save and return; peripheral state reinitializes correctly |
| Auto start | Off after provisioning; five-second cancellation consumes entire button gesture; app reset/crash skips auto; healthy acknowledgement enables future clean power-on startup |
| Power interruption | Cut power before/after metadata commit, during erase/write, verification and selection; launcher/other apps remain recoverable; incomplete app retry succeeds |
| Metadata failures | Invalid/missing metadata with residual app/private storage shows recovery and never formats data automatically |
| Stress | Repeated upload/switch cycles; measure stack high-water marks, total/largest heap block, battery behavior, network timeouts and watchdogs |
| Last-resort recovery | Verify actual USB ROM recovery access without inventing GPIO0/BOOT wiring; no eFuse changes |

If USB is available, discover devices read-only and request approval for the exact artifact and its data impact before flashing. A connected device or successful build is not authorization. If this environment cannot access the user's USB bus, perform local flashing only after the same review and collect device logs/user observations; do not report hardware PASS from logs that show only successful download.

The firmware gate generates a fresh isolated sdkconfig from defaults and retains it as `build/validated.sdkconfig`, leaving local root settings alone. The physical power button is outside the MCU input API; apps must commit settings as they change, rather than rely on a shutdown callback.

Version 0.1.2 also runs the combined manager/SDK/boot [lifecycle harness](lifecycle.md), with a persistent host backend and real OpenSSL SHA, and tests the startup controller at input/deadline boundaries. These tests use non-executable protocol fixtures; they do not run ESP32 apps or prove real NVS page recovery.
