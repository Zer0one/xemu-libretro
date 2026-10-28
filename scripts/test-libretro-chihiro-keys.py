#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Native key/ZIP tests; optional differential check against the donor script.
All fixtures are synthetic. No game keys are read or printed.
"""
import argparse
import ast
import ctypes as C
from pathlib import Path
import os
import random
import shlex
import subprocess
import tempfile
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('--reference', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
cc = shlex.split(os.environ.get('CC', 'cc'))
class Key(C.Structure):
    _fields_ = [('des', C.c_ubyte * 8), ('loader', C.c_char * 8), ('bios', C.c_bool)]

def data_file(key=b'12345678', loader=b'LOADER'):
    data = bytearray(80)
    data[49:56] = key[:7]; data[41] = key[7]
    data[33:33+len(loader)] = loader
    return bytes(data)

def pic_file(data):
    pic = bytearray(0x4000)
    for i, offset in enumerate((0x6e0,0x6bc,0x6aa,0x7de,0x7be,0x79e,0x77e,0x698,0x686,0x674)):
        for j in range(8): pic[offset+j*2] = data[i*8+j]
    return bytes(pic)

with tempfile.TemporaryDirectory(prefix='xemu-key-test-') as tmp:
    work = Path(tmp)
    subprocess.run(cc+['-std=gnu11','-fPIC','-w','-c',str(root/'util/miniz/miniz.c'),
                        '-o',str(work/'miniz.o')],check=True)
    subprocess.run(cc+['-std=c11','-fPIC','-Wall','-Wextra','-Werror','-c',
        '-I'+str(root),str(root/'ui/libretro-chihiro-keys.c'),'-o',str(work/'keys.o')],check=True)
    libpath=work/'keys.so'
    subprocess.run(cc+['-shared',str(work/'keys.o'),str(work/'miniz.o'),'-o',str(libpath)],check=True)
    lib=C.CDLL(str(libpath))
    parse=lib.xemu_chihiro_parse_key
    parse.argtypes=[C.c_void_p,C.c_size_t,C.c_bool,C.POINTER(Key)];parse.restype=C.c_bool
    read=lib.xemu_chihiro_read_keys
    read.argtypes=[C.c_char_p,C.POINTER(Key),C.c_size_t,C.POINTER(C.c_size_t)]
    read.restype=C.c_int
    def decode(data,pic=False):
        key=Key();ok=parse(data,len(data),pic,C.byref(key))
        return (bytes(key.des),bytes(key.loader)) if ok else (None,None)
    data=data_file();pic=pic_file(data)
    assert decode(data)==(b'12345678',b'LOADER')
    assert decode(pic,True)==decode(data)
    assert decode(pic+b'\x00'*16,True)==decode(data)
    for bad in (b'',data[:-1],data+b'\x00',data_file(b'\x00'*8),data_file(loader=b''),data_file(loader=b'\xff')):
        assert decode(bad)==(None,None)
    assert decode(pic[:-1],True)==(None,None)
    count_tests=10
    def check_file(path,result,expected=(),capacity=8):
        keys=(Key*8)();count=C.c_size_t(99)
        assert read(os.fsencode(path),keys,capacity,C.byref(count))==result, path.name
        assert count.value==len(expected)
        for i,(key,loader,bios) in enumerate(expected):
            assert (bytes(keys[i].des),bytes(keys[i].loader),keys[i].bios)==(key,loader,bios)
    check_file(work/'missing.data',1)
    check_file(work,1)
    (work/'game.data').write_bytes(data)
    check_file(work/'game.data',0,[(b'12345678',b'LOADER',False)])
    check_file(work/'game.data',2,capacity=0)
    (work/'game.pic').write_bytes(pic)
    check_file(work/'game.pic',0,[(b'12345678',b'LOADER',False)])
    (work/'bad.data').write_bytes(b'wrong')
    check_file(work/'bad.data',2)
    path=work/'game.zip'
    with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_DEFLATED) as z:
        z.writestr('bios/317-0352-exp.pic',pic_file(data_file(b'87654321',b'BIOS')))
        z.writestr('game.data',data)
        z.writestr('unrelated.txt','not a key')
    check_file(path,0,[(b'12345678',b'LOADER',False),(b'87654321',b'BIOS',True)])
    check_file(path,3,capacity=1)
    with zipfile.ZipFile(work/'bad.zip','w') as z:z.writestr('game.data',data)
    corrupt=bytearray((work/'bad.zip').read_bytes());corrupt[30+len('game.data')+49]^=1
    (work/'bad.zip').write_bytes(corrupt)
    check_file(work/'bad.zip',2)
    with zipfile.ZipFile(work/'large.zip','w') as z:
        for i in range(4097): z.writestr(str(i),b'')
    check_file(work/'large.zip',3)
    count_tests+=10
    differential=0
    if args.reference:
        # Load only the two inspected pure parsers, not the converter CLI.
        tree=ast.parse(args.reference.read_text())
        functions=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in ('read_data_file','read_pic_file')]
        assert len(functions)==2
        scope={};exec(compile(ast.Module(body=functions,type_ignores=[]),'<donor parsers>','exec'),scope)
        rng=random.Random(0x43484948)
        for _ in range(256):
            raw=data_file(rng.randbytes(8),bytes(rng.randrange(65,91) for _ in range(rng.randrange(1,8))))
            assert decode(raw)==tuple(v.encode() if isinstance(v,str) else v for v in scope['read_data_file'](raw))
            rawpic=pic_file(raw)
            assert decode(rawpic,True)==tuple(v.encode() if isinstance(v,str) else v for v in scope['read_pic_file'](rawpic))
            differential+=2
    print(f'{count_tests} native format/archive cases passed; {differential} donor differential comparisons passed')
