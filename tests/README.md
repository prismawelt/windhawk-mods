# Translucent Windows GDI regressions

This fork keeps the mod's existing BGRA8 GDI/Direct2D rendering and native acrylic
backdrop. It repairs missing alpha in text, empty selection fills, constant-alpha
blends, dynamic DC_BRUSH foreground fills, and solid system carets. It also
separates ordinary legacy background erases from foreground drawing during
window paint messages. Unsupported ItemsView parts/states
fall back to native theming without indexing or poisoning a cache. Premultiplication,
BITMAPINFO initialization, and system-color restoration are corrected.

Keep **Windows theme custom rendering** enabled and **New system colors** off.
Public GetSysColor queries remain native in that mode; the mod's existing system
brushes still paint the glass background. This lets applications make normal
contrast decisions. The fixes use drawing contracts and do not test application
names, accent colors, or the display bit depth.

## Install a modified version as a local fork

Windhawk 1.7.3 identifies published mods by their installed id. A different source
version from its cached upstream latestVersion produces an update offer; a higher
custom version number does not prevent it. A GitHub fork alone does not change
that installed identity.

Use Windhawk's local-fork identity for the modified installation. On the affected
laptop, the source metadata id is translucent-windows-fork, its name is
Translucent Windows - Fork, and its installed id is local@translucent-windows-fork.
Both DLLs were rebuilt for that identity and existing settings were preserved.
The original installed id and its stale update entry were removed. Other mods
continue to receive their normal update checks.

This is an installation adaptation: only the source id/name differ from the
published fix in this repository. The rendering functions remain identical.

Save the installed source as UTF-8 **without a BOM**. Windhawk 1.7.3 anchors its
metadata opening marker at the start of a line; a BOM before the first marker
causes "Couldn't find a metadata block in the source code". Verify the source
with the installed UI metadata parser as well as the compiler: compiling alone
does not detect this UI loading failure.

## Reproduce the native checks

Use Windhawk's Windows compiler. From WSL, supply a Windows-drive temporary directory
already excluded from mod injection. The fixtures refuse to run when this mod is
injected; injecting the mod into its own test would invalidate the comparison.

    python3 tests/run-translucent-windows-rendering.py \
      --compiler '/mnt/c/Program Files/Windhawk/Compiler/bin/clang++.exe' \
      --temp-dir /path/to/excluded/windows-directory \
      --architectures 32 64

    for target in i686-w64-mingw32 x86_64-w64-mingw32; do
      python3 tests/run-translucent-windows-alpha.py \
        --compiler '/mnt/c/Program Files/Windhawk/Compiler/bin/clang++.exe' \
        --temp-dir /path/to/excluded/windows-directory --target "$target"
      python3 tests/run-translucent-windows-solid-fill.py \
        --compiler '/mnt/c/Program Files/Windhawk/Compiler/bin/clang++.exe' \
        --temp-dir /path/to/excluded/windows-directory --target "$target"
      python3 tests/run-translucent-windows-glass-paint.py \
        --compiler '/mnt/c/Program Files/Windhawk/Compiler/bin/clang++.exe' \
        --temp-dir /path/to/excluded/windows-directory --target "$target"
      python3 tests/run-translucent-windows-caret.py \
        --compiler '/mnt/c/Program Files/Windhawk/Compiler/bin/clang++.exe' \
        --temp-dir /path/to/excluded/windows-directory --target "$target"
    done

Each runner also accepts --source. In the standalone fix package, add
--source ./translucent-windows.wh.cpp when running from the package directory.
The rendering runner's --baseline accepts the unmodified mod source and verifies
the original text-alpha and ItemsView failures.

The runners extract production functions directly. Text, blend and fill tests use
native Windows GDI DIBs. ItemsView tests use a routing/cache fixture. Each architecture
passes 21 text, 8 ItemsView, 20 blend and 23 solid-fill checks: **144 checks total**
across 32-bit and 64-bit builds. They cover exact RGB/alpha, selection bounds,
clipping, mapping/world transforms, RTL fills, fallback behavior, source/DC state,
and GDI object stability over 1,000 repeated draws.

The caret fixture adds 35 checks per architecture. It uses real glass windows and
the same NtUser caret entry points called by native controls. It covers native
blinking, exact RGB/alpha on transparent, partial-alpha and opaque backgrounds,
movement, repainting, nested hide/show counts, allocation/subclass fallback,
owner destruction, cross-thread unload, and GDI object stability over 500 cycles.
Application bitmap carets and gray patterns retain their native behavior.

For an optional installed-mod check, add --injected-temp-dir with a Windows-drive
directory where mod injection is enabled. This runs a real Edit control twice:
the excluded baseline reproduces 00FFFFFF while the installed version produces
FFFFFFFF during native blinking. The binary must load a mod DLL with the version
from the supplied source. Both focus-cycle tests check for GDI leaks. Across both
architectures, the glass-paint checks below, and installed-control comparisons
on both architectures, 282 checks passed. The unchanged alpha-blend functions
retain their earlier 40 passing checks; their extracted production section is
byte-identical to version 1.8.2.13.

## Display and resource verification

On the affected laptop, Advanced Color remained enabled and output remained
10 bpc. The user confirmed Word selection backgrounds during the fix and Explorer's marquee
interior and original pink border on the physical display with production version 1.8.2.12.
The user also confirmed the Explorer rename insertion caret on the physical
display with version 1.8.2.13. With version 1.8.2.17, the user confirmed that the iCloud Photos Options
dialog retained its transparency and readable text without the reported fringes.
A live readback found zero pixels with RGB exceeding alpha in the dialog and its
visible children. Clipped or occluded readback regions can return zero, so this
measurement supplements the physical confirmation. Screen captures alone do not
validate this display-path issue.

The source remains BGRA8, as required by the existing DC render target. Display
output depth and source-buffer depth are different; this change does not create
FP16 surfaces or alter the Windows color pipeline.

Constant-alpha repair copies only the source crop. The observed Explorer call
uses 100 x 100 pixels (40,000 temporary pixel bytes), regardless of its scaled
destination size. Dynamic solid fills use a 1 x 1 pixel source (4 pixel bytes).
These figures exclude GDI objects and allocation overhead. Very large or unsupported
blend requests retain native rendering; the crop limit is 16 million pixels/64 MiB.

In the native 64-bit fixture, repairing the observed blend added about 60 microseconds
per call, and repairing four thin border fills added about 65 microseconds total.
Repeated tests leaked no GDI objects. These are local drawing microbenchmarks,
not whole-application CPU/GPU or battery measurements.

## Solid caret rendering

Windows draws a default solid caret with XOR. Repairing text/selection alpha does
not repair that primitive: the original caret in the native fixture is 00FFFFFF
while shown on a transparent black background. A 32-bit XOR bitmap supplies the
missing alpha mask. Each pixel uses backgroundAlpha XOR 255 in its alpha byte and
FFFFFF in its RGB bytes. Native drawing makes the shown caret opaque, and native
hiding restores the original RGB and alpha exactly, including opaque backgrounds.

Only the active solid caret of each participating GUI thread retains a small
bitmap and memory DC. The mask is refreshed while hidden at movement, show and
paint boundaries; it is deselected before Windows uses it for blinking. Windows
still handles blink timing, DPI, client clipping and position. No polling timer,
overlay window, full-window surface or FP16 renderer is added. The ordinary
3 x 24 test caret uses 288 pixel bytes; the actual Edit control used 1 x 32 pixels,
or 128 pixel bytes. GDI/DC and map bookkeeping are additional.

Native calls are intercepted in win32u when available because native controls
bypass user32's public wrappers. The window subclass restores the original caret
on unload and releases resources on destruction. Unsupported custom bitmap/gray
carets and resource failures use native rendering.

## Legacy background and foreground painting

Some legacy controls erase with ordinary RGB or PALETTERGB brushes through
NtGdiPatBlt/PATCOPY instead of the mod's intercepted system brushes. A neutral
background drawn that way can leave RGB with zero alpha and text composed against
a background that no longer matches the native acrylic backdrop.

Version 1.8.2.17 tracks WM_ERASEBKGND, WM_PAINT, WM_PRINT and WM_PRINTCLIENT with
per-window subclasses and a nested thread-local scope. Only eligible windows
participate. Neutral erases clear to transparent black, then mark the root window
as having a glass background. A neutral light whole-clip repaint also preserves
that backdrop; small foreground fills keep their RGB and receive opaque alpha.
PALETTERGB is normalized only on devices without a system palette. Patterns,
palette indices, unrelated raster operations and unowned drawing stay native.

Text uses the existing alpha mask and premultiplication code. On a mapped glass
background in dark mode, neutral dark text adapts its source color to the actual
local background; a single destination pixel distinguishes bright content from
the dark backdrop. Explicit colored text, white text and HDC text-color state are
preserved. ETO_OPAQUE background clears follow the same glass intent, while the
existing selection-highlight branch retains its foreground alpha.

The glass-paint fixture adds 30 checks per architecture. It uses real Windows
paint messages, subclasses, palette-relative brushes, foreground rectangles and
text-color queries against the extracted production functions. It checks
transparent background pixels, exact foreground RGB/alpha, nesting, native
fallbacks, settings and unload behavior, and stable GDI counts over 100 repaints.

This adds window/property bookkeeping and scoped drawing work, without polling,
a new renderer or a full-window buffer. Foreground fills reuse the existing
single-pixel helper. Disabling custom rendering restores native drawing; unloading
also removes the subclasses and properties. An experiment that made every
background opaque was rejected because it removed transparency; that behavior is
not in the final version.
