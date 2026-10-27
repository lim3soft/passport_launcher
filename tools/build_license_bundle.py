#!/usr/bin/env python3
"""Retain complete local notices in a deterministic gzip served by the portal."""
import gzip
from pathlib import Path
import sys

def build(license_path, notices_path, output):
    raw=(b'Passport Launcher\nDeveloped by 9W3SPY\n\nAPPLICATION LICENSE / RETAINED BSP NOTICE\n\n'+
         Path(license_path).read_bytes()+b'\n\nOPEN-SOURCE COMPONENT NOTICES\n'+
         b'Components retain their respective licenses and copyright notices.\n'+
         b'This collection also retains notices for optional application builds.\n\n'+Path(notices_path).read_bytes())
    compressed=gzip.compress(raw,compresslevel=9,mtime=0)
    assert gzip.decompress(compressed)==raw
    Path(output).write_bytes(compressed)

if __name__=='__main__':build(*sys.argv[1:])
