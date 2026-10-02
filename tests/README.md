# Translucent Windows GDI regressions

This fork keeps the mod's existing BGRA8 GDI/Direct2D rendering and native acrylic
backdrop. It repairs missing alpha in text, empty selection fills, constant-alpha
blends, and dynamic DC_BRUSH foreground fills. Unsupported ItemsView parts/states
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

## Display and resource verification

On the affected laptop, Advanced Color remained enabled and output remained
10 bpc. The user confirmed Word selection backgrounds during the fix and Explorer's marquee
interior and original pink border on the physical display with production version 1.8.2.12.
Screen captures alone do not validate this display-path issue.

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
