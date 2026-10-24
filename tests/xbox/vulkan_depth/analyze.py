#!/usr/bin/env python3
"""Analyze raw/encoded fragment values; this does not test depth attachments."""
import argparse,array,json,math,struct
from pathlib import Path
parser=argparse.ArgumentParser();parser.add_argument('directory',type=Path);args=parser.parse_args()
out=args.directory;gpu=json.loads((out/'gpu.json').read_text())
rows=[]
for case in gpu['cases']:
    values=array.array('f');values.frombytes((out/case['file']).read_bytes())
    n=case['width']*case['height'];assert len(values)==n*8
    maxz=16777215 if case['d24'] else 65535
    # Match the C++ push constants' FP32 products, then use double arithmetic.
    f32=lambda v:struct.unpack('f',struct.pack('f',v))[0]
    vertex=[(2.25,3,f32(f32(.12)*maxz),f32(f32(.10)*maxz)),(29,4.25,f32(f32(.73)*maxz),f32(f32(.65)*maxz)),(5,28,f32(f32(.42)*maxz),f32(f32(.32)*maxz))]
    x0,y0=vertex[0][:2];dx1=vertex[1][0]-x0;dy1=vertex[1][1]-y0;dx2=vertex[2][0]-x0;dy2=vertex[2][1]-y0;det=dx1*dy2-dy1*dx2
    plane=[1/v[3] if case['wmode'] else v[2] for v in vertex]
    slope_x=((plane[1]-plane[0])*dy2-(plane[2]-plane[0])*dy1)/det
    slope_y=((plane[2]-plane[0])*dx1-(plane[1]-plane[0])*dx2)/det
    factor=[0,1,-.25][case['bias']];offset=0 if not case['bias'] else .75
    errors=[[],[],[]];quantized=[0,0,0];cpu_errors=[];samples=0;nonfinite=0
    for i in range(n):
        raw=values[4*i:4*i+4]
        if raw[3]<0:continue
        x=(i%case['width']+.5)/case['scale'][0];y=(i//case['width']+.5)/case['scale'][1]
        b1=((x-x0)*dy2-(y-y0)*dx2)/det;b2=((y-y0)*dx1-(x-x0)*dy1)/det;b0=1-b1-b2
        if min(b0,b1,b2)<.04:continue
        if not all(math.isfinite(v) for v in raw):nonfinite+=1;continue
        samples+=1
        interp=b0*plane[0]+b1*plane[1]+b2*plane[2]
        ideal=1/interp if case['wmode'] else interp
        ideal+=offset+factor*max(abs(slope_x),abs(slope_y))*(ideal*ideal if case['wmode'] else 1)
        cpu_errors.append(abs(raw[3]-ideal))
        encoded=values[4*n+4*i:4*n+4*i+4]
        for j in range(3):
            errors[j].append(abs(raw[j]-raw[3]));quantized[j]+=encoded[j]!=encoded[3]
    assert samples>100
    stats=lambda e:{'mean_abs':sum(e)/len(e),'max_abs':max(e),'p99_abs':sorted(e)[min(len(e)-1,int(.99*len(e)))]}
    rows.append({**case,'interior_samples':samples,'nonfinite':nonfinite,'baseline':stats(errors[0]),'w_only':stats(errors[1]),'scaled':stats(errors[2]),'encoded_mismatches':dict(zip(['baseline','w_only','scaled'],quantized)),'reference_vs_cpu_double':stats(cpu_errors)})
# Bounded gates: prove the two targeted improvements, not bit-exact fidelity.
assert gpu['validation_errors']==0
assert all(r['nonfinite']==0 for r in rows)
for r in rows:
    maxz=16777215 if r['d24'] else 65535
    assert r['scaled']['max_abs'] < maxz*1e-5, r['file']
    assert r['reference_vs_cpu_double']['max_abs'] < maxz*1e-5, r['file']
    if r['wmode']:
        assert r['scaled']['mean_abs'] < r['baseline']['mean_abs']/1000, r['file']
    if r['bias'] and r['scale'] != [1,1]:
        unscaled='w_only' if r['wmode'] else 'baseline'
        assert r['scaled']['mean_abs'] < r[unscaled]['mean_abs']/1000, r['file']
report={'bounded_gates':'pass','device':gpu['device'],'cases':len(rows),'interior_samples':sum(r['interior_samples'] for r in rows),'validation_errors':gpu['validation_errors'],'rows':rows,'limits':['Synthetic ordinary positive-W triangles only; no guest scene or Xbox hardware reference.','Upstream GS equations run in fragment stage, not an actual geometry shader.','Outputs are raw and encoded fragment values in RGBA32F; depth attachments and equality tests are not exercised.','Edge/helper behavior, clipping, degenerate primitives and other devices remain unqualified.']}
(out/'analysis.json').write_text(json.dumps(report,indent=2)+'\n')
print('Device:',report['device'],'Cases:',len(rows),'Interior samples:',report['interior_samples'],'Validation errors:',report['validation_errors'])
for r in rows:
    if r['scale'] in [[1,1],[4,2]] and r['bias'] in [0,1]:
        print(r['file'],'mean abs baseline/W-only/scaled:',*(round(r[k]['mean_abs'],6) for k in ['baseline','w_only','scaled']),'encoded mismatches:',r['encoded_mismatches'])
