# Translucent Windows rendering regressions

This fork keeps the existing BGRA8 GDI/Direct2D rendering and native backdrop effects.
The changes repair unflagged text alpha, exact premultiplication, empty selection fills,
ItemsView fallback/cache bounds, and system-color restoration.

For compatibility, keep **Windows theme custom rendering** enabled and turn
**New system colors** off. In this mode public GetSysColor queries remain native,
while custom system brushes still paint the existing glass background. Applications
can make their normal contrast/theme decisions without changing the system palette.
No application-name exceptions or extra window-wide rendering buffers are introduced.

Run native Windows regressions with Windhawk's compiler. On WSL, use a temporary
directory on a Windows drive that is already excluded from mod injection:

    python3 tests/run-translucent-windows-rendering.py \
      --compiler '/mnt/c/Program Files/Windhawk/Compiler/bin/clang++.exe' \
      --temp-dir /path/to/excluded/windows-directory \
      --architectures 32 64

The --baseline option accepts the unmodified source and verifies that the original
text-alpha and unsupported ItemsView regressions reproduce. The runner extracts
production functions directly. Text tests use native GDI DIBs; ItemsView tests verify
routing, cache bounds and reuse with a cache test fixture.

The text fixture also reports per-call timing and checks GDI object stability over
1,000 repeated draws. These are local microbenchmarks, not whole-application CPU/GPU
or battery measurements.

Tested with Advanced Color enabled and a 10 bpc display. Word selection is confirmed
visible on the physical display. Explorer's marquee remains under investigation:
it appears in captures but the user reports it missing on the physical panel.
Screenshots alone cannot validate this display-path issue.
