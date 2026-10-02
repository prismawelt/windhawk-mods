#!/usr/bin/env python3
"""Run native Windows checks for the actual mod's dynamic solid fill path."""
from pathlib import Path
import argparse,subprocess,tempfile,json,sys
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler',required=True)
p.add_argument('--target',default='x86_64-w64-mingw32')
p.add_argument('--source',type=Path,default=Path(__file__).resolve().parent.parent/'mods/translucent-windows.wh.cpp')
p.add_argument('--temp-dir',type=Path)
p.add_argument('--result',type=Path)
a=p.parse_args()
s=a.source.read_text(encoding='utf-8-sig')
helper=s.split('// ==LegacyGdiSolidFill==\n',1)[1].split('// ==/LegacyGdiSolidFill==',1)[0]
prefix=r"""
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <climits>
#include <initializer_list>
#define WH_MOD_ID L"translucent-windows-fixture"
#include <commctrl.h>
struct {bool FillBg=true;bool Unload=false;enum Type{Default,Blur}BgType=Blur;}g_settings;
bool g_IsSysThemeDarkMode=true;
BOOL IsWindowEligible(HWND h){return h!=nullptr;}
namespace WindhawkUtils {
using WH_SUBCLASSPROC=LRESULT(CALLBACK*)(HWND,UINT,WPARAM,LPARAM,DWORD_PTR);
BOOL SetWindowSubclassFromAnyThread(HWND,WH_SUBCLASSPROC,DWORD_PTR){return FALSE;}
void RemoveWindowSubclassFromAnyThread(HWND,WH_SUBCLASSPROC){}
}

static decltype(&FillRect) FillRect_orig=FillRect;
bool forceBlendFailure=false;
int lastWidth=0,lastHeight=0;
BOOL WINAPI FixtureAlphaBlend(HDC d,int x,int y,int w,int h,HDC s,int sx,int sy,int sw,int sh,BLENDFUNCTION b){
 lastWidth=sw;lastHeight=sh;
 if(forceBlendFailure){SetLastError(987654);return FALSE;}
 return AlphaBlend(d,x,y,w,h,s,sx,sy,sw,sh,b);
}
#define AlphaBlend FixtureAlphaBlend
"""
def wp(path):
 path=str(path.resolve())
 return path[5].upper()+':\\'+path[7:].replace('/','\\') if path.startswith('/mnt/') else path
with tempfile.TemporaryDirectory(prefix='.windhawk-solid-tests-',dir=a.temp_dir) as tmp:
 t=Path(tmp);cpp=t/'solid.cpp';exe=t/'solid.exe'
 body=(Path(__file__).resolve().parent/'translucent-windows-solid-fill.cpp').read_text(encoding='utf-8')
 context=s.split('// ==LegacyGdiGlassPaint==\n',1)[1].split('// ==/LegacyGdiGlassPaint==',1)[0]
 cpp.write_text(prefix+context+helper+'\n#undef AlphaBlend\n'+body,encoding='utf-8')
 r=subprocess.run([a.compiler,'-std=c++23','-O2','-target',a.target,wp(cpp),'-o',wp(exe),'-lgdi32','-lcomctl32','-lmsimg32','-static'],capture_output=True,timeout=120)
 print('compile',r.returncode,flush=True)
 if r.returncode:print(r.stderr.decode('utf-8','replace'));sys.exit(2)
 r=subprocess.run([str(exe)],capture_output=True,timeout=60);output=r.stdout.decode('utf-8','replace');print(output,flush=True)
 if r.stderr:print(r.stderr.decode('utf-8','replace'))
 if a.result:a.result.write_text(json.dumps({'passed':r.returncode==0,'output':output},indent=2)+'\n',encoding='utf-8')
 sys.exit(r.returncode)
