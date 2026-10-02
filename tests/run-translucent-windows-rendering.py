#!/usr/bin/env python3
"""Run native GDI regressions and test theme dispatch from the actual mod source."""
from pathlib import Path
import argparse, json, subprocess, tempfile, sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', required=True, help='Windhawk clang++ executable')
parser.add_argument('--source', type=Path,
                    default=Path(__file__).resolve().parent.parent/'mods/translucent-windows.wh.cpp')
parser.add_argument('--baseline', type=Path, help='Optional unmodified mod source')
parser.add_argument('--temp-dir', type=Path, help='Windows-accessible directory excluded from mod injection')
parser.add_argument('--result', type=Path)
parser.add_argument('--architectures', nargs='+', choices=['32', '64'], default=['64'])
args = parser.parse_args()
here = Path(__file__).resolve().parent

def windows_path(path):
    path = str(Path(path).resolve())
    return path[5].upper()+':\\'+path[7:].replace('/', '\\') if path.startswith('/mnt/') else path

prefix = r"""
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <uxtheme.h>
#include <tlhelp32.h>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#define RECTWIDTH(r) ((r)->right - (r)->left)
#define RECTHEIGHT(r) ((r)->bottom - (r)->top)
#define Wh_Log(...) std::fwprintf(stderr, __VA_ARGS__)
static decltype(&ExtTextOutW) ExtTextOutW_orig = &ExtTextOutW;
"""
cases = [('fixed', args.source)]
if args.baseline:
    cases.insert(0, ('upstream', args.baseline))
results = []
with tempfile.TemporaryDirectory(prefix='.windhawk-rendering-tests-', dir=args.temp_dir) as directory:
    directory = Path(directory)
    for label, source in cases:
        text = source.read_text(encoding='utf-8-sig')
        text_routines = text[text.index('static std::array<BYTE, 256> g_textAlphaGammaLUT'):
                             text.index('// Bypass alpha blending operation done by DrawTextWithGlow')]
        start = text.index('BOOL PaintItemsView(')
        items_routine = text[start:text.index('BOOL CThemeCache::CacheItemsView(', start)]
        items_fixture, items_body = (here/'translucent-windows-items-view.cpp').read_text().split('// ==TestBody==', 1)
        fixtures = [
            ('text', prefix+text_routines+(here/'translucent-windows-text.cpp').read_text()),
            ('items-view', prefix+items_fixture+items_routine+items_body),
        ]
        for arch in args.architectures:
            for name, code in fixtures:
                cpp = directory/f'{label}-{name}-{arch}.cpp'
                exe = cpp.with_suffix('.exe')
                cpp.write_text(code)
                target = 'x86_64-w64-mingw32' if arch == '64' else 'i686-w64-mingw32'
                compile_result = subprocess.run([
                    args.compiler, '-std=c++23', '-O2', '-target', target, windows_path(cpp),
                    '-o', windows_path(exe), '-lgdi32', '-luxtheme', '-lmsimg32', '-static'
                ], capture_output=True, timeout=120)
                if compile_result.returncode:
                    print(compile_result.stderr.decode('utf-8', 'replace'))
                    sys.exit(2)
                run = subprocess.run([str(exe)], capture_output=True, timeout=30)
                output = run.stdout.decode('utf-8', 'replace')
                print(f'{label} {name} {arch}-bit\n{output}', flush=True)
                results.append(dict(source=label, fixture=name, architecture=arch,
                                    passed=run.returncode == 0, output=output))
                if label == 'fixed' and run.returncode:
                    sys.exit('Fixed regression fixture failed')
                if label == 'upstream':
                    expected = ('FAIL zero-options-text-has-valid-alpha' if name == 'text'
                                else 'FAIL unimplemented-part-reaches-native-rendering')
                    if expected not in output:
                        sys.exit('Baseline did not reproduce the regression')
if args.result:
    args.result.write_text(json.dumps(dict(fixedPassed=True, fixtures=results),
                                     ensure_ascii=False, indent=2)+'\n')
