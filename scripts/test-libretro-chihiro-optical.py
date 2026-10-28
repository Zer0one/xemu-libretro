#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Synthetic optical/container and native key-to-FATX checks; no game copies.

--library is a focused library linked from the production adapter objects.
The DES encryption oracle uses CommonCrypto on macOS; reader tests are portable.
"""
import argparse
import ctypes as C
import os
from pathlib import Path
import struct
import sys
import tempfile
import zipfile

p = argparse.ArgumentParser()
p.add_argument('--library', required=True, type=Path)
a = p.parse_args()
lib = C.CDLL(str(a.library.resolve()))
def bind(name, args, result):
    f = getattr(lib, name); f.argtypes = args; f.restype = result; return f
op = bind('xemu_chihiro_optical_open', [C.c_char_p,C.c_void_p,C.c_size_t], C.c_void_p)
close = bind('xemu_chihiro_optical_close', [C.c_void_p], None)
read = bind('xemu_chihiro_optical_read', [C.c_void_p,C.c_uint64,C.c_void_p,C.c_size_t], C.c_bool)
size = bind('xemu_chihiro_optical_size', [C.c_void_p], C.c_uint64)
base = bind('xemu_chihiro_optical_base', [C.c_void_p], C.c_uint32)
prepare = bind('libretro_chihiro_disc_prepare', [C.c_char_p,C.c_char_p,C.c_void_p,C.c_size_t],C.c_bool)
disc_read = bind('libretro_chihiro_disc_read',[C.c_char_p,C.c_uint64,C.c_void_p,C.c_size_t],C.c_bool)
disc_close = bind('libretro_chihiro_disc_close',[],None)
dimm = bind('libretro_chihiro_disc_dimm_mib',[C.c_char_p],C.c_uint)
warning = bind('libretro_chihiro_disc_warning',[],C.c_char_p)
source = bind('libretro_chihiro_disc_key_source',[],C.c_char_p)

def volume(lba=0):
    image=bytearray(64*2048); image[16*2048:16*2048+7]=b'\1CD001\1'
    struct.pack_into('<I',image,16*2048+158,lba+20)
    struct.pack_into('<I',image,16*2048+166,2048)
    return image

def raw(image,unit):
    if unit==2048:return image
    return b''.join(bytes(16)+image[i:i+2048]+bytes(unit-2064) for i in range(0,len(image),2048))

with tempfile.TemporaryDirectory(prefix='xemu-optical-') as tmp:
    w=Path(tmp); error=C.create_string_buffer(512);count=0
    image=volume(45000)
    def check(path,expected=image,lba=45000):
        global count
        d=op(os.fsencode(path),error,len(error));assert d,(path,error.value)
        try:
            assert base(d)==lba and size(d)==len(expected)
            for offset,n in [(0,len(expected)),(2041,34),(len(expected)-9,9)]:
                buf=C.create_string_buffer(n);assert read(d,offset,buf,n)
                assert buf.raw==expected[offset:offset+n],(path,offset)
            buf=C.create_string_buffer(8)
            assert not read(d,len(expected),buf,1)
            assert not read(d,2**64-1,buf,8)
        finally:close(d)
        count+=1
    def bad(path):
        global count
        d=op(os.fsencode(path),error,len(error))
        if d:close(d)
        assert not d,(path,error.value)
        count+=1
    for unit in [2048,2352]:
        track=w/f'Track data {unit}.bin';track.write_bytes(raw(image,unit))
        check(track)
        cue=w/f'one{unit}.cue';cue.write_text(f'FILE "{track.name}" BINARY\n TRACK 03 MODE1/{unit}\n INDEX 01 00:00:00\n')
        check(cue)
        # INDEX00 pregap occupies bytes, PREGAP alone does not.
        track.write_bytes(bytes(2*unit)+raw(image,unit))
        cue.write_text(f'FILE "{track.name}" BINARY\nTRACK 03 MODE1/{unit}\nINDEX 00 00:00:00\nINDEX 01 00:00:02\nPOSTGAP 00:00:01\n')
        check(cue)
        # GDI offset points to the actual first sector.
        gdi=w/f'{unit}.gdi'
        (w/'audio.bin').write_bytes(bytes(2352))
        (w/'low.bin').write_bytes(bytes(2048))
        gdi.write_text(f'3\n1 0 4 2048 low.bin 0\n2 100 0 2352 audio.bin 0\n3 45000 4 {unit} "{track.name}" {2*unit}\n')
        check(gdi)
    # All tracks in one raw BIN; next INDEX00 bounds the selected volume.
    shared=w/'shared.bin';shared.write_bytes(bytes(2352)+raw(image,2352)+bytes(2*2352))
    cue=w/'shared.cue';cue.write_text('FILE "shared.bin" BINARY\nTRACK 02 AUDIO\nINDEX 01 00:00:00\nTRACK 03 MODE1/2352\nINDEX 01 00:00:01\nTRACK 04 AUDIO\nINDEX 00 00:00:65\nINDEX 01 00:00:66\n')
    check(cue)
    # Redump multi-CUE high-density numbering can restart at track01.
    (w/'hi.bin').write_bytes(raw(image,2352))
    cue=w/'density.cue';cue.write_text('\ufeffREM SINGLE-DENSITY AREA\r\nFILE "low.bin" BINARY\r\nTRACK 01 MODE1/2048\r\nINDEX 01 00:00:00\r\nREM HIGH-DENSITY AREA\r\nFILE "hi.bin" BINARY\r\nTRACK 01 MODE1/2352\r\nPREGAP 00:02:00\r\nINDEX 01 00:00:00\r\n')
    check(cue)
    # CHD v5 uncompressed map, all supported metadata forms, stored pregap.
    for tag,meta,skip in [
        (b'CHGD','TRACK:1 TYPE:MODE1_RAW SUBTYPE:NONE FRAMES:64 PAD:0 PREGAP:0 PGTYPE:MODE1 PGSUB:NONE POSTGAP:0',0),
        (b'CHT2','TRACK:1 TYPE:MODE1_RAW SUBTYPE:NONE FRAMES:66 PREGAP:2 PGTYPE:VMODE1_RAW PGSUB:NONE POSTGAP:0',2),
        (b'CHTR','TRACK:1 TYPE:MODE1_RAW SUBTYPE:NONE FRAMES:64',0)]:
        data=bytes(skip*2448)+raw(image,2448);unit=2448;hunk=unit*4
        data=data.ljust((len(data)+hunk-1)//hunk*hunk,b'\0');n=len(data)//hunk
        header=bytearray(124);header[:8]=b'MComprHD';struct.pack_into('>II',header,8,124,5)
        struct.pack_into('>QQQII',header,32,len(data),124,124+4*n,hunk,unit)
        m=meta.encode()+b'\0';md=struct.pack('>IIQ',int.from_bytes(tag,'big'),len(m),0)+m
        blocks=1+(124+4*n+len(md))//hunk
        packed=bytes(header)+b''.join(struct.pack('>I',blocks+i) for i in range(n))+md
        path=w/(tag.decode()+'.chd');path.write_bytes(packed.ljust(blocks*hunk,b'\0')+data)
        check(path)
    # Ordinary ISO extents start at0.
    plain=volume();path=w/'ordinary.iso';path.write_bytes(plain);check(path,plain,0)
    for text in [
        'FILE "missing.bin" BINARY\nTRACK 03 MODE1/2352\nINDEX 01 00:00:00\n',
        'FILE "hi.bin" BINARY\nTRACK 03 MODE2/2352\nINDEX 01 00:00:00\n',
        'FILE "hi.bin" BINARY\nTRACK 03 MODE1/2352\nINDEX 01 99:00:00\n',
        'FILE "hi.bin" BINARY\nTRACK 03 MODE1/2352\n',
        'FILE "hi.bin BINARY\nTRACK 03 MODE1/2352\nINDEX 01 00:00:00\n']:
        path=w/'invalid.cue';path.write_text(text);bad(path)
    for text in ['100\n','1\n1 45000 4 2352 missing.bin 0\n','1\n1 45000 4 2352 hi.bin 99999999999999999\n']:
        path=w/'invalid.gdi';path.write_text(text);bad(path)
    path=w/'empty.iso';path.touch();bad(path)
    path.write_bytes(b'MComprHD'+bytes(116));bad(path)
    # Native ISO -> key -> DES -> unique DIMM/FATX/boot.id, without game data.
    if sys.platform=='darwin':
        crypto=C.CDLL('/usr/lib/libSystem.B.dylib')
        crypt=crypto.CCCrypt
        crypt.argtypes=[C.c_uint,C.c_uint,C.c_uint,C.c_void_p,C.c_size_t,C.c_void_p,C.c_void_p,C.c_size_t,C.c_void_p,C.c_size_t,C.POINTER(C.c_size_t)]
        crypt.restype=C.c_int
        cluster=4096; entries=((128-16)<<20)//cluster+1
        dataoff=4096+((entries*2+4095)//4096)*4096
        payload=bytearray(dataoff+2*cluster)
        struct.pack_into('<4sIII',payload,0,b'FATX',1,8,1)
        payload[dataoff:dataoff+9]=b'\7\0boot.id'
        struct.pack_into('<II',payload,dataoff+44,2,192)
        payload[dataoff+cluster:dataoff+cluster+4]=b'BTID'
        payload[dataoff+cluster+32:dataoff+cluster+36]=b'XBAM'
        payload[dataoff+cluster+48:dataoff+cluster+52]=b'TEST'
        key=b'12345678'
        swap=lambda b:b''.join(b[i:i+8][::-1] for i in range(0,len(b),8))
        out=C.create_string_buffer(len(payload));moved=C.c_size_t()
        assert crypt(0,1,2,key[::-1],8,None,swap(payload),len(payload),out,len(payload),C.byref(moved))==0
        encrypted=swap(out.raw)
        disc=volume(45000);disc.extend(bytes(128*2048-len(disc)))
        def record(name,lba,length):
            n=33+len(name);n+=(n%2)
            r=bytearray(n);r[0]=n;r[32]=len(name);r[33:33+len(name)]=name
            struct.pack_into('<I',r,2,lba);struct.pack_into('>I',r,6,lba)
            struct.pack_into('<I',r,10,length);struct.pack_into('>I',r,14,length)
            return r
        directory=record(b'LOADER;1',45021,224)+record(b'PAYLOAD.BIN;1',45022,len(encrypted))
        disc[20*2048:20*2048+len(directory)]=directory
        disc[21*2048+192:21*2048+203]=b'PAYLOAD.BIN'
        disc[22*2048:22*2048+len(encrypted)]=encrypted
        track=w/'encrypted track.bin';track.write_bytes(raw(disc,2352))
        cue=w/'Synthetic Game v1.0.cue';cue.write_text('FILE "encrypted track.bin" BINARY\nTRACK 03 MODE1/2352\nINDEX 01 00:00:00\n')
        gdi=w/'Synthetic Game v1.0.gdi';gdi.write_text('1\n1 45000 4 2352 "encrypted track.bin" 0\n')
        keys=w/'system/keys';keys.mkdir(parents=True)
        redump=keys.parent/'redump';redump.mkdir()
        data=bytearray(80);data[49:56]=key[:7];data[41]=key[7];data[33:39]=b'LOADER'
        # Wrong but parsable higher-priority key must fall through.
        wrong=bytearray(data);wrong[49:56]=b'7654321'
        (w/(cue.stem+'.data')).write_bytes(wrong)
        zpath=redump/(cue.stem+'.zip')
        with zipfile.ZipFile(zpath,'w') as z:
            z.writestr('unrelated-name.data',data)
        for path in [cue,gdi]:
            assert prepare(os.fsencode(path),os.fsencode(keys),error,len(error)),error.value
            try:
                assert dimm(os.fsencode(path))==128
                result=C.create_string_buffer(len(payload))
                assert disc_read(os.fsencode(path),0,result,len(payload))
                assert result.raw==payload
                assert source().decode()==str(zpath)+':unrelated-name.data'
                assert b'uncatalogued' in warning()
                # Failed replacement preserves the previous valid source.
                assert not prepare(os.fsencode(w/'missing.cue'),os.fsencode(keys),error,len(error))
                assert dimm(os.fsencode(path))==128
            finally:disc_close()
            count+=1
    print(f'{count} native optical/container and end-to-end synthetic checks passed')
