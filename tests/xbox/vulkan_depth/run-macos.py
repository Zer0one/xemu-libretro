#!/usr/bin/env python3
"""Build and run the bounded Vulkan depth probe with explicitly selected tools."""
import argparse,hashlib,json,os,subprocess,sys
from pathlib import Path

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',required=True,type=Path)
parser.add_argument('--depth',action='store_true',help='Run real depth-buffer clipping/compare suite')
parser.add_argument('--vulkan-include',required=True,type=Path)
parser.add_argument('--loader-dir',required=True,type=Path)
parser.add_argument('--icd',required=True,type=Path)
parser.add_argument('--layer-dir',required=True,type=Path)
args=parser.parse_args()
root=Path(__file__).resolve().parents[3];tests=Path(__file__).resolve().parent
out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
paths=[args.vulkan_include/'vulkan/vulkan.h',args.loader_dir/'libvulkan.dylib',args.icd,args.layer_dir/'VkLayer_khronos_validation.json',root/'build/macos-arm64/subprojects/glslang/libglslang.a']
for path in paths:
    if not path.is_file():raise SystemExit(f'Missing dependency: {path}')
subprocess.run([sys.executable,str(tests/('prepare-depth.py' if args.depth else 'prepare.py')),str(out)],check=True)
glslang=root/'subprojects/glslang'
command=['c++','-std=c++17','-O2','-I',str(args.vulkan_include.resolve()),'-I',str(glslang),str(tests/('depth.cpp' if args.depth else 'probe.cpp')),str(glslang/'glslang/ResourceLimits/ResourceLimits.cpp'),str(glslang/'glslang/ResourceLimits/resource_limits_c.cpp'),str(paths[-1]),'-L',str(args.loader_dir.resolve()),'-lvulkan','-o',str(out/'probe')]
with (out/'build.log').open('w') as log:
    subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=60)
env=os.environ.copy()
for name in list(env):
    if name.startswith(('DYLD_','VK_','MVK_')):env.pop(name)
selected={'DYLD_LIBRARY_PATH':str(args.loader_dir.resolve()),'VK_DRIVER_FILES':str(args.icd.resolve()),'VK_LAYER_PATH':str(args.layer_dir.resolve()),'VK_INSTANCE_LAYERS':'VK_LAYER_KHRONOS_validation','VK_LAYER_VALIDATE_SYNC':'1','VK_LOADER_DEBUG':'error,warn','MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS':'1','XDG_CACHE_HOME':str(out/'cache')}
env.update(selected)
(out/'environment.json').write_text(json.dumps(selected,indent=2)+'\n')
icd=json.loads(args.icd.read_text());provider=Path(icd['ICD']['library_path'])
if not provider.is_absolute():provider=args.icd.resolve().parent/provider
provenance={'build_command':command,'driver':str(provider.resolve()),'driver_sha256':hashlib.sha256(provider.read_bytes()).hexdigest(),'test_sources':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in tests.iterdir() if p.is_file()}}
(out/'test-manifest.json').write_text(json.dumps(provenance,indent=2)+'\n')
with (out/'run.log').open('w') as log,(out/'gpu.json').open('w') as stdout:
    subprocess.run([str(out/'probe'),str(out/'probe.vert'),str(out/'probe.frag'),str(out)],env=env,stdout=stdout,stderr=log,check=True,timeout=60)
subprocess.run([sys.executable,str(tests/('analyze-depth.py' if args.depth else 'analyze.py')),str(out)],check=True)
