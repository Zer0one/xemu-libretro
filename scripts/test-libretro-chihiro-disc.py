#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Read/decrypt archived CHDs through the actual native disc adapter.

The supplied library is a focused test build exporting the adapter, not a
release core. The reference JSON contains chd, payload_bytes and
 decrypted_sha256 fields generated independently from the earlier probe.
All content is read in place; no decrypted output file or key bytes are saved.
"""
import argparse
from pathlib import Path
import ctypes as C,hashlib,json,time
parser=argparse.ArgumentParser()
parser.add_argument('--library',required=True,type=Path)
parser.add_argument('--references',required=True,type=Path)
parser.add_argument('--keys-dir',required=True,type=Path)
parser.add_argument('--report',required=True,type=Path)
args=parser.parse_args()
lib=C.CDLL(str(args.library.resolve()))
lib.libretro_chihiro_disc_prepare.argtypes=[C.c_char_p,C.c_char_p,C.c_void_p,C.c_size_t];lib.libretro_chihiro_disc_prepare.restype=C.c_bool
lib.libretro_chihiro_disc_size.argtypes=[C.c_char_p];lib.libretro_chihiro_disc_size.restype=C.c_int64
lib.libretro_chihiro_disc_read.argtypes=[C.c_char_p,C.c_uint64,C.c_void_p,C.c_size_t];lib.libretro_chihiro_disc_read.restype=C.c_bool
lib.libretro_chihiro_disc_close.restype=None
lib.libretro_chihiro_disc_dimm_mib.argtypes=[C.c_char_p]
lib.libretro_chihiro_disc_dimm_mib.restype=C.c_uint
keys=bytes(args.keys_dir)
results=[]
for ref in json.loads(args.references.read_text()):
 start=time.monotonic();path=ref['chd'].encode();error=C.create_string_buffer(256)
 assert lib.libretro_chihiro_disc_prepare(path,keys,error,len(error)),error.value
 size=lib.libretro_chihiro_disc_size(path);assert size==ref['payload_bytes']
 assert lib.libretro_chihiro_disc_dimm_mib(path)==ref.get('dimm_mib',512)
 assert lib.libretro_chihiro_disc_size(b'/other.chd')==-1
 digest=hashlib.sha256();buf=C.create_string_buffer(512*1024)
 for offset in range(0,size,len(buf)):
  n=min(len(buf),size-offset);assert lib.libretro_chihiro_disc_read(path,offset,buf,n)
  digest.update(buf.raw[:n])
 assert digest.hexdigest()==ref['decrypted_sha256']
 first=C.create_string_buffer(16);assert lib.libretro_chihiro_disc_read(path,0,first,16) and first.raw[:4]==b'FATX'
 for offset,n in [(1,9),(7,17),(65535,43),(size-9,9)]:
  check=C.create_string_buffer(n);assert lib.libretro_chihiro_disc_read(path,offset,check,n)
  aligned=offset//8*8;total=(offset-aligned+n+7)//8*8;other=C.create_string_buffer(total)
  assert lib.libretro_chihiro_disc_read(path,aligned,other,total)
  assert check.raw==other.raw[offset-aligned:offset-aligned+n]
 for offset,n in [(size,1),(2**64-1,9)]:assert not lib.libretro_chihiro_disc_read(path,offset,first,n)
 # Missing keys_dir is no longer a guaranteed failure: ROM-relative sets work.
 assert not lib.libretro_chihiro_disc_prepare(b'/__xemu_nonexistent__/missing.chd',keys,error,len(error))
 assert lib.libretro_chihiro_disc_size(path)==size
 lib.libretro_chihiro_disc_close();assert lib.libretro_chihiro_disc_size(path)==-1
 result={'chd':ref['chd'],'size':size,'sha256':digest.hexdigest(),'elapsed_s':round(time.monotonic()-start,3),'all_native':True}
 results.append(result);print(json.dumps(result),flush=True)
args.report.write_text(json.dumps(results,indent=2)+'\n')
