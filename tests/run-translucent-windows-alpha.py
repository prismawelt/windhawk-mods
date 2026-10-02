#!/usr/bin/env python3
"""Run native Windows alpha-blend checks extracted from the actual mod source."""
from pathlib import Path
import argparse,subprocess,tempfile,json,sys
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',required=True);p.add_argument('--target',default='x86_64-w64-mingw32');p.add_argument('--source',type=Path,default=Path(__file__).resolve().parent.parent/'mods/translucent-windows.wh.cpp');p.add_argument('--body',type=Path,default=Path(__file__).resolve().parent/'translucent-windows-alpha-blend.cpp');p.add_argument('--temp-dir',type=Path);p.add_argument('--result',type=Path);a=p.parse_args()
s=a.source.read_text(encoding='utf-8-sig');helper=s.split('// ==LegacyGdiAlphaBlend==\n',1)[1].split('// ==/LegacyGdiAlphaBlend==',1)[0]
prefix=r"""
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
struct {bool FillBg=true;bool Unload=false;enum Type{Default,Blur}BgType=Blur;}g_settings;
"""
def wp(path):
 path=str(path.resolve());return path[5].upper()+':\\'+path[7:].replace('/','\\') if path.startswith('/mnt/') else path
with tempfile.TemporaryDirectory(prefix='.windhawk-alpha-tests-',dir=a.temp_dir) as tmp:
 t=Path(tmp);cpp=t/'alpha.cpp';exe=t/'alpha.exe';cpp.write_text(prefix+helper+a.body.read_text(encoding='utf-8'),encoding='utf-8')
 r=subprocess.run([a.compiler,'-std=c++23','-O2','-target',a.target,wp(cpp),'-o',wp(exe),'-lgdi32','-lmsimg32','-static'],capture_output=True,timeout=120);print('compile',r.returncode,flush=True)
 if r.returncode:print(r.stderr.decode('utf-8','replace'));sys.exit(2)
 r=subprocess.run([str(exe)],capture_output=True,timeout=60);output=r.stdout.decode('utf-8','replace');print(output,flush=True)
 if r.stderr:print(r.stderr.decode('utf-8','replace'))
 if a.result:a.result.write_text(json.dumps({'passed':r.returncode==0,'output':output},indent=2)+'\n',encoding='utf-8')
 sys.exit(r.returncode)
