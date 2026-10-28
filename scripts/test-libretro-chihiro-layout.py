#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Run synthetic DIMM geometry/catalogue checks with ASan and UBSan."""
import argparse
import ctypes as C
import struct
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
parser=argparse.ArgumentParser()
parser.add_argument('--disc-library',type=Path)
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='chihiro-layout-') as tmp:
    exe=Path(tmp)/'test-layout'
    subprocess.run(shlex.split(os.environ.get('CC','cc'))+[
        '-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
        '-I'+str(root),str(root/'tests/libretro/test-chihiro-layout.c'),
        str(root/'hw/xbox/chihiro/chihiro-image-layout.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)

if args.disc_library:
    lib=C.CDLL(str(args.disc_library.resolve()))
    get=lib.libretro_chihiro_disc_dimm_mib
    get.argtypes=[C.c_char_p];get.restype=C.c_uint
    with tempfile.TemporaryDirectory(prefix='chihiro-raw-layout-') as tmp:
        path=Path(tmp)/'synthetic.bin'
        for ident in (b'SGBZ',b'ZZZZ'):
            for mib in (128,256,512,1024):
                image=bytearray(1024*1024)
                struct.pack_into('<4sIII',image,0,b'FATX',1,32,1)
                entries=((mib-16)*1024*1024)//16384+1
                data=4096+((entries*(2 if entries<0xfff0 else 4)+4095)//4096)*4096
                image[data]=7;image[data+2:data+9]=b'boot.id'
                struct.pack_into('<II',image,data+44,2,192)
                bid=data+16384;image[bid:bid+4]=b'BTID';image[bid+32:bid+36]=b'XBAM';image[bid+48:bid+52]=ident
                path.write_bytes(image)
                assert get(bytes(path))==mib
        path.write_bytes(b'FATX')
        assert get(bytes(path))==0
        assert get(bytes(Path(tmp)/'missing.bin'))==0
    print('10 raw-file adapter cases passed; identical file lengths select all four DIMM capacities')
