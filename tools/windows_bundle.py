#!/usr/bin/env python3
"""Build or verify a portable Windows handoff; never access a device."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import zipfile

import archive_firmware
import package_app


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(root):
    root = Path(root).resolve()
    manifest = json.loads((root/'bundle-manifest.json').read_text(encoding='utf-8'))
    if manifest.get('format') != 'passportos.windows-handoff' or manifest.get('version') != 1:
        raise ValueError('invalid Windows bundle manifest')
    files = manifest['files']
    for name, entry in files.items():
        pure = PurePosixPath(name)
        if (not name or pure.is_absolute() or str(pure) != name or '..' in pure.parts or
                '\\' in name or ':' in name): raise ValueError('unsafe bundle path')
        path = root.joinpath(*pure.parts)
        if not path.is_file() or path.is_symlink() or not path.resolve().is_relative_to(root):
            raise ValueError('missing/unsafe bundle file: '+name)
        if path.stat().st_size != entry['bytes'] or digest(path) != entry['sha256']:
            raise ValueError('bundle file hash mismatch: '+name)
    full = root/'firmware/PassportOS-full.bin'
    if 'firmware/PassportOS-full.bin' not in files or digest(full) != manifest['firmware_sha256']:
        raise ValueError('missing or inconsistent firmware identity')
    archive_firmware.verify_archive(root/'firmware/firmware'/manifest['firmware_sha256'])
    for name in ('counter-a','counter-b','qso-vision'):
        relative = f'apps/{name}.passport'
        if relative not in files: raise ValueError('missing app package')
        package_app.verify((root/relative).read_bytes())
    for name in ('tools/windows_capture.py','tools/qso_settings.py','tools/plan_qso_migration.py'):
        if name not in files: raise ValueError('missing capture tool')
    return manifest


def build(project, output, zip_path):
    project, output, zip_path = Path(project).resolve(), Path(output).resolve(), Path(zip_path).resolve()
    if output.exists() or zip_path.exists(): raise ValueError('handoff outputs already exist; use new paths')
    report = json.loads((project/'docs/validation-results.json').read_text())
    source_archive = Path(report['archive'])
    archive = archive_firmware.verify_archive(source_archive)
    full = Path(report['full_image']['path'])
    if digest(full) != report['full_image']['sha256'] or digest(full) != archive['full_bin_sha256']:
        raise ValueError('current firmware and validation identity disagree')
    output.mkdir(parents=True)
    (output/'firmware').mkdir(); (output/'apps').mkdir(); (output/'tools/vendor').mkdir(parents=True)
    shutil.copy(full, output/'firmware/PassportOS-full.bin')
    shutil.copytree(source_archive, output/'firmware/firmware'/archive['full_bin_sha256'])
    for name, entry in report['packages'].items():
        source = Path(entry['path'])
        if digest(source) != entry['package_sha256']: raise ValueError('app identity mismatch')
        package_app.verify(source.read_bytes())
        shutil.copy(source, output/'apps'/source.name)
    for name in ('windows_bundle.py','windows_capture.py','qso_settings.py','plan_qso_migration.py',
                 'archive_firmware.py','verify_firmware.py','package_app.py'):
        shutil.copy(project/'tools'/name,output/'tools'/name)
    for name in ('nvs_parser.py','ESP-IDF-LICENSE'):
        shutil.copy(project/'tools/vendor'/name,output/'tools/vendor'/name)
    for src, dest in [('docs/windows.md','START-HERE.md'),('docs/windows.zh_CN.md','START-HERE.zh_CN.md'),
                      ('LICENSE','LICENSE'),('delivery/THIRD-PARTY-NOTICES.txt','THIRD-PARTY-NOTICES.txt')]:
        shutil.copy(project/src, output/dest)
    for name in ('START-HERE.md','START-HERE.zh_CN.md'):
        path=output/name
        text=path.read_text(encoding='utf-8').replace('windows.zh_CN.md','START-HERE.zh_CN.md').replace('windows.md','START-HERE.md')
        path.write_text(text,encoding='utf-8')
    (output/'requirements-windows.txt').write_text('esptool==4.12.0\n', encoding='ascii')
    (output/'.gitignore').write_text('.venv/\nprivate/\n*.qso-settings\n', encoding='ascii')
    (output/'build-validation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    files = {str(p.relative_to(output).as_posix()):{'bytes':p.stat().st_size,'sha256':digest(p)}
             for p in sorted(output.rglob('*')) if p.is_file()}
    manifest = {'format':'passportos.windows-handoff','version':1,'firmware_version':report['version'],
                'firmware_sha256':digest(full),'esptool_version':'4.12.0','files':files,
                'flash_automation':False,'windows_device_tested':False}
    (output/'bundle-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    verify(output)
    with zipfile.ZipFile(zip_path,'x',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for p in sorted(output.rglob('*')):
            if p.is_file():z.write(p,output.name+'/'+p.relative_to(output).as_posix())
    print('Windows handoff:',zip_path)
    print('ZIP SHA256:',digest(zip_path))
    return manifest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    sub=p.add_subparsers(dest='command',required=True)
    check=sub.add_parser('verify');check.add_argument('directory',type=Path)
    new=sub.add_parser('build');new.add_argument('--project',type=Path,required=True)
    new.add_argument('--output',type=Path,required=True);new.add_argument('--zip',type=Path,required=True)
    args=p.parse_args()
    try:
        if args.command=='verify':
            manifest=verify(args.directory);print('Windows bundle verified; firmware',manifest['firmware_sha256'])
        else:build(args.project,args.output,args.zip)
    except (OSError,ValueError,KeyError) as error:p.exit(1,str(error)+'\n')


if __name__=='__main__':main()
