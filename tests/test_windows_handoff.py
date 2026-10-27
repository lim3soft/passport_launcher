from pathlib import Path
import json
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT=Path(__file__).parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import windows_capture as tool
from test_migration_plan import table,app_header


class WindowsCapture(unittest.TestCase):
    def run_capture(self, folder, security=None, flash=None, old_table=None, header=None, revision=4,
                    truncated=False, changed_table=False):
        bundle=folder/'bundle';image=bundle/'firmware/firmware'/('1'*64)/'PassportOS.bin'
        image.parent.mkdir(parents=True,exist_ok=True)
        raw=bytearray(24);struct.pack_into('<HH',raw,15,3,199);image.write_bytes(raw)
        commands=[];reads=[]
        sec=security or f'Chip is ESP32-C3 (revision v0.{revision})\nMAC: aa:bb:cc:dd:ee:ff\nSecure Boot: Disabled\nFlash Encryption: Disabled\n'
        ident=flash or 'Detected flash size: 8MB\n'
        def runner(args,**kwargs):
            commands.append(args)
            self.assertNotIn('shell',kwargs)
            self.assertIn('--no-stub',args)
            self.assertEqual(args[args.index('--after')+1],'no_reset')
            self.assertEqual(args[args.index('--before')+1],'default_reset' if len(commands)==1 else 'no_reset')
            command=next(x for x in ('get_security_info','flash_id','read_flash') if x in args)
            if command=='get_security_info':out=sec
            elif command=='flash_id':out=ident
            else:
                i=args.index(command);address,size,path=args[i+1:i+4];reads.append(address)
                data={'0x8000':old_table if old_table is not None else table(),
                      '0x10000':header if header is not None else app_header(),
                      '0x9000':(ROOT/'tests/fixtures/qso-legacy-synthetic.bin').read_bytes()}[address]
                if address=='0x8000' and changed_table and reads.count(address)==2:data=bytes(len(data))
                if truncated and address=='0x9000':data=data[:-1]
                Path(path).write_bytes(data);out='Read complete\n'
            return SimpleNamespace(stdout=out,stderr='',returncode=0)
        self.commands,self.reads=commands,reads
        with patch.object(tool,'verify_bundle',return_value={'firmware_sha256':'1'*64}), \
             patch.object(tool,'version',return_value='4.12.0'), \
             patch.object(tool,'make_plan',return_value={'status':'REVIEW ONLY','device_written':False}):
            return tool.capture('COM5',folder/'private/capture one',bundle,runner)

    def test_success_uses_read_only_ranges_and_shares_no_credentials(self):
        with tempfile.TemporaryDirectory(prefix='Windows path with spaces ') as d:
            root=Path(d);report=self.run_capture(root)
            self.assertEqual(report['status'],'READ_ONLY_CAPTURE_VERIFIED')
            self.assertEqual(self.reads,['0x8000','0x10000','0x9000','0x8000'])
            self.assertEqual(report['settings_field_count'],23)
            self.assertFalse(report['flash_written']);self.assertFalse(report['efuses_written'])
            summary=(root/'private/capture one/report-to-share.json').read_text()
            for secret in ('aa:bb:cc:dd:ee:ff','TEST-NETWORK','NOT-A-REAL-PASSWORD'):
                self.assertNotIn(secret,summary)
            self.assertTrue((root/'private/capture one/qso.qso-settings').exists())
            self.assertIn('aa:bb:cc:dd:ee:ff',(root/'private/capture one/security.log').read_text())

    def test_security_capacity_revision_and_unknown_layout_fail_before_nvs(self):
        for kwargs in ({'security':'Secure Boot: Enabled\nFlash Encryption: Disabled\n'},
                       {'flash':'Detected flash size: 4MB\n'}, {'revision':2},
                       {'old_table':table(0x3f0000)}, {'header':bytes(288)}):
            with self.subTest(case=kwargs),tempfile.TemporaryDirectory() as d:
                root=Path(d)
                with self.assertRaises(ValueError):self.run_capture(root,**kwargs)
                self.assertNotIn('0x9000',self.reads)
                report=json.loads((root/'private/capture one/report-to-share.json').read_text())
                self.assertEqual(report['status'],'STOP_FOR_REVIEW')
                self.assertFalse((root/'private/capture one/qso.qso-settings').exists())

    def test_partial_read_or_changing_table_does_not_export(self):
        for kwargs in ({'truncated':True},{'changed_table':True}):
            with tempfile.TemporaryDirectory() as d:
                root=Path(d)
                with self.assertRaises(ValueError):self.run_capture(root,**kwargs)
                self.assertFalse((root/'private/capture one/qso.qso-settings').exists())
                self.assertFalse((root/'private/capture one/migration-plan.json').exists())

    def test_invalid_ports_and_write_operations_are_rejected(self):
        for port in ('COM0','COM5 & erase_flash','socket://example:1','/dev/ttyUSB0'):
            with self.assertRaises(ValueError):tool.read_command(port,'flash_id')
        for command in ('write_flash','erase_flash','erase_region','write_mem','load_ram','run'):
            with self.assertRaises(ValueError):tool.read_command('COM5',command)
        with self.assertRaises(ValueError):tool.read_command('COM5','read_flash',extra=('0','ALL','dump.bin'))

    def test_existing_output_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);out=root/'private/capture one';out.mkdir(parents=True)
            marker=out/'keep';marker.write_text('preserve')
            with self.assertRaises(FileExistsError):self.run_capture(root)
            self.assertEqual(marker.read_text(),'preserve');self.assertEqual(self.commands,[])

    def test_bundle_or_tool_version_failure_never_opens_device(self):
        with tempfile.TemporaryDirectory() as d,patch.object(tool,'verify_bundle',side_effect=ValueError('changed bundle')):
            runner=unittest.mock.Mock()
            with self.assertRaises(ValueError):tool.capture('COM5',Path(d)/'new',Path(d),runner)
            runner.assert_not_called()
        with tempfile.TemporaryDirectory() as d,patch.object(tool,'verify_bundle',return_value={}),patch.object(tool,'version',return_value='5.0'):
            runner=unittest.mock.Mock()
            with self.assertRaises(ValueError):tool.capture('COM5',Path(d)/'new',Path(d),runner)
            runner.assert_not_called()


if __name__=='__main__':unittest.main()
