#!/usr/bin/env python3
"""Make an explicit fresh-install image that clears the complete 8 MiB flash.

The ordinary merged prefix alone would leave old slot data in the new pool.
The padded image is intentionally destructive and is NOT a routine update.
"""
from pathlib import Path
import sys
FLASH=0x800000
POOL=0x190000

def prepare(path):
    path=Path(path)
    raw=path.read_bytes()
    if not 0x10000<len(raw)<=FLASH or raw[0]!=0xe9:
        raise ValueError('not a complete ESP32-C3 merged image')
    if len(raw)>POOL and any(b!=255 for b in raw[POOL:]):
        raise ValueError('refusing to overwrite unexpected app-pool or metadata data')
    padded=raw+bytes([255])*(FLASH-len(raw))
    path.write_bytes(padded)
    print(f'Fresh install: {FLASH} bytes; app pool and metadata explicitly cleared.')

if __name__=='__main__':
    prepare(sys.argv[1])
