<p align="right"><a href="application-contract.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Application contract, SDK ABI 2

Passport Launcher 0.4.0 uses the shared-storage registry, SDK ABI 2 and a 32-byte one-use RTC ticket. SDK 1 app binaries target a different address/registry contract and must be rebuilt. Existing community BINs remain a separate import mode with saved default-NVS contexts. The portal's only application file format is `.bin`; internal package headers and Python package tools are developer transport/testing mechanisms.

## Identity and private storage

Use a permanent ID of 1–63 ASCII characters: initial lowercase `a-z`, then `a-z0-9._-`. Display names and versions can change without changing the ID. Add `PASSPORT_DECLARE_APP("org.example.app")` once to retain the `PASSPORTOS_ABI2:` declaration. It is a compatibility claim, not a publisher signature.

`passport_app_open(PASSPORT_APP_ID, namespace, mode, &handle)` loads the v2 registry, matches the declared ID to a READY record and verifies the exact running image address/capacity. The temporary boot table presents the selected app as OTA 0 regardless of its registry index; applications must not derive identity or storage from the OTA subtype. The helper opens only that record's named private NVS, whose address remains stable during code updates. Applications may reuse namespaces/keys through this API. A private allocation is 128 KiB including NVS overhead, not 128 KiB of guaranteed record payload.

Keep Wi-Fi profiles, progress, preferences and app calibration private. Use `WIFI_STORAGE_RAM` and load selected credentials from private NVS. Default `nvs_open` uses shared platform NVS and is not a native private-store API. Community mode separately saves/restores the 16 KiB default-NVS region before and after an unmodified app; native SDK 2 apps should not bypass the private API. Additional BLE persistence, filesystem or resource partitions need an audited integration.

Do not write the physical partition table, shared metadata, another app's storage or absolute flash addresses. Do not run a firmware's built-in full-device updater under the launcher; install application updates through the launcher portal. All firmware still has native privileges, so this is cooperative isolation rather than a security sandbox. Only the launcher owns registry changes except the SDK's health acknowledgement.

## Build and lifecycle

Build for the verified ESP32-C3, DIO, 80 MHz, 8 MiB flash and ESP-IDF 5.5.3 using the retained BSP initialization and display/button drivers. Include the current `passport_core` and `passport_sdk` components and enable `CONFIG_BOOTLOADER_CUSTOM_RESERVE_RTC=y`, `CONFIG_BOOTLOADER_CUSTOM_RESERVE_RTC_SIZE=0x20`, with custom RTC excluded from the retained-memory CRC. Do not treat the application's build-time partition CSV as the physical device layout; the launcher's bootloader supplies the runtime table.

Call `passport_app_mark_healthy(PASSPORT_APP_ID)` only after essential initialization succeeds. This clears the native pending-launch state and enables optional future cold-start resume. Before returning, save settings, close handles, stop app tasks/audio/network activity, then call `passport_return_to_launcher()`. It issues an ordinary reset without a fresh launch ticket, so the bootloader repairs/selects factory. Community apps can return through a power-cycle or normal reset; cold-start Auto start remains cancellable.

The installer validates image integrity, chip/flash/SDK settings, stable identity and allocation before making the record READY. It never auto-launches an upload. New private data is initialized before READY; updates preserve initialized data. A failed update leaves the selected app non-bootable until repaired but does not erase unrelated applications/settings. Full uninstall releases both code and private data only after erasure.

## Space and compatibility

See [shared storage](shared-storage.md) for exact addresses, allocation, table recovery and flash-wear implications. Images are limited by actual available extents, per-app private-data reservations, alignment and validated image memory requirements. The pool totals 6.375 MiB, and the index supports up to 16 identities. There is no fixed 2 MiB application allocation and no assumption that arbitrary numbers of full firmware images fit.

Supported standard merged community BINs are extracted locally; only the checked application payload is transferred. Extra resource/filesystem/data partitions are rejected rather than silently omitted. A larger shared code extent does not make custom-layout Pokémon/app 1434 binaries compatible. The supplied original community QSO/Radio BINs pass host tests; old launcher-adapted SDK 1 QSO packages must be rebuilt with SDK 2 and are not bundled.

Host tests exercise the production SDK and manager with persistent byte/NVS backends, including fourth-app identity and a 2.5 MiB image. These are not ESP32 execution or RF/heap/power-cut/endurance measurements. Validate the new SDK and each app on the physical device before claiming acceptance.
