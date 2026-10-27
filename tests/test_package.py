import hashlib
import importlib.util
import struct
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location('package_app', Path(__file__).parents[1]/'tools/package_app.py')
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


def image():
    header = bytearray(24)
    header[0:4] = b'\xe9\x01\x02\x3f'
    header[12:14] = b'\x05\0'
    header[23] = 1
    data = bytearray(struct.pack('<I', 0xabcd5432) + bytes(252) + b'PASSPORTOS_ABI2:org.test.app\0')
    data[112:118] = b'v5.5.3'
    raw = bytes(header) + struct.pack('<II', 0x3c000020, len(data)) + data
    checksum = 0xef
    for byte in data:
        checksum ^= byte
    raw += bytes(15 - len(raw) % 16) + bytes([checksum])
    return raw + hashlib.sha256(raw).digest()


CONFIG = '\n'.join(('CONFIG_IDF_TARGET="esp32c3"', 'CONFIG_BOOTLOADER_CUSTOM_RESERVE_RTC=y',
                    'CONFIG_BOOTLOADER_CUSTOM_RESERVE_RTC_SIZE=0x20', 'CONFIG_BOOTLOADER_RESERVE_RTC_SIZE=0x20'))


class Packages(unittest.TestCase):
    def setUp(self):
        self.raw = tool.create(image(), 'org.test.app', 'Test', '1', CONFIG)

    def test_roundtrip(self):
        self.assertEqual(tool.verify(self.raw)['app_id'], 'org.test.app')

    def test_corruption_every_region(self):
        for offset in (0, 8, 12, 16, 20, 84, 116, 148, 180, 511, 512, 550, len(self.raw)-1):
            with self.subTest(offset=offset):
                changed = bytearray(self.raw)
                changed[offset] ^= 255
                with self.assertRaises((ValueError, UnicodeError)):
                    tool.verify(changed)

    def test_lengths(self):
        for raw in (self.raw[:-1], self.raw+b'\0', self.raw[:511]):
            with self.assertRaises(ValueError): tool.verify(raw)

    def test_wrong_chip_merged_oversize_checksum(self):
        for raw in (image()[:12]+b'\x09\0'+image()[14:], bytes(65536)+image(),
                    image()+bytes(0x640000), image()[:-40]+bytes(40)):
            with self.assertRaises(ValueError): tool.image_valid(raw)

    def test_missing_sdk_and_false_id(self):
        for app_id, config in (('org.test.app', ''), ('org.other.app', CONFIG), ('../nvs', CONFIG)):
            with self.assertRaises(ValueError): tool.create(image(), app_id, 'Test', '1', config)


if __name__ == '__main__': unittest.main()
