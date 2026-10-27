"""Create protocol fixtures for the host simulator, never firmware for a device."""
import hashlib
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).parents[1]/'tools'))
import package_app

CONFIG = '\n'.join(('CONFIG_IDF_TARGET="esp32c3"', 'CONFIG_BOOTLOADER_CUSTOM_RESERVE_RTC=y',
    'CONFIG_BOOTLOADER_CUSTOM_RESERVE_RTC_SIZE=0x20', 'CONFIG_BOOTLOADER_RESERVE_RTC_SIZE=0x20'))


def make(app_id, version, legacy=False, payload=8200):
    header = bytearray(24)
    header[:4] = b'\xe9\x01\x02\x3f'
    header[12:14] = b'\x05\0'; header[23] = 1
    segment = bytearray(payload)
    struct.pack_into('<I', segment, 0, 0xabcd5432)
    segment[112:118] = b'v5.5.3'
    # The declaration crosses a 4 KiB flash-read boundary.
    marker = b'COMMUNITY_DEFAULT_NVS' if legacy else b'PASSPORTOS_ABI2:'+app_id.encode()+b'\0'
    segment[4090-32:4090-32+len(marker)] = marker
    segment[7000:7000+len(version)] = version.encode()
    raw = bytes(header)+struct.pack('<II', 0x3c000020, len(segment))+segment
    checksum = 0xef
    for byte in segment: checksum ^= byte
    raw += bytes(15-len(raw)%16)+bytes([checksum])
    raw += hashlib.sha256(raw).digest()
    if legacy:
        return package_app.HEADER.pack(package_app.MAGIC,1,0,len(raw),package_app.text(app_id,64,True),
            package_app.text(app_id.rsplit('.',1)[1],32),package_app.text(version,32),
            hashlib.sha256(raw).digest(),bytes(332))+raw
    return package_app.create(raw, app_id, app_id.rsplit('.', 1)[1], version, CONFIG)


if __name__ == '__main__':
    destination = Path(sys.argv[1]); destination.mkdir(parents=True, exist_ok=True)
    for label, app_id, version in (
        ('a1', 'org.passportos.test.a', 'v1'), ('a2', 'org.passportos.test.a', 'v2'),
        ('b1', 'org.passportos.test.b', 'v1'), ('c1', 'org.passportos.test.c', 'v1'),
        ('d1', 'org.passportos.test.d', 'v1')):
        raw = make(app_id, version)
        package_app.verify(raw)
        (destination/(label+'.passport')).write_bytes(raw)
    for label in ('x1','y1'):
        (destination/(label+'.passport')).write_bytes(make('community.'+label,'v1',legacy=True))
    (destination/'large.transport').write_bytes(make('community.large','v1',legacy=True,payload=0x280000))
    print('Host lifecycle protocol fixtures generated; these are not executable test apps.')
