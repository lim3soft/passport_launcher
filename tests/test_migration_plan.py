import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).parents[1]
sys.path.insert(0, str(ROOT/'tools'))
import plan_qso_migration as tool
import qso_settings


def table(factory_size=0x7f0000, flags=0):
    raw = b''
    for name, kind, sub, offset, size in [('nvs', 1, 2, 0x9000, 0x6000),
            ('phy_init', 1, 1, 0xf000, 0x1000), ('factory', 0, 0, 0x10000, factory_size)]:
        raw += struct.pack('<HBBII16sI', 0x50aa, kind, sub, offset, size, name.encode(), flags)
    raw += b'\xeb\xeb'+b'\xff'*14+hashlib.md5(raw).digest()
    return raw.ljust(4096, b'\xff')


def app_header():
    raw = bytearray(288);raw[0] = 0xe9;struct.pack_into('<H', raw, 12, 5)
    struct.pack_into('<I', raw, 32, 0xabcd5432)
    raw[48:53] = b'2.3.3';raw[144:150] = b'v5.5.3'
    raw[176:208] = bytes.fromhex(tool.LEGACY_QSO_ELF_SHA256)
    return raw


class Plans(unittest.TestCase):
    def setUp(self):
        self.nvs = (ROOT/'tests/fixtures/qso-legacy-synthetic.bin').read_bytes()
        self.settings = qso_settings.encode(qso_settings.legacy_values(self.nvs), hashlib.sha256(self.nvs).digest())

    def test_review_only_plan_bound_to_matching_files(self):
        with tempfile.TemporaryDirectory() as d:
            firmware = Path(d)/'PassportOS-full.bin'
            firmware.write_bytes(b'synthetic-artifact')
            archive = Path(d)/'firmware'/hashlib.sha256(firmware.read_bytes()).hexdigest()
            archive.mkdir(parents=True);(archive/firmware.name).write_bytes(firmware.read_bytes())
            with patch('archive_firmware.verify_archive', return_value={}) as check:
                plan = tool.make_plan(table(), self.nvs, firmware, self.settings, 'TEST-ONLY', app_header())
            check.assert_called_once_with(archive)
            self.assertFalse(plan['device_accessed']);self.assertFalse(plan['device_written'])
            self.assertEqual(plan['settings_field_count'], 23)
            self.assertEqual(plan['reviewed_operations'][0]['offset'], '0x190000')
            self.assertEqual(plan['reviewed_operations'][0]['length'], '0x670000')
            self.assertIn('full-chip erase', plan['excluded'])
            self.assertNotIn('NOT-A-REAL-PASSWORD', str(plan))

    def test_foreign_layout_and_flags_refused_before_archive_access(self):
        with patch('archive_firmware.verify_archive') as check:
            for t in (table(0x3f0000), table(flags=1), bytes(4096)):
                with self.assertRaises(ValueError): tool.make_plan(t, self.nvs, Path('missing'), self.settings, 'TEST', app_header())
            check.assert_not_called()

    def test_mismatching_export_refused_before_archive_access(self):
        values = qso_settings.legacy_values(self.nvs);values['volume'] = 10
        changed = qso_settings.encode(values, hashlib.sha256(self.nvs).digest())
        with patch('archive_firmware.verify_archive') as check:
            with self.assertRaises(ValueError): tool.make_plan(table(), self.nvs, Path('missing'), changed, 'TEST', app_header())
            check.assert_not_called()

    def test_foreign_firmware_identity_refused(self):
        with patch('archive_firmware.verify_archive') as check:
            for offset in (0, 12, 32, 48, 144, 176):
                bad = app_header();bad[offset] ^= 1
                with self.assertRaises(ValueError): tool.make_plan(table(), self.nvs, Path('missing'), self.settings, 'TEST', bad)
            check.assert_not_called()


if __name__ == '__main__': unittest.main()
