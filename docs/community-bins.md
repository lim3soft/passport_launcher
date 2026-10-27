<p align="right"><a href="community-bins.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Passport Launcher 0.2.0 — community BIN import

> Historical fixed-slot guide: for current 0.4.0 capacity and SDK compatibility, see [shared storage](shared-storage.md). Custom-partition dependency checks still apply.

**Current interface: 0.3.1.** Follow [the interface guide](interface.md) for BIN-only installation, full uninstall, Auto start and About. This 0.2.0 architecture record describes the earlier package picker and startup restrictions.

The device owner reports successfully booting the previous 0.1.2 launcher. That confirms its menu boot on their device; it does not establish acceptance of the new 0.2.0 importer, networking or unmodified community apps.

## Use the launcher

The product is now named **Passport Launcher**. Choose **Install New App**, connect the phone or laptop to Wi-Fi **Passport Launcher**, password **aipassport**, then open **http://192.168.168.1**. The AP gateway is `192.168.168.1/24`; DHCP uses this subnet. The hotspot exists only while installation mode is open.

Select a `.bin` or an existing `.passport` package. For a BIN, the browser checks its image and shows a suggested app name and stable app ID. Use a unique ID per app, and reuse the same ID for updates. Filename version suffixes are removed from suggested IDs where recognized; check the ID before updating. Native SDK declarations keep their original ID.

**Install / update** installs the app into a bootable slot without starting it. Hold OK on the device to leave installation mode, select the installed app, then press OK to boot it. An unmodified community app returns to the launcher on a power cycle or reset; no new in-app button behavior can be added to a compiled BIN. Apps using the launcher SDK can retain their existing return action. Auto-start still requires the SDK's healthy acknowledgement; an unmodified BIN cannot provide it.

## Accepted files and limits

- ESP32-C3, DIO / 80 MHz / 8 MB image headers, appended SHA-256, valid ESP segment/checksum/hash structure. Community firmware supports the ESP-IDF 5.5 series (`v5.5`, `v5.5.x`, optional build suffix). Native ABI 1 SDK packages retain their ESP-IDF 5.5.3 requirement.
- App-only BIN files, or standard AI-Passport full BINs with the original `nvs` / `phy_init` / `factory` table, valid partition-table MD5 and blank initial NVS/PHY regions. Full flash padding is accepted when blank.
- The browser extracts only application code from a full BIN. It sends an internal transport to the existing streaming installer. The uploaded bootloader, partition table and unused flash are never installed. This does not require the user to convert or rename a community BIN.
- **Three positions, 2,097,152 application bytes per position.** A merged BIN can be larger because it includes a bootloader and gaps; the extracted application must fit. Registry/data reservations still occupy a position after ordinary removal.
- Custom resource/filesystem/data/recovery partition tables, nonblank initial data, signed or encrypted image formats and oversized applications are rejected. App-only BINs do not carry a partition table: their custom partition dependencies cannot be proven absent from image headers. Trust the source and test each app on the device. Identical ESP-IDF versions alone do not establish compatibility.

This uses ESP32's [bootable application/OTA mechanism](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32c3/api-reference/system/ota.html); it does not run multiple applications concurrently or provide a process sandbox. A firmware can access shared flash. No arbitrary binary can be guaranteed safe.

## The three supplied files

Host checks bind these results to the hashes in [the input report](community-input-results.json). “Pass” means parsing, integrity, installer and modeled context/boot selection, **not ESP32 execution**.

| File | Extracted application | Result |
| --- | ---: | --- |
| `QSO-Vision_latest2.3.3.bin` | 1,828,608 bytes | Pass; standard merged image, ESP-IDF 5.5.3 |
| `Internet-Radio.bin` | 1,889,520 bytes | Pass; app-only image, descriptor reports ESP-IDF 5.5 |
| `pokemon.bin` / PokeWalk | 3,229,280 bytes | Blocked; exceeds a slot and requires custom partitions |

PokeWalk expects `cardid`, `save_restore`, `wifi_config`, `pokewalk_store` and `recovery`, including addresses overlapping launcher app slots. Ignoring its table would risk settings/data corruption. Its source needs a storage port and capacity plan. A larger-slot/two-app layout is one possible alternative after that port, with a deliberately different partition layout. It is not implemented in 0.2.0. All three application payloads alone total **6,947,408 bytes**, exceeding the current **6,291,456-byte application area** before PokeWalk's extra data/recovery space. They cannot all be retained in this layout unchanged.

## Settings and recovery

Native SDK apps continue using 128 KiB private NVS partitions. Unmodified community apps use the runtime table's **16 KiB default NVS**, as described by the [ESP-IDF NVS partition API](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32c3/api-reference/storage/nvs_flash.html). Their default NVS bytes are restored from their own 128 KiB private area before boot and captured after returning to the launcher. This is not a 128 KiB default NVS allocation for legacy apps.

The context journal runs before launcher default-NVS initialization. RESTORING retains the old snapshot after an interrupted restore; ACTIVE captures the application's latest bytes; SAVED prevents recapturing a partially cleared default NVS; IDLE permits launcher radio initialization. Snapshot ID and SHA-256 checks protect accidental reassignment/corruption. Mode metadata stays separate from the version-1 registry so previously packaged ABI 1 apps can read it. A missing mode cannot silently boot a community image as a native SDK app. Changing storage mode under a retained ID is refused; deleting the app **and settings** frees it for a different mode.

Full default-NVS copies add flash writes at switching. Neither the journal nor hashes constrain arbitrary raw flash access by an application. Host tests model before/after I/O failures and partial raw erase/write effects, not real NVS page recovery or physical power cuts. Physical switching, settings persistence, RF/heap/stack and power-cut behavior remain pending.

## Upgrade from 0.1.2

Use the supplied **Passport-Launcher-0.2.0-full.bin** with the [official browser flasher](https://ai-passport.folotoy.cn/en/tools/web-flasher/) at **0x0**. Leave whole-device clear/erase disabled for an update. The partition table is unchanged; the image ends before the first app slot at `0x190000`, preserving installed code, metadata and private data. The merged image resets launcher default NVS and replaces its bootloader/table/application region. This is a launcher firmware update, not a `.bin` to install as another app inside the launcher. Do not use the old 8 MB fresh-install image for a preserving update.

After updating: confirm the new name, SSID/password and address; upload the original QSO and Internet Radio BINs; boot each and configure different settings; power-cycle between them and verify retention. Confirm Pokémon is rejected before upload. The files have not been flashed or executed by this host. Build and host checks are separate from device acceptance.

Internal SDK symbols, IDs and the build/archive name `PassportOS` remain stable for compatibility; the display, web page, hotspot and delivered firmware use Passport Launcher.
