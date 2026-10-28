#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise the actual resolver with synthetic keys and MAME directory layouts."""
import ctypes as C
import hashlib
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import zipfile

root = Path(__file__).resolve().parents[1]
cc = shlex.split(os.environ.get('CC', 'cc'))
flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'glib-2.0'], text=True))
class Key(C.Structure):
    _fields_ = [('des', C.c_ubyte * 8), ('loader', C.c_char * 8), ('bios', C.c_bool)]
class Media(C.Structure):
    _fields_ = [(n, C.c_char_p) for n in ('disc', 'disc_sha1', 'set', 'parent', 'key', 'key_sha1')]
class Found(C.Structure):
    _fields_ = [('keys', Key * 16), ('verified', C.c_bool * 16),
                ('source', (C.c_char * 4352) * 16), ('sha1', (C.c_char * 41) * 16), ('count', C.c_size_t)]
def data_file(des=b'12345678'):
    data = bytearray(80)
    data[49:56] = des[:7]; data[41] = des[7]; data[33:39] = b'LOADER'
    return bytes(data)
def pic_file(data):
    pic = bytearray(0x4000)
    for i, offset in enumerate((0x6e0,0x6bc,0x6aa,0x7de,0x7be,0x79e,0x77e,0x698,0x686,0x674)):
        for j in range(8): pic[offset+j*2] = data[i*8+j]
    return bytes(pic)
with tempfile.TemporaryDirectory(prefix='xemu-search-') as tmp:
    work = Path(tmp)
    subprocess.run(cc + ['-std=gnu11','-fPIC','-w','-c',str(root/'util/miniz/miniz.c'),'-o',str(work/'miniz.o')],check=True)
    shared = work/'search.so'
    subprocess.run(cc + ['-std=c11','-fPIC','-shared','-Wall','-Wextra','-Werror',
        '-I'+str(root),str(root/'ui/libretro-chihiro-keys.c'),str(root/'ui/libretro-chihiro-key-search.c'),
        str(work/'miniz.o'),*flags,'-o',str(shared)],check=True)
    lib = C.CDLL(str(shared))
    find = lib.xemu_chihiro_find_keys
    find.argtypes = [C.c_char_p,C.c_char_p,C.POINTER(Media),C.POINTER(Found)]; find.restype = C.c_bool
    lookup = lib.xemu_chihiro_media_lookup
    lookup.argtypes = [C.c_char_p,C.c_void_p]; lookup.restype = C.POINTER(Media)
    sha = bytes.fromhex('055a13a5dc4f54e6b6bdf5ce29dbda14cc9741d7')
    assert lookup(b'renamed.chd',sha).contents.set == b'outr2'
    assert lookup(b'GDX-0004A.CHD',bytes(20)).contents.set == b'outr2'
    assert lookup(b'gdx-0011.chd',bytes(20)).contents.parent == b'outr2st'
    assert not lookup(b'unknown.chd',bytes(20))
    # SHA identity wins over a misleading basename.
    assert lookup(b'gdx-0011.chd',sha).contents.set == b'outr2'
    cases = 5
    good = data_file(); other = data_file(b'87654321'); pic = pic_file(other)
    media = Media(b'gdx-test', b'0'*40, b'game', b'parent', b'317-test.data', hashlib.sha1(good).hexdigest().encode())
    fixture = work/'fixture'
    def reset():
        shutil.rmtree(fixture,ignore_errors=True)
        chd = fixture/'roms'/'game'/'gdx-test.chd'
        chd.parent.mkdir(parents=True); chd.touch()
        keys = fixture/'system'/'keys'; keys.mkdir(parents=True)
        return chd,keys
    def put(path, raw):
        path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(raw)
    def archive(path, members):
        path.parent.mkdir(parents=True,exist_ok=True)
        with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_DEFLATED) as z:
            for name,raw in members: z.writestr(name,raw)
    def check(chd,keys,expected,verified=True,custom=media):
        out=Found()
        assert find(os.fsencode(chd),os.fsencode(keys),C.byref(custom) if custom else None,C.byref(out))
        assert out.count >= 1
        assert bytes(out.source[0]).split(b'\0')[0].decode() == str(expected)
        assert out.verified[0] == verified
        return out
    # All three roots, each with ZIP / associated directory / loose root.
    for which in range(3):
        for place in range(3):
            chd,keys=reset(); base=[chd.parent,chd.parent.parent,keys][which]
            if place == 0:
                target=base/'game.zip'; archive(target,[('renamed.data',good)])
                expected=str(target)+':renamed.data'
            else:
                target=base/('game' if place == 1 else '')/'renamed.data'; put(target,good)
                expected=target
            check(chd,keys,expected); cases+=1
    chd,keys=reset()
    archive(chd.parent/'game.zip',[('317-test.data',other)])
    put(keys/'renamed.data',good)
    check(chd,keys,keys/'renamed.data'); cases+=1  # Global hash pass.
    chd,keys=reset()
    for base in [chd.parent,chd.parent.parent,keys]: put(base/'317-test.data',good)
    check(chd,keys,chd.parent/'317-test.data'); cases+=1
    chd,keys=reset()
    archive(chd.parent/'game.zip',[('317-test.data',good)])
    put(chd.parent/'game'/'317-test.data',good)
    put(chd.parent/'317-test.data',good)
    check(chd,keys,str(chd.parent/'game.zip')+':317-test.data'); cases+=1
    # Name fallback is nonfatal and flagged; DATA precedes PIC even in reverse ZIP order.
    chd,keys=reset()
    archive(keys/'game.zip',[('317-test.pic',pic),('317-test.data',other)])
    out=check(chd,keys,str(keys/'game.zip')+':317-test.data',False)
    assert bytes(out.keys[0].des)==b'87654321'; cases+=1
    # A differing PIC remains available if a name-matched DATA cannot decrypt.
    chd,keys=reset()
    put(keys/'game.data',other); put(keys/'game.pic',pic_file(good))
    out=check(chd,keys,keys/'game.data',False)
    assert out.count==2 and bytes(out.keys[1].des)==b'12345678'; cases+=1
    # Merged clone keys and legacy short names.
    chd,keys=reset(); archive(keys/'parent.zip',[('renamed.data',good)])
    check(chd,keys,str(keys/'parent.zip')+':renamed.data'); cases+=1
    chd,keys=reset(); put(keys/'game.data',good)
    check(chd,keys,keys/'game.data'); cases+=1
    # Unknown images are handled only by the explicit image-name fallback.
    chd,keys=reset(); put(keys/'gdx-test.data',other); out=Found()
    assert not find(os.fsencode(chd),os.fsencode(keys),None,C.byref(out)); cases+=1
    chd,keys=reset(); out=Found()
    assert not find(os.fsencode(chd),os.fsencode(keys),C.byref(media),C.byref(out)); cases+=1
    # Malformed higher-priority container does not hide a complete valid source.
    chd,keys=reset(); put(chd.parent/'game.zip',b'bad zip'); put(keys/'renamed.data',good)
    check(chd,keys,keys/'renamed.data'); cases+=1
    # Flat content layout.
    chd,keys=reset(); flat=chd.parent.parent/'renamed.chd'; flat.touch()
    archive(flat.parent/'game.zip',[('317-test.data',good)])
    check(flat,keys,str(flat.parent/'game.zip')+':317-test.data'); cases+=1
    # Renamed, unknown-hash files must not be guessed just because they parse.
    chd,keys=reset(); put(keys/'unrelated.data',other)
    assert not find(os.fsencode(chd),os.fsencode(keys),C.byref(media),C.byref(out)); cases+=1
    # Original dump SHA-1, including optional PIC tail, is recorded unchanged.
    chd,keys=reset(); raw=pic_file(good)+bytes(16); put(keys/'game.pic',raw)
    out=check(chd,keys,keys/'game.pic',False)
    assert bytes(out.sha1[0]).split(b'\0')[0].decode()==hashlib.sha1(raw).hexdigest(); cases+=1
    # Real MAME OutRun 2 ZIP includes >8 MiB of drive-board firmware.
    chd,keys=reset()
    archive_path=keys/'game.zip'
    with zipfile.ZipFile(archive_path,'w') as z:
        z.writestr('board.bin', bytes(9*1024*1024))
        z.writestr('317-test.data',good)
    check(chd,keys,str(archive_path)+':317-test.data'); cases+=1
    callback_type=C.CFUNCTYPE(C.c_bool,C.c_void_p,C.POINTER(Key),C.c_char_p,C.c_char_p,C.c_bool)
    generic=lib.xemu_chihiro_try_image_keys
    generic.argtypes=[C.c_char_p,C.c_char_p,C.POINTER(Media),callback_type,C.c_void_p]
    generic.restype=C.c_bool
    def image_check(chd,keys,expected,accept=b'12345678'):
        seen=[]
        @callback_type
        def check_key(_,key,source,sha1,verified):
            seen.append(source.decode())
            return bytes(key.contents.des)==accept
        assert generic(os.fsencode(chd),os.fsencode(keys),None,check_key,None)==(expected is not None)
        if expected is not None: assert seen[-1]==str(expected),seen
        return seen
    for which in range(3):
        for place in range(3):
            chd,keys=reset(); chd=chd.with_name('OutRun 2 (Japan) v1.0.chd'); chd.touch()
            base=[chd.parent,keys,keys.parent/'redump'][which]; name=chd.stem
            if place==0:
                target=base/(name+'.zip'); archive(target,[('odd-name.pic',pic_file(good)),('different.data',good)])
                expected=str(target)+':different.data'
            elif place==1:
                target=base/name/'arbitrary.data'; put(target,good); expected=target
            else:
                target=base/(name+'.data'); put(target,good);expected=target
            image_check(chd,keys,expected);cases+=1
    # No parent-root search or unrelated loose key guessing in the ISO route.
    chd,keys=reset();put(chd.parent.parent/(chd.stem+'.data'),good);put(keys/'unrelated.data',good)
    image_check(chd,keys,None);cases+=1
    # Invalid early keys do not hide the 21st candidate, including in a ZIP.
    for zip_container in [False,True]:
        chd,keys=reset(); members=[(f'{i:02}.data',other) for i in range(20)]+[('last.data',good)]
        if zip_container:
            target=keys/(chd.stem+'.zip'); archive(target,members);expected=str(target)+':last.data'
        else:
            target=keys/chd.stem
            for n,raw in members:put(target/n,raw)
            expected=target/'last.data'
        assert len(image_check(chd,keys,expected))==21;cases+=1
    # Root and container precedence, then fallback when all earlier keys fail.
    chd,keys=reset(); archive(chd.parent/(chd.stem+'.zip'),[('wrong.data',other)])
    put(keys/(chd.stem+'.data'),good);put(keys.parent/'redump'/(chd.stem+'.data'),good)
    image_check(chd,keys,keys/(chd.stem+'.data'));cases+=1
    print(f'{cases} native media identity and key search cases passed')
