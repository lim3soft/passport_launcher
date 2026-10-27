#!/usr/bin/env python3
"""Explicit-COM read-only capture for Windows. Never flashes or erases a device."""
import argparse
import hashlib
from importlib.metadata import version, PackageNotFoundError
import json
from pathlib import Path
import re
import struct
import subprocess
import sys

from windows_bundle import verify as verify_bundle
from plan_qso_migration import validate_legacy_app_header, validate_legacy_layout, make_plan
from qso_settings import legacy_values, encode, write_private

PINNED_ESPTOOL='4.12.0'


def read_command(port, command, first=False, extra=()):
    if not re.fullmatch(r'COM[1-9][0-9]{0,3}',port,re.I): raise ValueError('provide a specific COM port, such as COM5')
    if command not in ('get_security_info','flash_id','read_flash'): raise ValueError('only read commands are allowed')
    if command!='read_flash' and extra: raise ValueError('unexpected command arguments')
    if command=='read_flash':
        if len(extra)!=3 or (extra[0],extra[1]) not in (
                ('0x8000','0x1000'),('0x10000','0x120'),('0x9000','0x6000')):
            raise ValueError('flash read is outside the audited capture ranges')
    return [sys.executable,'-m','esptool','--chip','esp32c3','--port',port.upper(),
            '--baud','115200','--before','default_reset' if first else 'no_reset',
            '--after','no_reset','--no-stub','--connect-attempts','3',command,*map(str,extra)]


def capture(port, output, bundle, runner=subprocess.run):
    manifest=verify_bundle(bundle) # Check files before any serial access.
    try: installed=version('esptool')
    except PackageNotFoundError as error: raise ValueError('install requirements-windows.txt into the selected Python environment first') from error
    if installed!=PINNED_ESPTOOL: raise ValueError('use the bundle virtual environment with esptool '+PINNED_ESPTOOL)
    read_command(port,'get_security_info') # Validate port before creating files.
    output=Path(output).resolve();output.mkdir(parents=True,exist_ok=False)
    report={'format':'passportos.windows-capture','version':1,'port':port.upper(),
            'firmware_sha256':manifest['firmware_sha256'],'status':'INCOMPLETE',
            'flash_written':False,'efuses_written':False,'values_shared':False}
    def execute(name,command,first=False,extra=()):
        result=runner(read_command(port,command,first,extra),capture_output=True,text=True,
                      encoding='utf-8',errors='replace',timeout=180,check=False)
        # Raw connection logs stay in the user's private capture directory.
        (output/(name+'.log')).write_text(result.stdout+'\n'+result.stderr,encoding='utf-8')
        if result.returncode:raise ValueError('read-only device command failed; inspect the private '+name+'.log locally')
        return result.stdout
    try:
        security=execute('security','get_security_info',True)
        if not re.search(r'^Secure Boot: Disabled\s*$',security,re.M) or not re.search(r'^Flash Encryption: Disabled\s*$',security,re.M):
            raise ValueError('security status is unsupported or unclear; stop for review')
        report.update(secure_boot='disabled',flash_encryption='disabled')
        flash=execute('flash-id','flash_id')
        if not re.search(r'Detected flash size:\s*8MB\s*$',flash,re.M):
            raise ValueError('detected flash capacity is not the audited 8 MB configuration')
        report.update(chip='ESP32-C3',flash_capacity='8MB')
        revision=re.search(r'revision\s+(v[0-9]+\.[0-9]+)',security+'\n'+flash)
        if not revision:raise ValueError('silicon revision was not reported; stop for review')
        report['silicon_revision']=revision.group(1)
        major,minor=map(int,revision.group(1)[1:].split('.'));revision_number=major*100+minor
        app=(Path(bundle)/'firmware/firmware'/manifest['firmware_sha256']/'PassportOS.bin').read_bytes()
        minimum,maximum=struct.unpack_from('<HH',app,15)
        if revision_number<minimum or (maximum and revision_number>maximum):
            raise ValueError('silicon revision is outside this firmware image compatibility range')
        def read(name,address,size):
            path=output/(name+'.bin')
            execute(name,'read_flash',extra=(address,size,str(path)))
            raw=path.read_bytes()
            if len(raw)!=int(size,0):raise ValueError('incomplete '+name+' read; keep files and stop for review')
            return raw
        table=read('legacy-table','0x8000','0x1000');validate_legacy_layout(table)
        header=read('legacy-app-header','0x10000','0x120');validate_legacy_app_header(header)
        # Read credentials only after identifying the audited legacy layout/app.
        nvs=read('legacy-nvs','0x9000','0x6000')
        check_table=read('table-check','0x8000','0x1000')
        if table!=check_table:raise ValueError('partition table changed during capture; stop for review')
        values=legacy_values(nvs);settings=encode(values,hashlib.sha256(nvs).digest())
        write_private(output/'qso.qso-settings',settings)
        plan=make_plan(table,nvs,Path(bundle)/'firmware/PassportOS-full.bin',settings,port.upper(),header)
        write_private(output/'migration-plan.json',(json.dumps(plan,indent=2)+'\n').encode())
        report.update(status='READ_ONLY_CAPTURE_VERIFIED',legacy_app='QSO Vision 2.3.3 (audited build)',
                      settings_field_count=len(values),nvs_sha256=hashlib.sha256(nvs).hexdigest(),
                      settings_sha256=hashlib.sha256(settings).hexdigest(),migration_plan_generated=True)
    except (ValueError,OSError,subprocess.SubprocessError) as error:
        report.update(status='STOP_FOR_REVIEW',reason=str(error))
        raise
    finally:
        # Whitelist-only summary: no MAC, serial number, SSID, password or raw logs.
        (output/'report-to-share.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return report


def main():
    p=argparse.ArgumentParser(description=__doc__)
    sub=p.add_subparsers(dest='command',required=True)
    sub.add_parser('ports')
    read=sub.add_parser('capture');read.add_argument('--port',required=True);read.add_argument('--output',type=Path,required=True)
    args=p.parse_args()
    try:
        if args.command=='ports':
            from serial.tools import list_ports
            for port in list_ports.comports():print(port.device, '-', port.description)
            print('Select the AI-Passport COM port explicitly. No port was opened.')
        else:
            from serial.tools import list_ports
            if args.port.upper() not in {port.device.upper() for port in list_ports.comports()}:
                raise ValueError('that COM port is not currently listed; do not guess another port')
            report=capture(args.port,args.output,Path(__file__).resolve().parents[1])
            print(report['status']);print('Share only:',args.output/'report-to-share.json')
            print('Keep the NVS, settings, plan and raw logs private. Device remains in ROM loader; power it off/on to resume the current firmware.')
    except (ValueError,OSError,subprocess.SubprocessError,ImportError) as error:
        p.exit(1,str(error)+'\nNo flash/erase operation was attempted. Inspect report-to-share.json if created.\n')


if __name__=='__main__':main()
