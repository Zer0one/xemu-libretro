#!/usr/bin/env python3
"""Measure Z invariance and homogeneous-scale conditioning in GPU interpolants."""
import argparse,array,json,math,struct
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('directory',type=Path);a=p.parse_args();out=a.directory
metadata={api:json.loads((out/(api+'.json')).read_text()) for api in ['vk','gl']}
f32=lambda v:struct.unpack('f',struct.pack('f',v))[0]
def load(case):
    data=array.array('f');data.frombytes((out/case['file']).read_bytes());assert len(data)==case['width']*case['height']*4;return data
cache={api:{c['name']:load(c) for c in m['cases']} for api,m in metadata.items()}
def w_values(v):return [f32(1/v[0]),f32(1/v[1]),v[2],v[3]]
def stats(values):return {'count':len(values),'changed':sum(v!=0 for v in values),'max_abs':max(values,default=0),'mean_abs':sum(values)/len(values) if values else 0}
rows=[];invariance=[];conditioning=[];cross=[]
for api,m in metadata.items():
    cases={c['name']:c for c in m['cases']}
    for name,c in cases.items():
        data=cache[api][name];covered=0;errors=[[],[],[]];qerr=[]
        for i in range(c['width']*c['height']):
            raw=data[4*i:4*i+4]
            if raw[3]<0:continue
            assert all(math.isfinite(v) and v>0 for v in raw),(api,name,i,raw)
            covered+=1;w=w_values(raw)
            for j in range(3):errors[j].append(abs(w[j]-w[3]))
            qerr.append(abs(raw[0]-1/raw[3])/(1/raw[3]))
        rows.append({'api':api,**c,'covered':covered,'builtin_w_error':stats(errors[0]),'affine_w_error':stats(errors[1]),'perspective_w_error':stats(errors[2]),'builtin_q_relative_error':stats(qerr)})
        if c['zmode']:
            base_name=name.rsplit('-',1)[0]+'-base';base=cache[api][base_name]
            errors=[[],[],[],[]];qchanged=0
            for i in range(c['width']*c['height']):
                v=data[4*i:4*i+4];b=base[4*i:4*i+4]
                if v[3]<0 or b[3]<0:continue
                qchanged+=v[0]!=b[0];w=w_values(v);bw=w_values(b)
                for j in range(4):errors[j].append(abs(w[j]-bw[j]))
            invariance.append({'api':api,'case':name,'base':base_name,'builtin_q_changed':qchanged,**{key:stats(errors[j]) for j,key in enumerate(['builtin_w','affine_w','perspective_w','reference_w'])}})
        if c['homogeneous']:
            old_name=name.replace('-power2-','-original-').replace('-normalized-','-original-').replace('-neutral-','-original-');old=cache[api][old_name];errors=[]
            for i in range(c['width']*c['height']):
                v=data[4*i:4*i+4];b=old[4*i:4*i+4]
                if v[3]>0 and b[3]>0:errors.append(abs(w_values(v)[0]-w_values(b)[0]))
            conditioning.append({'api':api,'case':name,'original':old_name,'builtin_w':stats(errors)})
for c in metadata['vk']['cases']:
    v=cache['vk'][c['name']];g=cache['gl'][c['name']];diff=[];coverage_diff=0
    for i in range(c['width']*c['height']):
        a=v[4*i:4*i+4];b=g[4*i:4*i+4];coverage_diff+=(a[3]>0)!=(b[3]>0)
        if a[3]>0 and b[3]>0:diff.append(abs(w_values(a)[0]-w_values(b)[0]))
    cross.append({'case':c['name'],'coverage_differences':coverage_diff,'builtin_w':stats(diff)})
report={'devices':{api:m['device'] for api,m in metadata.items()},'cases_per_api':{api:len(m['cases']) for api,m in metadata.items()},'validation_errors':metadata['vk']['validation_errors'],'gl_errors':metadata['gl']['gl_errors'],'rows':rows,'z_invariance':invariance,'homogeneous_conditioning':conditioning,'cross_api':cross,'limits':['Synthetic no-bias triangle; not a full emulator or Xbox hardware test.','GL uses its required [-W,+W] clip Z convention, Vulkan uses [0,W].','Cross-API coverage/precision may differ; invariance is measured separately within each API.']}
(out/'interpolation-analysis.json').write_text(json.dumps(report,indent=2)+'\n')
print('Devices:',report['devices'],'cases:',report['cases_per_api'],'Vulkan errors:',report['validation_errors'],'GL errors:',report['gl_errors'])
for r in invariance:
    if '-clamp1-' in r['case'] and r['case'].startswith('s1x1-') and r['case'].endswith('-far'):
        print(r['api'],r['case'],'changed q:',r['builtin_q_changed'],'W differences builtin/affine/perspective/reference:',*(r[k]['max_abs'] for k in ['builtin_w','affine_w','perspective_w','reference_w']))
assert report['validation_errors']==0 and report['gl_errors']==0

# These controls validate the fixture, not universal emulator correctness.
assert all(r['reference_w']['max_abs']==0 for r in invariance)
assert all(r['builtin_w']['max_abs']==0 and r['affine_w']['max_abs']==0 and r['perspective_w']['max_abs']==0 for r in invariance if '-neutral-' in r['case'])
