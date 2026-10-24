#!/usr/bin/env python3
"""Extract depth expressions from the active core and archived candidate A.

The upstream geometry equations execute in the fragment stage as an oracle;
this is not execution of a geometry shader, nor Xbox hardware validation.
"""
import argparse, ast, hashlib, json, re, subprocess, tempfile
from pathlib import Path

parser=argparse.ArgumentParser()
parser.add_argument('output',type=Path)
args=parser.parse_args()
root=Path(__file__).resolve().parents[3]
out=args.output.resolve(); out.mkdir(parents=True,exist_ok=True)
psh=(root/'hw/xbox/nv2a/pgraph/glsl/psh.c').read_text()
# Keep the historical A/B experiments reproducible after the restricted W-only
# implementation. Reverse its PSH delta only in a disposable source copy.
if 'Match the upstream W expression without a geometry shader.' in psh:
    with tempfile.TemporaryDirectory(prefix='xemu-depth-baseline-') as temporary:
        target=Path(temporary)/'hw/xbox/nv2a/pgraph/glsl/psh.c'
        target.parent.mkdir(parents=True); target.write_text(psh)
        subprocess.run(['git','apply','--reverse',
                        '--include=hw/xbox/nv2a/pgraph/glsl/psh.c',
                        str(root/'docs/patches/vulkan-depth-candidates/03-w-only-neutral-host-z.patch')],
                       cwd=temporary,check=True)
        psh=target.read_text()
# Reconstruct only in memory; never apply the archived patch to active sources.
patch=root/'docs/patches/vulkan-depth-candidates/01-reciprocal-w-and-bias.patch'
with tempfile.TemporaryDirectory(prefix='xemu-depth-source-') as temporary:
    target=Path(temporary)/'hw/xbox/nv2a/pgraph/glsl/psh.c'
    target.parent.mkdir(parents=True); target.write_text(psh)
    subprocess.run(['git','apply',str(patch)],cwd=temporary,check=True)
    candidate=target.read_text()
def strings(source):
    return ''.join(ast.literal_eval(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"',source))
def branches(source):
    start=source.index('    if (ps->state->use_hw_depth) {',source.index('/*  Window-clip'))
    end=source.index('    /* Depth clipping */',start)
    section=source[start:end]
    fallback,ref=section.split('    } else if (ps->state->z_perspective) {',1)
    ref_w,ref_z=ref.split('    } else {',1)
    return fallback,strings(ref_w),strings(ref_z)
fallback,ref_w,ref_z=branches(psh)
candidate_fallback,_,_=branches(candidate)
w,z=candidate_fallback.split('        if (ps->state->z_perspective) {',1)[1].split('        } else {',1)
w,z=strings(w),strings(z)
common='''#version 450
layout(push_constant) uniform Parameters { vec4 vertex[3]; vec4 config; vec4 extra; } p;
#define surfaceScale p.config.xy
#define clipRange vec4(0.0,p.config.z,0.0,p.config.z)
#define depthFactor p.config.w
#define depthOffset p.extra.x
#define vtxPos0 p.vertex[0]
#define vtxPos1 p.vertex[1]
#define vtxPos2 p.vertex[2]
'''
vertex=common+'''void main() {
    vec4 v = p.vertex[gl_VertexIndex];
    gl_Position = vec4((2.0*v.xy/32.0-1.0)*v.w, (v.z/clipRange.y)*v.w, v.w);
}
'''
# Exact determinant and barycentric helpers from active upstream-derived code.
helper=psh[psh.index('        "float kahan_det(vec2'):psh.index('        "const float[9] gaussian3x3')]
geom=(root/'hw/xbox/nv2a/pgraph/glsl/geom.c').read_text()
start=geom.index('            "float kahan_det(float')
end=geom.index('        if (state->z_perspective)',start)
determinant=strings(geom[start:end])
start=geom.index('                "mat4 calc_triz(int i0',end)
endw=geom.index('        } else {',start)
endz=geom.index('    if (need_linez)',endw)
geom_w=strings(geom[start:endw]); geom_z=strings(geom[endw:endz])
geom_w=geom_w.replace('calc_triz(', 'calc_triz_w(')
geom_z=geom_z.replace('calc_triz(', 'calc_triz_z(')
functions=''
for name,body in [('baseline',strings(fallback)),('w_only',w.replace(' * surfaceScale.x','').replace(' * surfaceScale.y','')),('scaled_w',w),('scaled_z',z)]:
    functions+=f'float {name}() {{\n'+body+'return zvalue;\n}\n'
for name,body,mode in [('reference_w',ref_w,'w'),('reference_z',ref_z,'z')]:
    functions+=f'float {name}() {{\nfloat triMZ = calc_triz_{mode}(0,1,2)[3][0];\n'+body+'return zvalue;\n}\n'
fragment=common+'''layout(location=0) out vec4 rawDepth;
layout(location=1) out vec4 encodedDepth;
vec4 v_vtxPos[3];
'''+strings(helper)+determinant+geom_w+geom_z+functions+'''
float encode(float z) {
    z = clamp(z,0.0,clipRange.y);
    if (p.extra.z > 0.0) return uintBitsToFloat(floatBitsToUint(floor(z)/16777216.0)+1u);
    return floor(z)/65535.0;
}
void main() {
    v_vtxPos[0]=vtxPos0; v_vtxPos[1]=vtxPos1; v_vtxPos[2]=vtxPos2;
    float old = baseline();
    float onlyW = p.extra.y > 0.0 ? w_only() : old;
    float candidate = p.extra.y > 0.0 ? scaled_w() : scaled_z();
    float reference = p.extra.y > 0.0 ? reference_w() : reference_z();
    rawDepth=vec4(old,onlyW,candidate,reference);
    encodedDepth=vec4(encode(old),encode(onlyW),encode(candidate),encode(reference));
}
'''
(out/'probe.vert').write_text(vertex)
(out/'probe.frag').write_text(fragment)
inputs=['hw/xbox/nv2a/pgraph/glsl/psh.c','hw/xbox/nv2a/pgraph/glsl/geom.c','docs/patches/vulkan-depth-candidates/01-reciprocal-w-and-bias.patch']
if (root/'docs/patches/vulkan-depth-candidates/03-w-only-neutral-host-z.patch').exists():
    inputs.append('docs/patches/vulkan-depth-candidates/03-w-only-neutral-host-z.patch')
(out/'source-manifest.json').write_text(json.dumps({p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in inputs},indent=2)+'\n')
print('Extracted active baseline, archived candidate A, and upstream-derived reference equations.')
