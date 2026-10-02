#define UNICODE
#define _UNICODE
#define WH_MOD_ID L"translucent-windows-glass-fixture"
#include <windows.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <climits>
#include <initializer_list>
struct {bool FillBg=true;bool Unload=false;enum Type{Default,Blur}BgType=Blur;}g_settings;
bool g_IsSysThemeDarkMode=true;
BOOL IsWindowEligible(HWND h){return h!=nullptr;}
static decltype(&FillRect) FillRect_orig=FillRect;
namespace WindhawkUtils {
using WH_SUBCLASSPROC=LRESULT(CALLBACK*)(HWND,UINT,WPARAM,LPARAM,DWORD_PTR);
LRESULT CALLBACK Thunk(HWND w,UINT m,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR fn){
 return reinterpret_cast<WH_SUBCLASSPROC>(fn)(w,m,wp,lp,0);
}
BOOL SetWindowSubclassFromAnyThread(HWND w,WH_SUBCLASSPROC fn,DWORD_PTR){return SetWindowSubclass(w,Thunk,1,reinterpret_cast<DWORD_PTR>(fn));}
void RemoveWindowSubclassFromAnyThread(HWND w,WH_SUBCLASSPROC){RemoveWindowSubclass(w,Thunk,1);}
}
// ==TestBody==
struct Bitmap {
 HDC dc=CreateCompatibleDC(nullptr);HBITMAP bmp;HGDIOBJ old;DWORD* px=nullptr;
 Bitmap(){BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=32;bi.bmiHeader.biHeight=-32;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,(void**)&px,nullptr,0);old=SelectObject(dc,bmp);}
 ~Bitmap(){SelectObject(dc,old);DeleteObject(bmp);DeleteDC(dc);}
 void Clear(DWORD v=0x55223344){GdiFlush();for(int i=0;i<1024;i++)px[i]=v;}
 bool All(DWORD v){GdiFlush();for(int i=0;i<1024;i++)if(px[i]!=v)return false;return true;}
};
int failures=0;
void Check(const char* name,bool ok){printf("%s %s\n",ok?"PASS":"FAIL",name);failures+=!ok;}
HBRUSH paintBrush;RECT paintRect={0,0,32,32};bool useFillRect=false;
HDC paintDC;COLORREF computedText;
LRESULT CALLBACK TestWindow(HWND w,UINT m,WPARAM wp,LPARAM lp){
 if(m==WM_ERASEBKGND||m==WM_PAINT){
  if(!paintBrush || (m==WM_PAINT && !paintDC))return DefWindowProcW(w,m,wp,lp);
  HDC dc=m==WM_ERASEBKGND?reinterpret_cast<HDC>(wp):paintDC;
  HGDIOBJ old=SelectObject(dc,paintBrush);
  if(useFillRect)HookedFillRect(dc,&paintRect,paintBrush);
  else HookedLegacyPatBlt(dc,paintRect.left,paintRect.top,paintRect.right-paintRect.left,paintRect.bottom-paintRect.top,PATCOPY);
  computedText=LegacyGlassTextColor(dc,RGB(0,0,0),&paintRect);
  SelectObject(dc,old);return 1;
 }
 return DefWindowProcW(w,m,wp,lp);
}
int main(){
 setvbuf(stdout,nullptr,_IONBF,0);
 HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,GetCurrentProcessId());MODULEENTRY32 m={};m.dwSize=sizeof(m);
 if(snap!=INVALID_HANDLE_VALUE&&Module32First(snap,&m))do{if(wcsstr(m.szModule,L"translucent-windows")){fprintf(stderr,"Fixture contaminated by mod injection\n");return 2;}}while(Module32Next(snap,&m));if(snap!=INVALID_HANDLE_VALUE)CloseHandle(snap);
 WNDCLASSW wc={};wc.lpfnWndProc=TestWindow;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"GlassPaintFixture";RegisterClassW(&wc);
 HWND w=CreateWindowW(wc.lpszClassName,L"",WS_OVERLAPPEDWINDOW,0,0,100,100,nullptr,nullptr,wc.hInstance,nullptr);
AttachLegacyGlassPainting(w);Bitmap d,n;paintDC=d.dc;
 for(COLORREF c:{PALETTERGB(255,255,255),PALETTERGB(238,238,238),RGB(255,255,255)}){
  paintBrush=CreateSolidBrush(c);d.Clear();SendMessageW(w,WM_ERASEBKGND,(WPARAM)d.dc,0);
  Check("native-background-message-clears-rgb-and-alpha-to-glass",d.All(0));
  Check("erase-establishes-local-background-intent",GetPropW(w,kLegacyGlassBackground)!=nullptr);
  Check("dark-text-on-cleared-backdrop-becomes-white",computedText==RGB(255,255,255));
  Check("nested-paint-context-is-unwound",LegacyGlassPaintScope::current==nullptr);
  DeleteObject(paintBrush);
 }
 paintBrush=(HBRUSH)GetStockObject(WHITE_BRUSH);useFillRect=true;
 d.Clear();SendMessageW(w,WM_ERASEBKGND,(WPARAM)d.dc,0);
 Check("fillrect-erase-keeps-glass-transparent",d.All(0));
 d.Clear();SendMessageW(w,WM_PAINT,0,0);
 Check("whole-clip-background-repaint-stays-transparent",d.All(0));
 paintRect={4,4,8,8};d.Clear(0);SendMessageW(w,WM_PAINT,0,0);GdiFlush();
 Check("small-white-foreground-retains-opacity",d.px[4*32+4]==0xffffffff&&d.px[0]==0);
 Check("dark-text-over-bright-opaque-content-retains-native-color",computedText==RGB(0,0,0));
 paintBrush=CreateSolidBrush(PALETTERGB(179,85,118));d.Clear(0);SendMessageW(w,WM_PAINT,0,0);GdiFlush();
 Check("colored-foreground-keeps-exact-rgb-and-opaque-alpha",d.px[4*32+4]==0xffb35576);DeleteObject(paintBrush);
 paintBrush=(HBRUSH)GetStockObject(DC_BRUSH);SetDCBrushColor(d.dc,0);d.Clear(0);SendMessageW(w,WM_PAINT,0,0);GdiFlush();
 Check("black-dc-brush-foreground-is-opaque",d.px[4*32+4]==0xff000000);
 for(int i=0;i<3;i++){
  g_settings.FillBg=i!=0;g_settings.Unload=i==1;g_settings.BgType=i==2?g_settings.Default:g_settings.Blur;
  paintRect={0,0,32,32};paintBrush=(HBRUSH)GetStockObject(WHITE_BRUSH);useFillRect=false;
  d.Clear();n.Clear();HGDIOBJ old=SelectObject(n.dc,paintBrush);PatBlt(n.dc,0,0,32,32,PATCOPY);SelectObject(n.dc,old);
  SendMessageW(w,WM_ERASEBKGND,(WPARAM)d.dc,0);GdiFlush();
  Check("disabled-unload-and-native-background-modes-keep-native-bytes",!memcmp(d.px,n.px,4096));
 }
 g_settings.FillBg=true;g_settings.Unload=false;g_settings.BgType=g_settings.Blur;
 paintBrush=(HBRUSH)GetStockObject(WHITE_BRUSH);d.Clear();HookedLegacyPatBlt(d.dc,0,0,32,32,PATCOPY);GdiFlush();
 Check("unowned-drawing-does-not-change-background-contract",d.px[0]==0x00ffffff);
 {
  LegacyGlassPaintScope scope(w,nullptr);
  d.Clear(0);RECT rc={0,0,8,8};SetTextColor(d.dc,RGB(0,0,0));
  Check("gray-text-contrast-adapts-on-dark-glass",LegacyGlassTextColor(d.dc,RGB(109,109,109),&rc)==RGB(146,146,146));
  Check("explicit-colored-text-is-preserved",LegacyGlassTextColor(d.dc,RGB(179,85,118),&rc)==RGB(179,85,118));
  Check("white-foreground-remains-white",LegacyGlassTextColor(d.dc,RGB(255,255,255),&rc)==RGB(255,255,255));
  Check("application-dc-text-color-is-preserved",GetTextColor(d.dc)==RGB(0,0,0));
  g_IsSysThemeDarkMode=false;
  Check("light-theme-retains-native-dark-text",LegacyGlassTextColor(d.dc,RGB(0,0,0),&rc)==RGB(0,0,0));
  g_IsSysThemeDarkMode=true;
 }
 DWORD before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
 for(int i=0;i<100;i++){paintRect={0,0,32,32};useFillRect=false;SendMessageW(w,WM_ERASEBKGND,(WPARAM)d.dc,0);paintRect={4,4,8,8};SendMessageW(w,WM_PAINT,0,0);}
 GdiFlush();DWORD after=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
 Check("repeated-background-and-foreground-paints-leak-no-gdi-objects",before==after);
 g_settings.Unload=true;RestoreLegacyGlassPainting();
 Check("unload-removes-local-background-properties",GetPropW(w,kLegacyGlassBackground)==nullptr);
 paintRect={0,0,32,32};useFillRect=false;d.Clear();SendMessageW(w,WM_ERASEBKGND,(WPARAM)d.dc,0);GdiFlush();
 Check("unload-removes-window-subclass",d.px[0]==0x00ffffff&&LegacyGlassPaintScope::current==nullptr);
 DestroyWindow(w);UnregisterClassW(wc.lpszClassName,wc.hInstance);
 printf("Failures: %d GDI-before=%lu GDI-after=%lu\n",failures,before,after);return failures?1:0;
}
