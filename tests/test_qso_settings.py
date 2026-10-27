import contextlib
import hashlib
import importlib.util
import io
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from zlib import crc32

ROOT = Path(__file__).parents[1]
sys.path.insert(0, str(ROOT/'tools'))
import qso_settings as tool
from vendor.nvs_parser import NVS_Partition


class Settings(unittest.TestCase):
    def setUp(self):
        self.nvs = (ROOT/'tests/fixtures/qso-legacy-synthetic.bin').read_bytes()
        self.values = tool.legacy_values(self.nvs)
        self.raw = tool.encode(self.values, hashlib.sha256(self.nvs).digest())

    def test_sdk_generated_fixture_and_canonical_roundtrip(self):
        self.assertEqual(len(self.values), 23)
        self.assertEqual(self.values['volume'], 75)
        self.assertEqual(self.values['p7_s'], b'TEST-NETWORK-7')
        self.assertEqual(tool.decode(self.raw), self.values)
        self.assertEqual(len(self.raw), 1056)
        self.assertEqual(self.raw[947:979], hashlib.sha256(self.nvs).digest())

    def test_physical_page_order_is_irrelevant(self):
        pages = [self.nvs[i:i+4096] for i in range(0, len(self.nvs), 4096)]
        self.assertEqual(tool.legacy_values(b''.join(reversed(pages))), self.values)

    def test_package_corruption_and_size(self):
        for offset in (0, 8, 12, 16, 32, 296, 816, 849, 914, 947, 979, 1023, 1024, 1055):
            bad = bytearray(self.raw); bad[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError): tool.decode(bad)
        for raw in (self.raw[:-1], self.raw+b'\0'):
            with self.assertRaises(ValueError): tool.decode(raw)

    def test_nvs_crc_and_source_size_fail_closed(self):
        p = NVS_Partition('test', bytearray(self.nvs))
        entry = next(e for page in p.pages for e in page.entries if e.key == 'p0_s')
        page = next(page for page in p.pages if entry in page.entries)
        string_offset = page.start_address+64+(entry.index+1)*32
        for offset in (4, 28, string_offset):
            bad = bytearray(self.nvs); bad[offset] ^= 1
            with self.assertRaises(ValueError): tool.legacy_values(bad)
        for raw in (self.nvs[:-1], self.nvs+b'\xff'*4096, b'\xff'*0x6000):
            with self.assertRaises(ValueError): tool.legacy_values(raw)

    def test_unknown_live_key_is_not_dropped(self):
        p = NVS_Partition('test', bytearray(self.nvs))
        for page in p.pages:
            for entry in page.entries:
                if entry.key == 'volume':
                    offset = page.start_address+64+entry.index*32
                    bad = bytearray(self.nvs)
                    bad[offset+8:offset+24] = b'new-setting\0'.ljust(16, b'\0')
                    crc = crc32(bad[offset:offset+4]+bad[offset+8:offset+32], 0xffffffff)
                    struct.pack_into('<I', bad, offset+4, crc)
                    with self.assertRaises(ValueError): tool.legacy_values(bad)
                    return
        self.fail('fixture field missing')

    def test_bad_schema_ranges_padding(self):
        for change in ({'volume': 101}, {'talkgroup': 90}, {'dim_mode': 5}, {'brightness': 0},
                       {'p0_s': b'a'*33}, {'p0_p': b'a\0b'}, {'new': 1}):
            values = dict(self.values); values.update(change)
            with self.assertRaises(ValueError): tool.encode(values, bytes(32))

    def test_cli_permissions_no_secrets_no_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)/'private.qso-settings'
            result = subprocess.run([sys.executable, str(ROOT/'tools/qso_settings.py'), 'export',
                str(ROOT/'tests/fixtures/qso-legacy-synthetic.bin'), '--output', str(out)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertNotIn('NOT-A-REAL-PASSWORD', result.stdout+result.stderr)
            self.assertNotIn('TEST-NETWORK', result.stdout+result.stderr)
            self.assertEqual(os.stat(out).st_mode & 0o777, 0o600)
            with self.assertRaises(FileExistsError): tool.write_private(out, b'overwrite')
            self.assertEqual(out.read_bytes(), self.raw)


if __name__ == '__main__': unittest.main()
