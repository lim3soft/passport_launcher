<p align="right"><a href="migration.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# QSO settings migration, PassportOS 0.1.1

The data-only export/import implementation is host-tested. No real device settings have been captured, restored or verified in this environment. Use this workflow only for the audited QSO Vision 2.3.3 release and single-application layout; a physical migration still requires device identification, a reviewed plan and explicit flash approval.

## Export before changing the partition table

The original layout puts NVS at `0x9000`, length `0x6000`. PassportOS uses the last two of those sectors as OTA selection data. Read the **old partition table and NVS before any erase or firmware write**. A full firmware backup is not required. These examples are manual read operations, not commands already executed:

```bash
# ESP-IDF 5.5.3 terminal, with the correct board/port identified first.
mkdir -p private
umask 077  # POSIX shell: keep data reads private
python -m esptool --chip esp32c3 --port PORT read_flash 0x8000 0x1000 private/legacy-table.bin
python -m esptool --chip esp32c3 --port PORT read_flash 0x10000 0x120 private/legacy-app-header.bin
python -m esptool --chip esp32c3 --port PORT read_flash 0x9000 0x6000 private/legacy-nvs.bin
python3 tools/qso_settings.py export private/legacy-nvs.bin --output private/qso.qso-settings
python3 tools/qso_settings.py verify private/qso.qso-settings
```

`qso_settings.py` never opens a serial port or writes a device. It reuses the unchanged Espressif 5.5.3 NVS parser, checks page/entry/string CRCs and supported page state/version, and exports only the audited `monitor` keys. Conflicting live keys, unknown QSO keys, invalid values, transitional/corrupt pages, unsupported encryption or truncated reads are refused rather than guessed or discarded. An empty/missing monitor namespace has no supported nonempty migration export; review that situation separately.

The export includes talkgroup, volume, dimming, brightness, up to eight Wi-Fi profile pairs, legacy Wi-Fi keys and last-selected SSID when present. Other SDK namespaces and RAM-only QSO history are not imported. Raw Wi-Fi bytes are preserved without assuming UTF-8. The tool prints counts and hashes, never SSIDs/passwords. New output files use mode `0600` on POSIX and refuse overwrites. The NVS read and `.qso-settings` file contain credentials: retain them privately, outside source control. `private/` and `*.qso-settings` are ignored by this project.

## Create a concrete review plan

After a verified launcher build:

```bash
python3 tools/plan_qso_migration.py --table private/legacy-table.bin \
  --nvs private/legacy-nvs.bin --app-header private/legacy-app-header.bin \
  --settings private/qso.qso-settings \
  --firmware build/PassportOS-full.bin --port PORT --output private/migration-plan.json
```

The planner only reads local files and creates a private JSON plan. It checks the 288-byte app header/descriptor against the audited release ELF identity, the exact three-entry legacy layout, table MD5 and flags, verifies that the exported settings exactly match the NVS read, and binds the launcher to its content-addressed debug archive. A different firmware descriptor/table or mismatched export is rejected. The small descriptor read is a build-identity check, not full-image authentication; no full firmware readback is required. The plan does **not** authorize or perform its listed operations.

First provisioning must initialize future app/data locations at `0x190000`, length `0x670000`, because the old QSO image can leave code inside the future first app slot. This is destructive to **all future app/private/metadata locations** and is valid only for the reviewed single-app conversion, never an existing PassportOS device. The plan also records the complete provisioning image's NVS impact. It does not use a full-chip erase or change eFuses. Only after reviewing these exact ranges, preserving the verified export and obtaining approval should any device write occur.

## Restore through the phone installer

1. Boot the newly provisioned launcher and install the current `QSO Vision` `.passport` package, ID `org.passportos.qso-vision`, with migration support (`2.3.3-passportos.2` from this increment).
2. Before using QSO or saving new profiles/settings, open **Install New App**, connect the phone to the displayed AP, and open `http://192.168.4.1`.
3. In QSO's browser card, select the `.qso-settings` file and choose **Import legacy settings**. The server validates the fixed format, SHA and supported app marker. It stages one NVS blob only in QSO's private partition.
4. Import refuses existing private records or a different staged export. Repeating the same file is safe, including after a reported write/commit failure. Import never erases another app or clears existing QSO data to make room.
5. Leave Install and launch QSO. Before reading settings or starting radio/UI workers, the adapter applies the staged values. A failure returns to the launcher on reset; the immutable stage allows retry on the next launch. Application initialization cannot use a partially applied import.
6. A completion digest is saved only after all setting writes and their commit succeed. Subsequent launches or re-uploading the same file do not overwrite changes made after completion. Modern profile entries take priority over redundant legacy credential keys.
7. Verify the actual talkgroup/volume/display settings and saved profiles on the board. Keep the original read/export until physical acceptance. Do not downgrade to an adapter without migration support while an import is pending.

## Data format and limits

`.qso-settings` is exactly **1056 bytes**. The first 1024 bytes are a little-endian, zero-padded payload: `PPTQSO1\0`, version `1`, a 23-field presence mask, four signed numeric fields, eight SSIDs/passwords (33/65 bytes each), legacy SSID/password and last SSID (33/65/33), the 32-byte source NVS SHA, and 45 reserved zero bytes. The final 32 bytes are SHA-256 of the entire payload. Limits mirror the inspected QSO readers. It is neither an executable image nor an encrypted/publisher-signed archive.

Staging uses private namespace `pos_migrate`, blob `qso_v1`; `applied_v1` stores the completion digest. The SDK's named-partition APIs avoid shared NVS; bounds checks reject a running-app or private-partition map that differs from the fixed PassportOS layout. NVS persistence semantics are described in the [Espressif NVS reference](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32c3/api-reference/storage/nvs_flash.html). Multi-key import is made retryable by controlling when the application can proceed; it is not advertised as an atomic multi-key NVS transaction.

Host checks exercise an SDK-generated synthetic 24 KiB NVS fixture, Python/C format agreement, real SHA/CRC checks in the exporter, 7 staging and 31 application I/O boundaries with before/after-write faults, existing-data refusal, idempotent retry, and completed-import preservation. The NVS/SHA functions in the C fault harness are stubs; real board power cuts, NVS recovery and user-visible restoration remain unverified.
