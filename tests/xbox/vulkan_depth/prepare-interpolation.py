#!/usr/bin/env python3
"""Read built-in, affine and perspective interpolants without guest bias/discard."""
import argparse,subprocess,sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('output',type=Path);a=p.parse_args();out=a.output.resolve()
subprocess.run([sys.executable,str(Path(__file__).with_name('prepare-depth.py')),str(out)],check=True)
common='''#version 450
layout(push_constant) uniform Parameters { vec4 vertex[3]; vec4 config; vec4 extra; vec4 clipControl; } p;
'''
vertex=common+'''layout(location=0) noperspective out float affineInvW;
layout(location=1) out float perspectiveW;
void main() {
    vec4 v=p.vertex[gl_VertexIndex];
    vec4 pos=vec4((2.0*v.xy/32.0-1.0)*v.w,(v.z/p.config.z)*v.w,v.w);
    if (p.clipControl.w>0.0) pos.z=0.5*pos.w;
    gl_Position=pos*p.clipControl.z;
    affineInvW=1.0/v.w;
    perspectiveW=v.w;
}
'''
fragment=(out/'probe.frag').read_text()
fragment=fragment[:fragment.index('void main() {')]+'''layout(location=0) noperspective in float affineInvW;
layout(location=1) in float perspectiveW;
void main() {
    v_vtxPos[0]=vtxPos0;v_vtxPos[1]=vtxPos1;v_vtxPos[2]=vtxPos2;
    rawDepth=vec4(gl_FragCoord.w*p.clipControl.z,affineInvW,perspectiveW,reference_w());
    gl_FragDepth=encode(reference_w());
}
'''
(out/'interpolation.vert').write_text(vertex);(out/'interpolation.frag').write_text(fragment)
for suffix,source in [('vert',vertex),('frag',fragment)]:
    s=source.replace('#version 450','#version 410 core')
    s=s.replace('layout(push_constant) uniform Parameters { vec4 vertex[3]; vec4 config; vec4 extra; vec4 clipControl; } p;','struct Parameters { vec4 vertex[3]; vec4 config; vec4 extra; vec4 clipControl; };\nuniform Parameters p;')
    s=s.replace('gl_VertexIndex','gl_VertexID')
    s=s.replace('    gl_Position=pos*p.clipControl.z;','    pos.z=2.0*pos.z-pos.w;\n    gl_Position=pos*p.clipControl.z;')
    (out/('interpolation-gl.'+suffix)).write_text(s)
