#!/usr/bin/env python3
"""Run native Windows rendering/lifetime checks against the mod's caret code."""
from pathlib import Path
import argparse,subprocess,tempfile,json,sys,re,shutil
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler',required=True)
p.add_argument('--target',default='x86_64-w64-mingw32')
p.add_argument('--source',type=Path,default=Path(__file__).resolve().parent.parent/'mods/translucent-windows.wh.cpp')
p.add_argument('--temp-dir',type=Path)
p.add_argument('--result',type=Path)
p.add_argument('--injected-temp-dir',type=Path,help='Optionally verify the installed mod using a real Edit control; this directory must allow injection.')
a=p.parse_args()
s=a.source.read_text(encoding='utf-8-sig')
helper=s.split('// ==LegacyGdiCaret==\n',1)[1].split('// ==/LegacyGdiCaret==',1)[0]
prefix=r"""
#define UNICODE
#define _UNICODE
#define WH_MOD_ID L"translucent-windows-caret-fixture"
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <tlhelp32.h>
#include <cstdio>
#include <algorithm>
#include <memory>
#include <vector>
#include <unordered_map>
#include <climits>
#include <initializer_list>
struct {bool FillBg=true;bool Unload=false;enum Type{Default,Blur}BgType=Blur;}g_settings;
BOOL IsWindowEligible(HWND window){return window!=nullptr;}
bool forceDcFailure=false,forceSubclassFailure=false;
HDC FixtureCreateDC(HDC dc){return forceDcFailure?nullptr:CreateCompatibleDC(dc);}
BOOL FixtureSubclass(HWND h,SUBCLASSPROC fn,UINT_PTR id,DWORD_PTR data){return forceSubclassFailure?FALSE:SetWindowSubclass(h,fn,id,data);}
#define CreateCompatibleDC FixtureCreateDC
#define SetWindowSubclass FixtureSubclass
"""
def wp(path):
 path=str(path.resolve())
 return path[5].upper()+':\\'+path[7:].replace('/','\\') if path.startswith('/mnt/') else path
with tempfile.TemporaryDirectory(prefix='.windhawk-caret-tests-',dir=a.temp_dir) as tmp:
 t=Path(tmp);cpp=t/'caret.cpp';exe=t/'caret.exe'
 body=(Path(__file__).resolve().parent/'translucent-windows-caret.cpp').read_text(encoding='utf-8')
 cpp.write_text(prefix+helper+'\n#undef CreateCompatibleDC\n#undef SetWindowSubclass\n'+body,encoding='utf-8')
 r=subprocess.run([a.compiler,'-std=c++23','-O2','-DWINVER=0x0A00','-D_WIN32_WINNT=0x0A00','-target',a.target,wp(cpp),'-o',wp(exe),'-lcomctl32','-ldwmapi','-lgdi32','-static'],capture_output=True,timeout=120)
 print('compile',r.returncode,flush=True)
 if r.returncode:print(r.stderr.decode('utf-8','replace'));sys.exit(2)
 r=subprocess.run([str(exe)],capture_output=True,timeout=60);output=r.stdout.decode('utf-8','replace');print(output,flush=True)
 if r.stderr:print(r.stderr.decode('utf-8','replace'))
 report={'passed':r.returncode==0,'output':output}
 if r.returncode==0 and a.injected_temp_dir:
  control=t/'caret-control.exe'
  control_source=(Path(__file__).resolve().parent/'translucent-windows-caret-control.cpp').read_bytes()
  c=subprocess.run([a.compiler,'-std=c++23','-O2','-DWINVER=0x0A00','-D_WIN32_WINNT=0x0A00','-target',a.target,'-x','c++','-','-o',wp(control),'-lcomctl32','-ldwmapi','-lgdi32','-static'],input=control_source,capture_output=True,timeout=120)
  if c.returncode:print(c.stderr.decode('utf-8','replace'));sys.exit(2)
  version=re.search(r'^// @version\s+(\S+)',s,re.M).group(1)
  with tempfile.TemporaryDirectory(prefix='.windhawk-caret-control-',dir=a.injected_temp_dir) as injected:
   copy=Path(injected)/control.name;shutil.copyfile(control,copy)
   integration=[]
   for mode,program,args in [('baseline',control,[]),('installed',copy,[version])]:
    c=subprocess.run([str(program),*args],capture_output=True,timeout=30)
    captured=c.stdout.decode('utf-8','replace');print(mode,c.returncode,captured,flush=True)
    if c.stderr:print(c.stderr.decode('utf-8','replace'))
    integration.append({'mode':mode,'passed':c.returncode==0,'output':captured})
   report['integration']=integration
   report['passed']=all(item['passed'] for item in integration)
 if a.result:a.result.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 sys.exit(0 if report['passed'] else 1)
