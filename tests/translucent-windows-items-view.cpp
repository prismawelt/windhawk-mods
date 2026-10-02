
struct CacheSlots {
    std::array<HDC, 6> values{};
    HDC invalid{};
    bool badAccess = false;
    HDC& operator[](int index) {
        if (index < 0 || index >= 6) { badAccess = true; return invalid; }
        return values[index];
    }
};
struct ThemeCache {
    CacheSlots itemsview;
    int creations = 0;
    BOOL CacheItemsView(INT, INT, INT index) {
        ++creations;
        itemsview[index] = reinterpret_cast<HDC>(static_cast<intptr_t>(index + 1));
        return TRUE;
    }
} g_themeCache;
void* g_d2dFactory = reinterpret_cast<void*>(1);
int draws = 0, delegatedPart = -1, delegatedState = -1;
bool delegateResult = true;
BOOL PaintListView(HDC, INT part, INT state, LPCRECT) {
    delegatedPart = part; delegatedState = state; return delegateResult;
}
void DrawNineGridStretch(HDC, HDC, LPCRECT, INT = 0, INT = 0, INT = 0, INT = 0) { ++draws; }

// ==TestBody==

int failures = 0;
void check(bool ok, const char* name) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    failures += !ok;
}
void reset() {
    g_themeCache = {}; draws = 0; delegatedPart = delegatedState = -1;
    delegateResult = true; g_d2dFactory = reinterpret_cast<void*>(1);
}
int main() {
    RECT rect{0, 0, 40, 40};
    reset();
    check(!PaintItemsView(nullptr, 2, 1, &rect) &&
          g_themeCache.creations == 0 && draws == 0,
          "unimplemented-part-reaches-native-rendering-without-poisoning-cache");
    bool rejected = true, bounded = true;
    for (int part = -2; part <= 12; ++part) {
        for (int state = -3; state <= 24; ++state) {
            const bool supported =
                (part == 1 && state >= 1 && state <= 4) ||
                ((part == 3 || part == 6) && state >= 1 && state <= 2) ||
                (part == 4 && (state == 11 || state == 12));
            if (supported) continue;
            reset();
            rejected &= !PaintItemsView(nullptr, part, state, &rect) &&
                g_themeCache.creations == 0 && draws == 0 && delegatedPart == -1;
            bounded &= !g_themeCache.itemsview.badAccess;
        }
    }
    check(rejected, "unknown-and-negative-parts-and-states-preserve-native-fallback");
    check(bounded, "unsupported-states-never-index-a-theme-cache");
    bool supportedOk = true;
    for (int part : {1, 3, 6}) {
        int lastState = part == 1 ? 4 : 2;
        for (int state = 1; state <= lastState; ++state) {
            reset();
            supportedOk &= PaintItemsView(nullptr, part, state, &rect) &&
                g_themeCache.creations == 1 && draws == 1 &&
                !g_themeCache.itemsview.badAccess;
            supportedOk &= PaintItemsView(nullptr, part, state, &rect) &&
                g_themeCache.creations == 1 && draws == 2;
        }
    }
    check(supportedOk, "supported-styles-retain-cache-reuse");
    reset();
    check(PaintItemsView(nullptr, 4, 11, &rect) && delegatedPart == 1 &&
          delegatedState == 6 && g_themeCache.creations == 0,
          "dark-conflict-focused-button-keeps-existing-rendering");
    reset();
    check(PaintItemsView(nullptr, 4, 12, &rect) && delegatedPart == 1 &&
          delegatedState == 2 && g_themeCache.creations == 0,
          "dark-conflict-selected-button-keeps-existing-rendering");
    reset(); delegateResult = false;
    check(!PaintItemsView(nullptr, 4, 11, &rect),
          "failed-delegated-rendering-reaches-native-fallback");
    reset(); g_d2dFactory = nullptr;
    check(!PaintItemsView(nullptr, 1, 1, &rect) && g_themeCache.creations == 0,
          "missing-renderer-keeps-native-fallback");
    return failures ? 1 : 0;
}
