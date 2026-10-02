
struct Surface {
    static constexpr int width = 420, height = 160;
    HDC dc = CreateCompatibleDC(nullptr);
    RGBQUAD* pixels = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ oldBitmap = nullptr, oldFont = nullptr;
    HFONT font = nullptr;
    Surface() {
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS,
                                  reinterpret_cast<void**>(&pixels), nullptr, 0);
        if (!dc || !bitmap || !pixels) std::abort();
        oldBitmap = SelectObject(dc, bitmap);
        font = CreateFontW(-24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        oldFont = SelectObject(dc, font);
        clear();
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
    }
    ~Surface() {
        SelectObject(dc, oldFont); SelectObject(dc, oldBitmap);
        DeleteObject(font); DeleteObject(bitmap); DeleteDC(dc);
    }
    void clear() {
        GdiFlush(); std::memset(pixels, 0, width * height * sizeof(RGBQUAD));
    }
    int alphaCount() {
        GdiFlush(); int n = 0;
        for (int i = 0; i < width * height; ++i) n += pixels[i].rgbReserved != 0;
        return n;
    }
    int rgbCount() {
        GdiFlush(); int n = 0;
        for (int i = 0; i < width * height; ++i)
            n += (pixels[i].rgbBlue | pixels[i].rgbGreen | pixels[i].rgbRed) != 0;
        return n;
    }
    bool validPremultiplied() {
        GdiFlush();
        for (int i = 0; i < width * height; ++i) {
            const auto& p = pixels[i];
            if (p.rgbBlue > p.rgbReserved || p.rgbGreen > p.rgbReserved ||
                p.rgbRed > p.rgbReserved) return false;
        }
        return true;
    }
};

int failures = 0;
void check(bool ok, const char* name) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    failures += !ok;
}

int main() {
    auto snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    MODULEENTRY32W module{sizeof(module)};
    if (Module32FirstW(snap, &module)) do {
        if (std::wcsstr(module.szModule, L"translucent-windows")) {
            std::puts("ERROR external Translucent Windows hooks would contaminate this fixture");
            CloseHandle(snap); return 2;
        }
    } while (Module32NextW(snap, &module));
    CloseHandle(snap);

    UINT pathCount = 0, modeCount = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) == ERROR_SUCCESS) {
        std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
        std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
        if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount,
                               modes.data(), nullptr) == ERROR_SUCCESS) {
            for (UINT i = 0; i < pathCount; ++i) {
                DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO info{};
                info.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
                info.header.size = sizeof(info);
                info.header.adapterId = paths[i].targetInfo.adapterId;
                info.header.id = paths[i].targetInfo.id;
                if (DisplayConfigGetDeviceInfo(&info.header) == ERROR_SUCCESS)
                    std::printf("DISPLAY advancedColorSupported=%u advancedColorEnabled=%u bpc=%u\n",
                                info.advancedColorSupported, info.advancedColorEnabled,
                                info.bitsPerColorChannel);
            }
        }
    }
    BufferedPaintInit(); GenerateTextAlphaGammaLUT();
    constexpr WCHAR text[] = L"ACM Text";
    constexpr UINT count = ARRAYSIZE(text) - 1;
    {
        Surface surface;
        check(HookedExtTextOutW(surface.dc, 25, 25, 0, nullptr, text, count, nullptr),
              "zero-options-call-succeeds");
        check(surface.rgbCount() > 0 && surface.alphaCount() > 0 &&
              surface.validPremultiplied(), "zero-options-text-has-valid-alpha");
        int core = 0;
        for (int i = 0; i < Surface::width * Surface::height; ++i) {
            auto& p = surface.pixels[i];
            core += p.rgbReserved == 255 && p.rgbRed == 255 &&
                    p.rgbGreen == 255 && p.rgbBlue == 255;
        }
        check(core > 0, "fully-opaque-text-preserves-white-255");
        surface.clear(); SetTextColor(surface.dc, RGB(0, 0, 0));
        HookedExtTextOutW(surface.dc, 25, 25, 0, nullptr, text, count, nullptr);
        check(surface.alphaCount() > 0 && surface.rgbCount() == 0,
              "black-text-retains-coverage-alpha");
    }
    {
        Surface surface; RECT selected{20, 20, 200, 60};
        SetBkColor(surface.dc, GetSysColor(COLOR_HIGHLIGHT));
        check(HookedExtTextOutW(surface.dc, 0, 0, ETO_OPAQUE, &selected,
                               nullptr, 0, nullptr), "empty-selection-fill-succeeds");
        GdiFlush(); int opaque = 0, outside = 0;
        for (int y = 0; y < Surface::height; ++y)
            for (int x = 0; x < Surface::width; ++x) {
                const auto& p = surface.pixels[y * Surface::width + x];
                bool in = x >= selected.left && x < selected.right &&
                          y >= selected.top && y < selected.bottom;
                opaque += in && p.rgbReserved == 255;
                outside += !in && (p.rgbReserved | p.rgbBlue | p.rgbGreen | p.rgbRed);
            }
        check(opaque == 180 * 40 && outside == 0,
              "empty-selection-background-is-opaque-and-bounded");
        surface.clear();
        HookedExtTextOutW(surface.dc, 25, 25, ETO_OPAQUE | ETO_CLIPPED,
                          &selected, text, count, nullptr);
        GdiFlush();
        bool allOpaque = true;
        for (int y = 20; y < 60; ++y) for (int x = 20; x < 200; ++x)
            allOpaque &= surface.pixels[y * Surface::width + x].rgbReserved == 255;
        check(allOpaque && surface.validPremultiplied(),
              "selected-text-and-background-keep-opaque-alpha");
    }
    {
        Surface surface; RECT clipped{25, 25, 70, 55};
        HookedExtTextOutW(surface.dc, 25, 25, ETO_CLIPPED, &clipped,
                          text, count, nullptr);
        GdiFlush(); bool bounded = true;
        for (int y = 0; y < Surface::height; ++y) for (int x = 0; x < Surface::width; ++x)
            if (surface.pixels[y * Surface::width + x].rgbReserved)
                bounded &= x >= clipped.left && x < clipped.right &&
                           y >= clipped.top && y < clipped.bottom;
        check(surface.alphaCount() > 0 && bounded, "explicit-text-clipping-is-preserved");
        surface.clear(); IntersectClipRect(surface.dc, 25, 25, 70, 55);
        HookedExtTextOutW(surface.dc, 25, 25, 0, nullptr, text, count, nullptr);
        GdiFlush(); bounded = true;
        for (int y = 0; y < Surface::height; ++y) for (int x = 0; x < Surface::width; ++x)
            if (surface.pixels[y * Surface::width + x].rgbReserved)
                bounded &= x >= clipped.left && x < clipped.right &&
                           y >= clipped.top && y < clipped.bottom;
        check(surface.alphaCount() > 0 && bounded, "dc-clip-region-is-preserved");
    }
    for (UINT align : {UINT(TA_LEFT | TA_TOP), UINT(TA_CENTER | TA_BASELINE),
                       UINT(TA_RIGHT | TA_BOTTOM)}) {
        Surface raw, fixed; SetTextAlign(raw.dc, align); SetTextAlign(fixed.dc, align);
        ExtTextOutW_orig(raw.dc, 210, 80, 0, nullptr, text, count, nullptr);
        HookedExtTextOutW(fixed.dc, 210, 80, 0, nullptr, text, count, nullptr);
        GdiFlush(); int missing = 0, extra = 0;
        for (int i = 0; i < Surface::width * Surface::height; ++i) {
            auto& p = raw.pixels[i]; bool was = (p.rgbBlue | p.rgbGreen | p.rgbRed) != 0;
            bool now = fixed.pixels[i].rgbReserved != 0;
            missing += was && !now; extra += !was && now;
        }
        std::printf("ALIGN %u missing=%d extra=%d\n", align, missing, extra);
        check(missing == 0 && extra == 0, "text-alignment-matches-native-gdi");
    }
    {
        Surface surface; WORD glyphs[count]{};
        check(GetGlyphIndicesW(surface.dc, text, count, glyphs, 0) != GDI_ERROR,
              "glyph-indices-available");
        HookedExtTextOutW(surface.dc, 25, 25, ETO_GLYPH_INDEX, nullptr,
                          reinterpret_cast<LPCWSTR>(glyphs), count, nullptr);
        check(surface.alphaCount() > 0 && surface.validPremultiplied(),
              "glyph-index-text-has-valid-alpha");
        surface.clear();
        constexpr WCHAR korean[] = L"\ud55c\uae00 \uc120\ud0dd";
        HookedExtTextOutW(surface.dc, 25, 25, 0, nullptr, korean,
                          ARRAYSIZE(korean) - 1, nullptr);
        check(surface.alphaCount() > 0 && surface.validPremultiplied(),
              "korean-text-has-valid-alpha");
        surface.clear(); SetTextAlign(surface.dc, TA_UPDATECP); MoveToEx(surface.dc, 25, 25, nullptr);
        POINT before{}, after{}; GetCurrentPositionEx(surface.dc, &before);
        check(ExtTextOutShouldSkip(surface.dc, 0, nullptr, text, count),
              "current-position-calls-use-native-fallback");
        HookedExtTextOutW(surface.dc, 200, 100, 0, nullptr, text, count, nullptr);
        GetCurrentPositionEx(surface.dc, &after);
        check(after.x > before.x && after.y == before.y,
              "native-fallback-advances-current-position");
    }

    for (int mode : {1, 2, 3}) {
        Surface raw, fixed;
        HFONT rotated = nullptr; HGDIOBJ oldRaw = nullptr, oldFixed = nullptr;
        if (mode == 1) {
            for (HDC dc : {raw.dc, fixed.dc}) {
                SetMapMode(dc, MM_ANISOTROPIC);
                SetWindowExtEx(dc, 1, 1, nullptr); SetViewportExtEx(dc, 2, 2, nullptr);
            }
        } else if (mode == 2) {
            XFORM transform{1, 0, 0, 1, 25, 15};
            for (HDC dc : {raw.dc, fixed.dc}) {
                SetGraphicsMode(dc, GM_ADVANCED); SetWorldTransform(dc, &transform);
            }
        } else {
            LOGFONTW font{}; GetObjectW(raw.font, sizeof(font), &font);
            font.lfEscapement = 900; font.lfOrientation = 900;
            rotated = CreateFontIndirectW(&font);
            oldRaw = SelectObject(raw.dc, rotated); oldFixed = SelectObject(fixed.dc, rotated);
        }
        ExtTextOutW_orig(raw.dc, 60, 60, 0, nullptr, text, count, nullptr);
        HookedExtTextOutW(fixed.dc, 60, 60, 0, nullptr, text, count, nullptr);
        GdiFlush();
        check(std::memcmp(raw.pixels, fixed.pixels,
                          Surface::width * Surface::height * sizeof(RGBQUAD)) == 0,
              "transformed-text-preserves-native-output");
        if (rotated) {
            SelectObject(raw.dc, oldRaw); SelectObject(fixed.dc, oldFixed); DeleteObject(rotated);
        }
    }
    BufferedPaintUnInit();

    {
        Surface benchmark;
        const wchar_t* sample = L"Legacy selection test";
        const UINT sampleLength = static_cast<UINT>(std::wcslen(sample));
        for (int i = 0; i < 32; ++i)
            HookedExtTextOutW(benchmark.dc, 10, 10, 0, nullptr, sample, sampleLength, nullptr);
        GdiFlush();
        const DWORD objectsBefore = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
        LARGE_INTEGER frequency, start, end;
        QueryPerformanceFrequency(&frequency);
        constexpr int iterations = 1000;
        QueryPerformanceCounter(&start);
        for (int i = 0; i < iterations; ++i)
            ExtTextOutW_orig(benchmark.dc, 10, 10, 0, nullptr, sample, sampleLength, nullptr);
        GdiFlush();
        QueryPerformanceCounter(&end);
        const double nativeUs = (end.QuadPart-start.QuadPart)*1e6/frequency.QuadPart/iterations;
        QueryPerformanceCounter(&start);
        for (int i = 0; i < iterations; ++i)
            HookedExtTextOutW(benchmark.dc, 10, 10, 0, nullptr, sample, sampleLength, nullptr);
        GdiFlush();
        QueryPerformanceCounter(&end);
        const double repairedUs = (end.QuadPart-start.QuadPart)*1e6/frequency.QuadPart/iterations;
        const DWORD objectsAfter = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
        check(objectsBefore == objectsAfter, "repeated-text-repair-does-not-leak-gdi-objects");
        std::printf("TEXT_TIMING native_us=%.3f repaired_us=%.3f iterations=%d gdi_before=%lu gdi_after=%lu\n",
                    nativeUs, repairedUs, iterations, objectsBefore, objectsAfter);
    }
    std::printf("FAILURES=%d\n", failures);
    return failures ? 1 : 0;
}
