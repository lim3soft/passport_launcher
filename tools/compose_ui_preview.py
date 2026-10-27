#!/usr/bin/env python3
"""Encode native LVGL PPM captures as PNGs without external image libraries."""
from pathlib import Path
import struct
import sys
import zlib

def png(path,width,height,raw):
    def chunk(kind,payload):
        return struct.pack('>I',len(payload))+kind+payload+struct.pack('>I',zlib.crc32(kind+payload)&0xffffffff)
    scan=b''.join(b'\0'+raw[y*width*3:(y+1)*width*3] for y in range(height))
    encoded=(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,2,0,0,0))+
        chunk(b'tEXt',b'Description\0Host-rendered production LVGL UI; not a physical-device screenshot')+
        chunk(b'IDAT',zlib.compress(scan,9))+chunk(b'IEND',b''))
    path.write_bytes(encoded)

def compose(directory):
    base=Path(directory);images={}
    for p in sorted(base.glob('*.ppm')):
        magic,size,maximum,raw=p.read_bytes().split(b'\n',3)
        assert magic==b'P6' and size==b'240 320' and maximum==b'255' and len(raw)==240*320*3
        images[p.stem]=raw;png(p.with_suffix('.png'),240,320,raw)
    order=['library','confirm-cancel','auto-on','countdown-5','about','install']
    rows=[]
    for group in (order[:3],order[3:]):
        for y in range(320):rows.append(b''.join(images[name][y*720:(y+1)*720] for name in group))
    png(base/'Passport-Launcher-0.4.1-preview.png',720,640,b''.join(rows))

if __name__=='__main__':compose(sys.argv[1])
