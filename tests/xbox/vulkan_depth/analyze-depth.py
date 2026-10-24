#!/usr/bin/env python3
"""Compare actual depth writes and EQUAL/LESS/GREATER replay masks."""
import argparse,array,json,math,struct
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('directory',type=Path);a=p.parse_args();out=a.directory
gpu=json.loads((out/'gpu.json').read_text());rows=[]
arms=['baseline','w_only','scaled_after_discard','scaled_before_discard','reference']
for case in gpu['cases']:
    n=case['width']*case['height'];depth_bytes=n*(4 if case['d24'] else 2);stride=n*16+depth_bytes
    payload=(out/(case['name']+'.writes')).read_bytes();masks=(out/(case['name']+'.compare')).read_bytes()
    assert len(payload)==stride*5 and len(masks)==n*15
    color=[];depth=[];coverage=[]
    write_errors=[]
    for j in range(5):
        raw=array.array('f');raw.frombytes(payload[j*stride:j*stride+n*16]);color.append(raw)
        d=array.array('f' if case['d24'] else 'H');d.frombytes(payload[j*stride+n*16:(j+1)*stride]);depth.append(d)
        covered=[raw[4*i+2]>.5 for i in range(n)];coverage.append(covered)
        err=[]
        for i,c in enumerate(covered):
            if not c:continue
            encoded=raw[4*i+1]
            expected=encoded if case['d24'] else min(65535,max(0,math.floor(encoded*65535+.5)))
            err.append(abs(d[i]-expected))
        write_errors.append(max(err,default=0))
    ref=4;metrics={};comparison_errors=0
    for j,name in enumerate(arms):
        shared=[i for i in range(n) if coverage[j][i] and coverage[ref][i]]
        missing=sum(coverage[j][i]!=coverage[ref][i] for i in range(n))
        differences=[abs(depth[j][i]-depth[ref][i])*(16777216 if case['d24'] else 1) for i in shared]
        cmp={}
        for op,label in enumerate(['equal','less','greater']):
            actual=masks[(j*3+op)*n:(j*3+op+1)*n];unexpected=0;passed=0
            for i in range(n):
                expected=False
                if coverage[j][i]:
                    reference=depth[ref][i] # clear depth is included outside seed coverage
                    incoming=depth[j][i]
                    expected=[incoming==reference,incoming<reference,incoming>reference][op]
                passed+=bool(actual[i]);unexpected+=bool(actual[i])!=expected
            comparison_errors+=unexpected
            cmp[label]={'passed':passed,'unexpected_from_stored_depth':unexpected}
        metrics[name]={'covered':sum(coverage[j]),'coverage_difference':missing,'depth_unequal':sum(v!=0 for v in differences),'mean_depth_difference_units':sum(differences)/len(differences) if differences else 0,'max_depth_difference_units':max(differences,default=0),'max_write_error':write_errors[j],'comparisons':cmp}
    boundary_diff=sum(depth[2][i]!=depth[3][i] for i in range(n) if coverage[2][i] and coverage[3][i])
    nonfinite=sum(not math.isfinite(color[j][4*i]) for j in range(5) for i in range(n) if coverage[j][i])
    rows.append({**case,'metrics':metrics,'comparison_errors':comparison_errors,'pre_post_discard_depth_differences':boundary_diff,'nonfinite_raw':nonfinite})
invariance=[]
# A W-only draw should not derive W from the guest Z plane. Compare matching
# ordinary/far-Z inputs: only vertex 1's Z changes, with identical XY and W.
for case in gpu['cases']:
    if case['shape']!='ordinary' or not case['wmode'] or case['bias'] or case['mask'] or case.get('guest_depth_cull',0):continue
    far=next(c for c in gpu['cases'] if c['name']==case['name'].replace('-ordinary-','-far-clip-'))
    n=case['width']*case['height'];stride=n*(20 if case['d24'] else 18)
    source=(out/(case['name']+'.writes')).read_bytes();changed=(out/(far['name']+'.writes')).read_bytes()
    arm_stats={}
    for arm in [2,3,4]:
        original=array.array('f');original.frombytes(source[arm*stride:arm*stride+n*16])
        modified=array.array('f');modified.frombytes(changed[arm*stride:arm*stride+n*16])
        errors=[abs(original[4*i]-modified[4*i]) for i in range(n) if original[4*i+2]>.5 and modified[4*i+2]>.5]
        arm_stats[arms[arm]]={'raw_depth_changed':sum(e!=0 for e in errors),'max_raw_difference':max(errors,default=0)}
    invariance.append({'ordinary':case['name'],'changed_vertex_z':far['name'],'metrics':arm_stats})
report={'upstream_depth_equality_qualification':'fail','device':gpu['device'],'cases':len(rows),'d16_format':gpu['d16_format'],'d24_host_format':gpu['d24_host_format'],'host_depth_clamp':gpu.get('host_depth_clamp',0),'validation_errors':gpu['validation_errors'],'comparison_errors':sum(r['comparison_errors'] for r in rows),'pre_post_discard_depth_differences':sum(r['pre_post_discard_depth_differences'] for r in rows),'nonfinite_raw':sum(r['nonfinite_raw'] for r in rows),'rows':rows,'w_depth_z_invariance':invariance,'limits':['Reference equations execute in fragment stage, not the upstream GS pipeline or Xbox hardware.','Triangle cases only; no lines, converted quads, guest programs, sample shading, or real game scene.','Post-discard derivatives are undefined across nonuniform termination; observed equality on one driver is not a portability proof.']}
(out/'depth-analysis.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k not in ['rows','limits','w_depth_z_invariance']},indent=2))
for shape in sorted({r['shape'] for r in rows}):
    subset=[r for r in rows if r['shape']==shape]
    print(shape,'reference coverage',sum(r['metrics']['reference']['covered'] for r in subset),'candidate unequal',sum(r['metrics']['scaled_before_discard']['depth_unequal'] for r in subset),'comparison errors',sum(r['comparison_errors'] for r in subset))
# API misuse or compare behavior unexplained by readback requires investigation.
assert gpu['validation_errors']==0
assert report['nonfinite_raw']==0

assert report['comparison_errors']==0
assert all(not r['metrics']['reference']['covered'] for r in rows if r['shape'] in ['all-negative-w','degenerate'])
assert all(r['metrics'][name]['max_write_error']==0 for r in rows for name in arms)
assert all(r['metrics']['scaled_before_discard']['coverage_difference']==0 for r in rows)
