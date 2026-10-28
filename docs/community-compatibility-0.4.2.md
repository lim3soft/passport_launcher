# Community BIN compatibility in 0.4.2

The importer checks the file's image and partition table. There is no filename or project-name allowlist. The previous importer accepted only one exact merged partition table, so it refused otherwise valid community builds before uploading them.

## Supplied files

These are checks of the owner's exact BIN bytes, not ESP32 execution. Each file was installed separately into an empty host-modeled pool using the production manager and boot hook.

| Original file | Extracted code | Private allocation | Minimum total allocation | Host import/install/boot selection |
| --- | ---: | ---: | ---: | --- |
| `badminston score.bin` | 1,429,488 B | 204,800 B | 1,634,304 B | PASS |
| `pokemon.bin` | 3,229,280 B | 1,507,328 B | 4,739,072 B | PASS |
| `qiannian-jiazu-v3.4-full.bin` | 1,601,136 B | 180,224 B | 1,781,760 B | PASS |

The totals include rounded code, launcher private NVS, a boot-manifest sector and declared data/recovery partitions. Placement alignment/fragmentation can require additional free gaps. The three totals sum to **8,155,136 B**, beyond the **6,684,672 B shared pool**. They cannot all be installed together. The preflight checks the selected file against actual available extents before installation.

Badminton's code begins at `0x20000`, not `0x10000`, and requests 64 KiB NVS. 千年家族 requests `cardid` in addition to default NVS/PHY. Pokémon requests `cardid`, `save_restore`, `wifi_config`, `pokewalk_store` and a 1 MiB `recovery` application partition. Those partitions were absent from the supplied merged file, so their initial bytes are correctly treated as erased flash; their space is still reserved.

## What changed

- Parse a validated table instead of matching one hard-coded layout. Read the factory app at its declared offset. Check table MD5, bounds, overlap, labels, flags, app checksum and SHA-256.
- Relocate supported data and test/recovery partitions into the selected app's private extent. Retain their label, type, subtype, size and supplied initial content. The bootloader emits these entries before the app's ESP-IDF startup parses its table. Original absolute flash offsets are never used for imported writes.
- Retain the original snapshot path for app-only images and the previously supported standard merged table. This preserves updates to existing QSO Vision/Internet Radio installations. Nonstandard merged builds get their own declared NVS size directly, without the 16 KiB snapshot limit.
- Count all private extents in allocation, overlap, uninstall and capacity calculations. A versioned extension fits the existing 512-byte package/registry entry; existing 0.4.x entries remain readable. The 32-byte ticket also remains the same size.
- Initialize and verify new private bytes before READY. Preserve those bytes during updates. Changing an existing identity's private layout is refused; uninstall first if that change is intended. Confirmed uninstall erases every owned byte before releasing its allocation.
- Verify a stored boot manifest before issuing a ticket and bound its entries again in the bootloader. Consuming the ticket precedes table writes; the next reset reconstructs the permanent launcher table after a torn operation.

The [Espressif partition-table documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32c3/api-guides/partition-tables.html) describes the label/type/offset API and application alignment that this mapping uses.

## Boundaries

This supports partition-API-based firmware, not rewriting arbitrary machine code. Firmware that uses hard-coded raw addresses, overwrites its own bootloader/table, depends on another OTA boot chain, enables encrypted/signed firmware, or requires unavailable hardware can still require a source change. The importer rejects unsupported OTA layouts, encrypted/key partitions, reserved-label collisions, invalid images and insufficient space. Up to eight additional partitions fit the bounded manifest. No applications are bundled.

A declared recovery/test partition is retained, including any supplied valid image. A normal application reset or OTA selector change still returns to Passport Launcher; an app's separate recovery boot chain is not adopted. Returning to the launcher does not execute uploaded bootloaders.

Native SDK 2 applications built from older sources should be rebuilt with the current core/model when sharing a registry with the new extension. Older model readers reject nonzero reserved fields. After installing extended-layout apps, do not downgrade the launcher to 0.4.1 while retaining that registry.

Host tests cover production streaming, initial-content hashes and readback, relocated tables, full-size NVS/resources, updates, uninstall, invalid layouts, missing manifests, capacity and interruption/retry. They do not emulate ESP32 instructions, NVS page recovery, RF, physical flash wear or electrical power cuts. The owner reports this update is working on their device; the host did not independently test physical application execution.

## Update an existing 0.4.0/0.4.1 launcher

1. Power-cycle, cancel Auto start, and wait for the launcher menu to save the last app's default-NVS snapshot.
2. Use the browser/USB flasher to write `Passport-Launcher-0.4.2-update.bin` at **0x0**, with **whole-chip erase OFF**. This updates the launcher and its bootloader. The short image ends before `0x190000`; existing app allocations and metadata are preserved.
3. Open **Install a ROM**, connect to **Passport Launcher** / **aipassport**, and visit **http://192.168.168.1**. Reopen/reload the portal to get the updated importer. Upload the original community BIN, wait for confirmed completion, then select it on the device.

The separate `fresh.bin` clears the entire 8 MiB flash. Use it only for a deliberately fresh installation or old 0.1–0.3 migration. Neither launcher image belongs in its own app-upload portal.

To repeat the optional owner-file checks, set `PASSPORT_COMMUNITY_INPUTS` to a local input directory and run `bash tools/validate.sh --static`. Third-party firmware inputs are not redistributed in the repository.
