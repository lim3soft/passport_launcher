#!/usr/bin/env python3
"""Produce a reviewable first-install plan. Never connects to or writes a device."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

from qso_settings import legacy_values, encode, decode, write_private
from verify_firmware import parse_partition_table

LEGACY_QSO_ELF_SHA256 = 'd3dbb983cb3a3e606eb0fafac0ad586474d869a7d978779afbc18ec64f9b8eb3'


def validate_legacy_app_header(app_header):
    if (len(app_header) != 288 or app_header[0] != 0xe9 or
        struct.unpack_from('<H', app_header, 12)[0] != 5 or
        struct.unpack_from('<I', app_header, 32)[0] != 0xabcd5432 or
        app_header[48:80].split(b'\0', 1)[0] != b'2.3.3' or
        app_header[144:176].split(b'\0', 1)[0] != b'v5.5.3' or
        app_header[176:208].hex() != LEGACY_QSO_ELF_SHA256):
        raise ValueError('legacy app descriptor does not match the audited QSO Vision 2.3.3 build; further audit required')


def validate_legacy_layout(table):
    partitions, has_md5 = parse_partition_table(table)
    expected = {'nvs': (1, 2, 0x9000, 0x6000), 'phy_init': (1, 1, 0xf000, 0x1000),
                'factory': (0, 0, 0x10000, 0x7f0000)}
    observed = {p.label: (p.kind, p.subtype, p.offset, p.size) for p in partitions}
    if not has_md5 or len(partitions) != 3 or observed != expected:
        raise ValueError('legacy table differs from the audited single-app QSO layout; no generic erase plan is safe')
    if any(struct.unpack_from('<I', table, i*32+28)[0] for i in range(3)):
        raise ValueError('legacy partition flags differ from the audited layout')


def make_plan(table, legacy_nvs, firmware, settings, port, app_header):
    validate_legacy_app_header(app_header)
    validate_legacy_layout(table)
    expected_file = encode(legacy_values(legacy_nvs), hashlib.sha256(legacy_nvs).digest())
    if settings != expected_file:
        raise ValueError('settings file does not exactly match the validated legacy NVS data')
    decode(settings)
    # Only accept a bound, verified launcher archive rather than any .bin file.
    digest = hashlib.sha256(firmware.read_bytes()).hexdigest()
    archive = firmware.parent/'firmware'/digest
    import archive_firmware
    archive_firmware.verify_archive(archive)
    if (archive/'PassportOS-full.bin').read_bytes() != firmware.read_bytes():
        raise ValueError('firmware does not match its verified archive')
    return {
        'format': 'passportos.qso-first-install-plan', 'version': 1,
        'status': 'REVIEW ONLY; explicit flash approval and device identity required',
        'port': port, 'device_accessed': False, 'device_written': False,
        'preconditions': ['confirm exact device, C3 revision, JEDEC capacity and current partition table',
            'retain the legacy NVS read and verified settings file securely before ANY writes',
            'this plan applies only to the audited QSO single-app layout, not an existing PassportOS installation'],
        'legacy_nvs_sha256': hashlib.sha256(legacy_nvs).hexdigest(),
        'legacy_app_header_sha256': hashlib.sha256(app_header).hexdigest(),
        'legacy_app_embedded_elf_sha256': LEGACY_QSO_ELF_SHA256,
        'settings_sha256': hashlib.sha256(settings).hexdigest(),
        'settings_field_count': len(decode(settings)),
        'firmware': str(firmware.resolve()), 'firmware_sha256': digest,
        'reviewed_operations': [
            {'operation': 'initialize future app/metadata/private storage', 'offset': '0x190000',
             'length': '0x670000', 'impact': 'erases ALL future app/data locations, including any stale legacy code; first install only'},
            {'operation': 'provision verified launcher image', 'offset': '0x0',
             'length': firmware.stat().st_size,
             'impact': 'replaces bootloader/table/app and resets old NVS; recovered monitor records are in the settings file'},
            {'operation': 'boot menu; install current QSO .passport package'},
            {'operation': 'upload matching .qso-settings file with Import legacy settings'},
            {'operation': 'launch QSO and verify restored profiles/settings; retain export until accepted'}],
        'excluded': ['full-chip erase', 'firmware readback prerequisite', 'automatic flashing', 'eFuse changes'],
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--table', type=Path, required=True)
    p.add_argument('--app-header', type=Path, required=True)
    p.add_argument('--nvs', type=Path, required=True)
    p.add_argument('--settings', type=Path, required=True)
    p.add_argument('--firmware', type=Path, required=True)
    p.add_argument('--port', required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    try:
        plan = make_plan(args.table.read_bytes(), args.nvs.read_bytes(), args.firmware,
                         args.settings.read_bytes(), args.port, args.app_header.read_bytes())
        write_private(args.output, (json.dumps(plan, indent=2)+'\n').encode())
        print('Review-only migration plan created. No device was accessed or written.')
    except (ValueError, OSError) as error: p.exit(1, str(error)+'\n')


if __name__ == '__main__': main()
