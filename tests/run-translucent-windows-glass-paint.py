#!/usr/bin/env python3
"""Exercise native WM_ERASEBKGND/WM_PAINT scopes, transparency, and local contrast."""
from pathlib import Path
import argparse,subprocess,tempfile,json,sys
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler',required=True)
p.add_argument('--target',default='x86_64-w64-mingw32')
p.add_argument('--source',type=Path,default=Path(__file__).resolve().parent.parent/'mods/translucent-windows.wh.cpp')
p.add_argument('--temp-dir',type=Path)
p.add_argument('--result',type=Path)
a=p.parse_args();s=a.source.read_text()
context=s.split('// ==LegacyGdiGlassPaint==\n',1)[1].split('// ==/LegacyGdiGlassPaint==',1)[0]
solid=s.split('// ==LegacyGdiSolidFill==\n',1)[1].split('// ==/LegacyGdiSolidFill==',1)[0]
fixture=(Path(__file__).resolve().parent/'translucent-windows-glass-paint.cpp').read_text()
prefix,body=fixture.split('// ==TestBody==',1)
def wp(p):
 v=str(p.resolve());return v[5].upper()+':\\'+v[7:].replace('/','\\')
with tempfile.TemporaryDirectory(prefix='.windhawk-glass-tests-',dir=a.temp_dir) as tmp:
 t=Path(tmp);cpp=t/'glass.cpp';exe=t/'glass.exe';cpp.write_text(prefix+context+solid+body)
 r=subprocess.run([a.compiler,'-std=c++23','-O2','-target',a.target,wp(cpp),'-o',wp(exe),'-lgdi32','-lcomctl32','-lmsimg32','-static'],capture_output=True,timeout=120)
 if r.returncode:print(r.stderr.decode('utf-8','replace'));sys.exit(2)
 r=subprocess.run([str(exe)],capture_output=True,timeout=30);output=r.stdout.decode('utf-8','replace');print(output)
 if r.stderr:print(r.stderr.decode('utf-8','replace'))
 if a.result:a.result.write_text(json.dumps({'passed':r.returncode==0,'output':output},indent=2)+'\n')
 sys.exit(r.returncode)
