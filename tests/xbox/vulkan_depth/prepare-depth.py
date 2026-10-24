#!/usr/bin/env python3
"""Extend the extracted shader equations to real depth-write/compare probes."""
import argparse,subprocess,sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('output',type=Path);a=p.parse_args()
out=a.output.resolve();subprocess.run([sys.executable,str(Path(__file__).with_name('prepare.py')),str(out)],check=True)
# Arm 3 evaluates the candidate before discard; arm 2 preserves its original
# after-discard placement. Other depth expressions are identical to probe.frag.
for name in ['probe.vert','probe.frag']:
    path=out/name;s=path.read_text().replace('vec4 extra; } p;','vec4 extra; vec4 clipControl; } p;')
    if name.endswith('frag'):
        s=s[:s.index('void main() {')]+'''void main() {
    v_vtxPos[0]=vtxPos0; v_vtxPos[1]=vtxPos1; v_vtxPos[2]=vtxPos2;
    int arm=int(p.extra.w);
    float depth=0.0;
    if (arm==3) depth=p.extra.y>0.0 ? scaled_w() : scaled_z();
    if (p.clipControl.x>0.0 &&
        (gl_FragCoord.x/surfaceScale.x < 13.25 ||
         gl_FragCoord.y/surfaceScale.y < 6.75)) discard;
    if (arm==0) depth=baseline();
    else if (arm==1) depth=p.extra.y>0.0 ? w_only() : baseline();
    else if (arm==2) depth=p.extra.y>0.0 ? scaled_w() : scaled_z();
    else if (arm==4) depth=p.extra.y>0.0 ? reference_w() : reference_z();
    if (p.clipControl.y>0.0 && (depth<0.0 || clipRange.y<depth)) discard;
    rawDepth=vec4(depth,encode(depth),1.0,1.0);
    encodedDepth=vec4(0.0);
    gl_FragDepth=encode(depth);
}
'''
        # Real depth suite uses one color attachment, unlike the arithmetic probe.
        s=s.replace('layout(location=1) out vec4 encodedDepth;\n','').replace('    encodedDepth=vec4(0.0);\n','')
    path.write_text(s)
