#!/usr/bin/env python3
"""Create/verify PassportOS ABI 2 packages. No dependency beyond Python 3."""
import argparse
import hashlib
import re
import struct
from pathlib import Path

HEADER = struct.Struct('<8sIII64s32s32s32s332s')
MAGIC = b'PPTOS01\0'
MAX_IMAGE = 0x640000


def text(value, size, app_id=False):
    raw = value.encode('ascii')
    if not 0 < len(raw) < size or any(c < 32 or c > 126 for c in raw):
        raise ValueError('invalid text length/characters')
    if app_id and not re.fullmatch(r'[a-z][a-z0-9._-]*', value):
        raise ValueError('invalid stable app ID')
    return raw.ljust(size, b'\0')


def image_valid(image):
    if not 288 <= len(image) <= MAX_IMAGE:
        raise ValueError('image must fit a available shared flash')
    if image[0] != 0xe9 or not 1 <= image[1] <= 16 or image[2:4] != b'\x02\x3f':
        raise ValueError('expected app-only image: DIO, 80 MHz, 8 MiB')
    if struct.unpack_from('<H', image, 12)[0] != 5 or image[23] != 1:
        raise ValueError('expected ESP32-C3 image with appended SHA-256')
    if image[32:36] != struct.pack('<I', 0xabcd5432):
        raise ValueError('missing ESP-IDF application descriptor; merged images are not accepted')
    if image[144:150] != b'v5.5.3' or image[150] not in (0, 45):
        raise ValueError('PassportOS 0.1 supports ESP-IDF 5.5.3 app builds')
    cursor, checksum = 24, 0xef
    for _ in range(image[1]):
        if cursor + 8 > len(image):
            raise ValueError('truncated segment header')
        _, size = struct.unpack_from('<II', image, cursor)
        cursor += 8
        if cursor + size > len(image):
            raise ValueError('truncated segment')
        for byte in image[cursor:cursor + size]:
            checksum ^= byte
        cursor += size
    end = (cursor + 16) & ~15
    if len(image) != end + 32 or image[end - 1] != checksum:
        raise ValueError('bad checksum, trailing bytes, or truncated image')
    if hashlib.sha256(image[:end]).digest() != image[end:]:
        raise ValueError('bad ESP image SHA-256')


def create(image, app_id, name, version, sdkconfig):
    required = {
        'CONFIG_IDF_TARGET="esp32c3"',
        'CONFIG_BOOTLOADER_CUSTOM_RESERVE_RTC=y',
        'CONFIG_BOOTLOADER_CUSTOM_RESERVE_RTC_SIZE=0x20',
        'CONFIG_BOOTLOADER_RESERVE_RTC_SIZE=0x20',
    }
    lines = set(sdkconfig.splitlines())
    if not required <= lines or 'CONFIG_BOOTLOADER_CUSTOM_RESERVE_RTC_IN_CRC=y' in lines:
        raise ValueError('sdkconfig must reserve the PassportOS RTC ABI')
    image_valid(image)
    if b'PASSPORTOS_ABI2:' + app_id.encode('ascii') + b'\0' not in image:
        raise ValueError('app must declare PASSPORT_DECLARE_APP with the same stable ID')
    return HEADER.pack(MAGIC, 1, 2, len(image), text(app_id, 64, True), text(name, 32),
                       text(version, 32), hashlib.sha256(image).digest(), bytes(332)) + image


def verify(package):
    if len(package) < HEADER.size:
        raise ValueError('truncated package')
    magic, fmt, abi, size, app_id, name, version, digest, reserved = HEADER.unpack_from(package)
    if magic != MAGIC or fmt != 1 or abi != 2 or reserved != bytes(332):
        raise ValueError('invalid package header')
    decoded = []
    for raw, width, is_id in ((app_id, 64, True), (name, 32, False), (version, 32, False)):
        value = raw.split(b'\0', 1)[0].decode('ascii')
        if text(value, width, is_id) != raw:
            raise ValueError('noncanonical text field')
        decoded.append(value)
    image = package[HEADER.size:]
    if len(image) != size or hashlib.sha256(image).digest() != digest:
        raise ValueError('package length or digest mismatch')
    image_valid(image)
    if b'PASSPORTOS_ABI2:' + decoded[0].encode('ascii') + b'\0' not in image:
        raise ValueError('missing ABI/app-ID declaration')
    return dict(zip(('app_id', 'name', 'version'), decoded), image_size=size, sha256=digest.hex())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    new = sub.add_parser('create')
    new.add_argument('image', type=Path)
    new.add_argument('--sdkconfig', type=Path, required=True)
    new.add_argument('--id', required=True)
    new.add_argument('--name', required=True)
    new.add_argument('--version', required=True)
    new.add_argument('--output', type=Path, required=True)
    check = sub.add_parser('verify')
    check.add_argument('package', type=Path)
    args = parser.parse_args()
    try:
        if args.command == 'create':
            raw = create(args.image.read_bytes(), args.id, args.name, args.version, args.sdkconfig.read_text())
            verify(raw)
            with args.output.open('xb') as output:
                output.write(raw)
            print(verify(raw))
        else:
            print(verify(args.package.read_bytes()))
    except (ValueError, OSError, UnicodeError) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
