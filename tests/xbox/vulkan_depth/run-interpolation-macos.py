#!/usr/bin/env python3
"""Run the bounded interpolation experiment through Vulkan and native OpenGL."""
import argparse,hashlib,json,os,subprocess,sys
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',required=True,type=Path)
for name in ['vulkan-include','loader-dir','icd','layer-dir']:p.add_argument('--'+name,required=True,type=Path)
a=p.parse_args();root=Path(__file__).resolve().parents[3];tests=Path(__file__).resolve().parent;out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
subprocess.run([sys.executable,str(tests/'prepare-interpolation.py'),str(out)],check=True)
glslang=root/'subprojects/glslang'
commands=[['c++','-std=c++17','-O2','-I',str(a.vulkan_include.resolve()),'-I',str(glslang),str(tests/'interpolation-vk.cpp'),str(glslang/'glslang/ResourceLimits/ResourceLimits.cpp'),str(glslang/'glslang/ResourceLimits/resource_limits_c.cpp'),str(root/'build/macos-arm64/subprojects/glslang/libglslang.a'),'-L',str(a.loader_dir.resolve()),'-lvulkan','-o',str(out/'interpolation-vk')],['c++','-std=c++17','-O2','-Wno-deprecated-declarations',str(tests/'interpolation-gl.cpp'),'-framework','OpenGL','-o',str(out/'interpolation-gl')]]
with (out/'build.log').open('w') as log:
    for cmd in commands:subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=60)
env=os.environ.copy()
for k in list(env):
    if k.startswith(('DYLD_','VK_','MVK_')):env.pop(k)
env['XDG_CACHE_HOME']=str(out/'cache');gl_env=env.copy()
selected={'DYLD_LIBRARY_PATH':str(a.loader_dir.resolve()),'VK_DRIVER_FILES':str(a.icd.resolve()),'VK_LAYER_PATH':str(a.layer_dir.resolve()),'VK_INSTANCE_LAYERS':'VK_LAYER_KHRONOS_validation','VK_LAYER_VALIDATE_SYNC':'1','VK_LOADER_DEBUG':'error,warn','MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS':'1'};env.update(selected)
(out/'environment.json').write_text(json.dumps(selected,indent=2)+'\n')
provider=Path(json.loads(a.icd.read_text())['ICD']['library_path'])
if not provider.is_absolute():provider=a.icd.resolve().parent/provider
manifest={'build_commands':commands,'driver':str(provider.resolve()),'driver_sha256':hashlib.sha256(provider.read_bytes()).hexdigest(),'sources':{f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in tests.iterdir() if f.is_file()}}
(out/'test-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
for api,suffix,environment in [('vk','',env),('gl','-gl',gl_env)]:
    with (out/(api+'.log')).open('w') as log,(out/(api+'.json')).open('w') as stdout:
        subprocess.run([str(out/('interpolation-'+api)),str(out/('interpolation'+suffix+'.vert')),str(out/('interpolation'+suffix+'.frag')),str(out)],env=environment,stdout=stdout,stderr=log,check=True,timeout=60)
subprocess.run([sys.executable,str(tests/'analyze-interpolation.py'),str(out)],check=True)
