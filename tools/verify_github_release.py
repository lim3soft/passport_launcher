#!/usr/bin/env python3
"""Verify the exact prebuilt firmware pair before creating a GitHub release."""
from pathlib import Path
import hashlib,json,sys
from verify_firmware import parse_partition_table

def verify(version,base=None):
    if version!='0.4.1':raise ValueError('This release verifier is bound to the reviewed 0.4.1 artifacts')
    root=Path(base) if base else Path(__file__).resolve().parents[1]/'firmware'/version
    manifest=json.loads((root/'manifest.json').read_text())
    if manifest.get('schema_version')!=1 or manifest.get('version')!=version:raise ValueError('Manifest identity mismatch')
    for name,record in manifest['files'].items():
        if Path(name).name!=name or name in {'.','..'}:raise ValueError('Unsafe asset path')
        p=root/name
        if p.is_symlink():raise ValueError('Symlink release asset')
        raw=p.read_bytes()
        if len(raw)!=record['size'] or hashlib.sha256(raw).hexdigest()!=record['sha256']:raise ValueError(f'Asset hash/length mismatch: {name}')
    fresh=(root/f'Passport-Launcher-{version}-fresh.bin').read_bytes()
    update=(root/f'Passport-Launcher-{version}-update.bin').read_bytes()
    if not 0x10000<len(update)<0x190000 or len(fresh)!=0x800000:raise ValueError('Firmware bounds mismatch')
    if fresh!=update+b'\xff'*(0x800000-len(update)):raise ValueError('Fresh/update pair mismatch')
    offset=0x10000
    if update[0]!=0xe9 or update[offset:offset+4][:1]!=b'\xe9' or update[offset+2:offset+4]!=b'\x02\x3f':raise ValueError('Wrong image target/configuration')
    if update[offset+12:offset+14]!=b'\x05\0':raise ValueError('Wrong MCU target')
    if update[offset+48:offset+80].split(b'\0',1)[0].decode()!=version:raise ValueError('Wrong application version')
    if update[offset+176:offset+208].hex()!=manifest['matching_elf_sha256']:raise ValueError('ELF identity mismatch')
    partitions,has_md5=parse_partition_table(update[0x8000:0x8c00])
    expected={'nvs':(0x9000,0x4000),'otadata':(0xd000,0x2000),'phy_init':(0xf000,0x1000),'factory':(0x10000,0x180000),'pos_meta':(0x7f0000,0x10000)}
    if not has_md5 or {p.label:(p.offset,p.size) for p in partitions}!=expected:raise ValueError('Wrong partition layout')
    sums={}
    for line in (root/'SHA256SUMS.txt').read_text().splitlines():
        digest,name=line.split('  ',1)
        if Path(name).name!=name or hashlib.sha256((root/name).read_bytes()).hexdigest()!=digest:raise ValueError('Checksum list mismatch')
        sums[name]=digest
    if set(sums)!=(set(manifest['files'])|{'manifest.json'}):raise ValueError('Incomplete checksum coverage')
    return {'version':version,'fresh_bytes':len(fresh),'update_bytes':len(update),'assets_checked':len(sums),'matching_elf_sha256':manifest['matching_elf_sha256']}

if __name__=='__main__':
    try:print(json.dumps(verify(sys.argv[1]),indent=2))
    except (ValueError,KeyError,OSError,IndexError) as e:raise SystemExit(str(e))
