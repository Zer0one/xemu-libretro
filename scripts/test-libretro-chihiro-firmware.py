#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Native firmware lookup/cache tests. Optional read-only real MAME ZIP check."""
import argparse
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import zipfile
p=argparse.ArgumentParser();p.add_argument('--mame-bios',type=Path);p.add_argument('--report',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[1]
class Firmware(C.Structure):
    _fields_=[('path',(C.c_char*4096)*6),('source',(C.c_char*4352)*6)]
names=['chihiro_xbox_bios.bin','fpr21042_m29w160et.bin','ic10_g24lc64.bin','ic11_24lc024.bin','pc20_g24lc64.bin','ver1305.bin']
sizes=[524288,2097152,8192,128,8192,2097152]
blobs={n:bytes([i+1])*size for i,(n,size) in enumerate(zip(names,sizes))}
cc=shlex.split(os.environ.get('CC','cc'))
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','glib-2.0'],text=True))
results=[]
with tempfile.TemporaryDirectory(prefix='xemu-firmware-') as tmp:
 work=Path(tmp);(work/'libretro-profile-config.h').write_text('#define XEMU_LIBRETRO_PROFILE 2\n')
 subprocess.run(cc+['-std=gnu11','-fPIC','-w','-c',str(root/'util/miniz/miniz.c'),'-o',str(work/'miniz.o')],check=True)
 libpath=work/'fw.so'
 subprocess.run(cc+['-std=c11','-fPIC','-shared','-Wall','-Wextra','-Werror','-I'+str(work),'-I'+str(root),str(root/'ui/libretro-chihiro-firmware.c'),str(work/'miniz.o'),*flags,'-o',str(libpath)],check=True)
 lib=C.CDLL(str(libpath));resolve=lib.libretro_chihiro_firmware_resolve
 resolve.argtypes=[C.c_char_p]*5+[C.POINTER(Firmware),C.c_void_p,C.c_size_t];resolve.restype=C.c_bool
 fixture=work/'fixture'
 def reset():
  shutil.rmtree(fixture,ignore_errors=True)
  sys=fixture/'system';sys.mkdir(parents=True)
  chd=fixture/'roms'/'outr2'/'renamed.chd';chd.parent.mkdir(parents=True);chd.touch()
  return sys,chd
 def write(path,blob):path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(blob)
 def archive(path,contents=blobs):
  path.parent.mkdir(parents=True,exist_ok=True)
  with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_DEFLATED) as z:
   for name,raw in contents.items():z.writestr(name,raw)
 def run(sys,chd,ok=True,profile=b'xemu-chihiro'):
  fw=Firmware();error=C.create_string_buffer(256)
  assert bool(resolve(bytes(sys),profile,bytes(chd),b'outr2',b'',C.byref(fw),error,len(error)))==ok,error.value
  return fw
 def path(fw,i):return Path(bytes(fw.path[i]).split(b'\0')[0].decode())
 def source(fw,i):return bytes(fw.source[i]).split(b'\0')[0].decode()
 def passed(label):results.append({'test':label,'ok':True})
 for folder in ['xemu-chihiro','xemu-chihiro/Chihiro','xemu/Chihiro','']:
  sys,chd=reset();write(sys/folder/names[0],blobs[names[0]])
  fw=run(sys,chd);assert path(fw,0).samefile(sys/folder/names[0])
  passed('System loose '+folder)
 for where in range(2):
  for container in ['zip','directory']:
   sys,chd=reset();base=[chd.parent,chd.parent.parent][where]
   if container=='zip':archive(base/'outr2.zip')
   else:
    for n,b in blobs.items():write(base/'outr2'/n,b)
   fw=run(sys,chd)
   for i,n in enumerate(names):assert path(fw,i).read_bytes()==blobs[n]
   passed('Associated game '+container+' root '+str(where))
 for where in range(2):
  for container in ['zip','directory']:
   sys,chd=reset();base=[chd.parent,chd.parent.parent][where]
   for setname,contents in [('chihiro',{k:v for k,v in blobs.items() if k!='ver1305.bin'}),('seganetw',{'ver1305.bin':blobs['ver1305.bin']})]:
    if container=='zip':archive(base/(setname+'.zip'),contents)
    else:
     for n,b in contents.items():write(base/setname/n,b)
   fw=run(sys,chd)
   for i,n in enumerate(names):assert path(fw,i).read_bytes()==blobs[n]
   passed('Shared BIOS '+container+' root '+str(where))
 sys,chd=reset();archive(sys/'xemu-chihiro/chihiro.zip');fw=run(sys,chd)
 for i,n in enumerate(names[:5]):
  assert path(fw,i).read_bytes()==blobs[n]
  assert source(fw,i)==str(sys/'xemu-chihiro/chihiro.zip')+':'+n
 # An unrelated member in chihiro.zip must not supply the network board.
 assert not bytes(fw.path[5]).split(b'\0')[0]
 passed('System MAME Chihiro ZIP; network member is not used')
 archive(sys/'xemu-chihiro/seganetw.zip',{'ver1305.bin':blobs['ver1305.bin']})
 fw=run(sys,chd);assert 'seganetw.zip:ver1305.bin' in source(fw,5)
 assert path(fw,5).read_bytes()==blobs['ver1305.bin']
 passed('System MAME network ZIP resolved independently')
 # Separate precedence for each firmware, not one directory chosen by BIOS.
 sys,chd=reset();write(sys/'xemu-chihiro'/names[0],blobs[names[0]])
 archive(chd.parent.parent/'outr2.zip');archive(chd.parent/'chihiro.zip')
 fw=run(sys,chd);assert path(fw,0)==sys/'xemu-chihiro'/names[0]
 assert 'outr2.zip:' in source(fw,1);passed('System first; associated set before closer BIOS set')
 cache=path(fw,1);digest=hashlib.sha256(cache.read_bytes()).hexdigest()
 assert cache.parent.name==digest
 mtime=cache.stat().st_mtime_ns;fw2=run(sys,chd);assert path(fw2,1).stat().st_mtime_ns==mtime
 cache.write_bytes(b'x'*cache.stat().st_size);fw2=run(sys,chd);assert path(fw2,1).read_bytes()==blobs[names[1]]
 passed('Hash cache reused and repaired; originals remain unchanged')
 sys,chd=reset();write(sys/'xemu-full/Chihiro'/names[0],blobs[names[0]])
 assert path(run(sys,chd,profile=b'xemu-full'),0).samefile(sys/'xemu-full/Chihiro'/names[0]);passed('Full nested System directory')
 sys,chd=reset();archive(chd.parent/'outr2.zip',{'../../escape/'+names[0]:blobs[names[0]]})
 fw=run(sys,chd);assert path(fw,0).is_relative_to(sys/'xemu-chihiro/firmware-cache')
 assert not (fixture/'escape').exists();passed('ZIP member path never used as output path')
 sys,chd=reset();write(sys/'xemu-chihiro'/names[0],b'wrong size');run(sys,chd,False);passed('Wrong BIOS size rejected')
 sys,chd=reset();run(sys,chd,False);passed('Missing BIOS fails without creating cache')
 assert not (sys/'xemu-chihiro/firmware-cache').exists()
 if a.mame_bios:
  sys,chd=reset()
  for n in ['chihiro','seganetw']:(chd.parent.parent/(n+'.zip')).symlink_to((a.mame_bios/(n+'.zip')).resolve())
  fw=run(sys,chd);hashes={}
  for i,n in enumerate(names):
   with zipfile.ZipFile(a.mame_bios/('seganetw.zip' if i==5 else 'chihiro.zip')) as z:original=z.read(n)
   assert path(fw,i).read_bytes()==original
   hashes[n]=hashlib.sha256(original).hexdigest()
  results.append({'test':'Real MAME shared BIOS archives read without changes','ok':True,'sha256':hashes})
print(f'{len(results)} native firmware lookup/cache cases passed')
if a.report:a.report.write_text(json.dumps(results,indent=2)+'\n')
