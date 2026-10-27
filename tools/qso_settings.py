#!/usr/bin/env python3
"""Export audited legacy QSO NVS settings without flashing or printing secrets."""
import argparse
import hashlib
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / 'vendor'))
from nvs_parser import NVS_Partition  # ESP-IDF v5.5.3, unmodified Apache-2.0 source.

SIZE = 1056
MAGIC = b'PPTQSO1\0'
NUMBERS = ('talkgroup', 'volume', 'dim_mode', 'brightness')
FIELDS = list(NUMBERS) + [f'p{i}_{k}' for i in range(8) for k in ('s', 'p')] + [
    'wifi_ssid', 'wifi_password', 'wifi_last_ssid']
RANGES = ((91, 16777215), (0, 100), (0, 4), (1, 100))


def field_location(i):
    if i < 4:
        return 16 + i * 4, 4
    if i < 20:
        profile = (i - 4) // 2
        return (296 + profile * 65, 65) if i & 1 else (32 + profile * 33, 33)
    return ((816, 33), (849, 65), (914, 33))[i - 20]


def encode(values, source_sha):
    if not values or set(values) - set(FIELDS) or len(source_sha) != 32:
        raise ValueError('empty/unknown QSO settings or invalid source identity')
    raw = bytearray(SIZE)
    struct.pack_into('<8sII', raw, 0, MAGIC, 1, sum(1 << FIELDS.index(k) for k in values))
    for i, key in enumerate(FIELDS):
        if key not in values:
            continue
        value = values[key]
        offset, width = field_location(i)
        if i < 4:
            low, high = RANGES[i]
            if type(value) is not int or not low <= value <= high:
                raise ValueError('QSO numeric setting is outside its supported range')
            struct.pack_into('<i', raw, offset, value)
        else:
            if not isinstance(value, bytes) or len(value) >= width or b'\0' in value:
                raise ValueError('invalid QSO string length or embedded NUL')
            raw[offset:offset+len(value)] = value
    raw[947:979] = source_sha
    raw[1024:] = hashlib.sha256(raw[:1024]).digest()
    return bytes(raw)


def decode(raw):
    if len(raw) != SIZE or raw[:8] != MAGIC:
        raise ValueError('invalid QSO settings file length/magic')
    version, mask = struct.unpack_from('<II', raw, 8)
    if version != 1 or not mask or mask >> len(FIELDS) or any(raw[979:1024]):
        raise ValueError('invalid QSO settings schema')
    if hashlib.sha256(raw[:1024]).digest() != raw[1024:]:
        raise ValueError('QSO settings checksum mismatch')
    values = {}
    for i, key in enumerate(FIELDS):
        offset, width = field_location(i)
        data = raw[offset:offset+width]
        if not mask & (1 << i):
            if any(data): raise ValueError('nonzero absent QSO field')
            continue
        if i < 4:
            values[key] = struct.unpack('<i', data)[0]
        else:
            if b'\0' not in data: raise ValueError('unterminated QSO string')
            value, tail = data.split(b'\0', 1)
            if any(tail): raise ValueError('noncanonical QSO string padding')
            values[key] = value
    if encode(values, raw[947:979]) != raw:
        raise ValueError('noncanonical QSO settings data')
    return values


def legacy_values(raw):
    if len(raw) != 0x6000:
        raise ValueError('expected exactly the audited 24 KiB legacy NVS partition')
    partition = NVS_Partition('legacy-qso', bytearray(raw))
    namespaces, sequences, entries = {}, set(), []
    for page in partition.pages:
        if page.is_empty:
            if bytes(raw[page.start_address:page.start_address+4096]) != b'\xff' * 4096:
                raise ValueError('nonblank content in an uninitialized NVS page')
            continue
        h = page.header
        if h['status'] not in ('Active', 'Full') or h['version'] != 2:
            raise ValueError('unsupported, transitional or corrupt NVS page')
        if h['crc']['original'] != h['crc']['computed'] or h['page_index'] in sequences:
            raise ValueError('NVS page CRC/sequence ambiguity')
        sequences.add(h['page_index'])
        for entry in page.entries:
            if entry.state == 'Invalid': raise ValueError('invalid NVS entry state')
            if entry.state != 'Written': continue
            m = entry.metadata
            if m['crc']['original'] != m['crc']['computed'] or entry.key is None or entry.data is None:
                raise ValueError('invalid written NVS entry')
            if not 1 <= m['span'] <= 126-entry.index:
                raise ValueError('invalid written NVS span')
            if m['namespace'] == 0:
                if m['type'] != 'uint8_t' or m['span'] != 1 or not 1 <= entry.data['value'] <= 254:
                    raise ValueError('invalid NVS namespace')
                index = entry.data['value']
                if index in namespaces and namespaces[index] != entry.key:
                    raise ValueError('ambiguous NVS namespace')
                namespaces[index] = entry.key
            entries.append(entry)
    monitor_ids = [i for i, name in namespaces.items() if name == 'monitor']
    if len(monitor_ids) != 1: raise ValueError('missing or ambiguous QSO monitor namespace')
    values = {}
    for entry in entries:
        m = entry.metadata
        if m['namespace'] != monitor_ids[0]: continue
        key = entry.key
        if key not in FIELDS or key in values:
            raise ValueError('unknown or duplicate live QSO key; no data was discarded')
        if key in NUMBERS:
            if m['type'] != 'int32_t' or m['span'] != 1:
                raise ValueError('unexpected QSO numeric storage type')
            values[key] = entry.data['value']
        else:
            if m['type'] != 'string' or m['span'] < 2:
                raise ValueError('unexpected QSO string storage type')
            size = entry.data['size']
            if len(entry.children) != m['span']-1 or size < 1 or size > len(entry.children)*32 or any(
                    child.state != 'Written' for child in entry.children):
                raise ValueError('incomplete NVS string')
            if m['crc']['data_original'] != m['crc']['data_computed']:
                raise ValueError('NVS string CRC mismatch')
            value = b''.join(bytes(c.raw) for c in entry.children)[:size]
            if value[-1:] != b'\0' or b'\0' in value[:-1]: raise ValueError('invalid NVS string termination')
            values[key] = value[:-1]
    encode(values, hashlib.sha256(raw).digest())  # Validate all ranges before export.
    return values


def write_private(path, raw):
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, 'wb') as output: output.write(raw)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    export = sub.add_parser('export')
    export.add_argument('nvs', type=Path)
    export.add_argument('--output', type=Path, required=True)
    verify = sub.add_parser('verify')
    verify.add_argument('file', type=Path)
    args = parser.parse_args()
    try:
        if args.command == 'export':
            nvs = args.nvs.read_bytes()
            raw = encode(legacy_values(nvs), hashlib.sha256(nvs).digest())
            write_private(args.output, raw)
        else: raw = args.file.read_bytes()
        values = decode(raw)
        print(f'QSO settings verified: {len(values)} fields; SHA256 {hashlib.sha256(raw).hexdigest()}')
        print('Contains private Wi-Fi credentials. Values were not printed.')
    except (ValueError, OSError, OverflowError, IndexError, KeyError, TypeError) as error:
        parser.exit(1, str(error)+'\n')


if __name__ == '__main__': main()
