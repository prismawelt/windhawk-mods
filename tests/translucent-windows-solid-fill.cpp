
struct Bitmap {
 HDC dc=CreateCompatibleDC(nullptr);HBITMAP bmp;HGDIOBJ old;DWORD* px=nullptr;int w,h;
 Bitmap(int width=64,int height=64):w(width),h(height){BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,(void**)&px,nullptr,0);old=SelectObject(dc,bmp);}
 ~Bitmap(){SelectObject(dc,old);DeleteObject(bmp);DeleteDC(dc);}
 void Clear(DWORD value=0x55223344){GdiFlush();for(int i=0;i<w*h;++i)px[i]=value;}
};
int failures=0;
void Check(const char* name,bool ok){printf("%s %s\n",ok?"PASS":"FAIL",name);if(!ok)++failures;}
bool SameRgb(Bitmap& a,Bitmap& b){GdiFlush();for(int i=0;i<a.w*a.h;i++)if((a.px[i]&0xffffff)!=(b.px[i]&0xffffff))return false;return true;}
bool CoverageMatchesNative(Bitmap& actual,Bitmap& native,DWORD rgb){
 GdiFlush();for(int i=0;i<actual.w*actual.h;i++){
  DWORD expected=(native.px[i]&0xffffff)==rgb ? (0xff000000|rgb) : native.px[i];
  if(actual.px[i]!=expected)return false;
 }return true;
}
using FillRoutine=decltype(&FillRect);
double Benchmark(FillRoutine routine,Bitmap& dest,int count){
 RECT edges[]={{3,3,5,59},{3,1,59,3},{57,3,59,59},{3,59,59,61}};
 LARGE_INTEGER a,b,f;QueryPerformanceFrequency(&f);QueryPerformanceCounter(&a);
 for(int i=0;i<count;i++)for(const RECT& edge:edges)routine(dest.dc,&edge,(HBRUSH)GetStockObject(DC_BRUSH));
 GdiFlush();QueryPerformanceCounter(&b);return 1e6*(b.QuadPart-a.QuadPart)/f.QuadPart/count;
}
int main(){
 HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,GetCurrentProcessId());MODULEENTRY32 m={};m.dwSize=sizeof(m);
 if(snap!=INVALID_HANDLE_VALUE&&Module32First(snap,&m))do{if(wcsstr(m.szModule,L"translucent-windows")){fprintf(stderr,"Test contaminated by active mod injection\n");CloseHandle(snap);return 2;}}while(Module32Next(snap,&m));if(snap!=INVALID_HANDLE_VALUE)CloseHandle(snap);
 Bitmap d,n;HBRUSH dcBrush=(HBRUSH)GetStockObject(DC_BRUSH);RECT rect={4,5,25,30};
 SetDCBrushColor(d.dc,RGB(179,85,118));SetDCBrushColor(n.dc,RGB(179,85,118));
 LOGBRUSH info={};GetObjectW(dcBrush,sizeof(info),&info);
 Check("dc-brush-object-color-differs-from-actual-dc-color",info.lbColor!=GetDCBrushColor(d.dc));
 d.Clear();n.Clear();FillRect(n.dc,&rect,dcBrush);HookedFillRect(d.dc,&rect,dcBrush);GdiFlush();
 printf("native=%08lx fixed=%08lx\n",n.px[5*64+4],d.px[5*64+4]);
 Check("native-dynamic-fill-reproduces-missing-alpha",n.px[5*64+4]==0x00B35576);
 Check("dynamic-fill-keeps-exact-rgb-and-repairs-alpha",SameRgb(d,n)&&CoverageMatchesNative(d,n,0xB35576));
 Check("repair-uses-only-one-source-pixel",lastWidth==1&&lastHeight==1);
 Check("pixels-outside-fill-stay-unchanged",d.px[0]==0x55223344&&d.px[31*64+26]==0x55223344);
 Check("destination-dc-brush-state-is-preserved",GetDCBrushColor(d.dc)==RGB(179,85,118)&&GetCurrentObject(d.dc,OBJ_BRUSH)==GetCurrentObject(n.dc,OBJ_BRUSH));
 for(COLORREF color:{RGB(0,0,0),RGB(255,255,255)}){
  SetDCBrushColor(d.dc,color);d.Clear();HookedFillRect(d.dc,&rect,dcBrush);GdiFlush();
  DWORD expected=0xff000000|((DWORD)GetRValue(color)<<16)|((DWORD)GetGValue(color)<<8)|GetBValue(color);
  Check(color==0?"black-foreground-retains-opacity":"full-white-is-exact",d.px[5*64+4]==expected);
 }
 SetDCBrushColor(d.dc,RGB(179,85,118));d.Clear();n.Clear();
 HRGN clip=CreateRectRgn(10,10,20,20);SelectClipRgn(d.dc,clip);SelectClipRgn(n.dc,clip);DeleteObject(clip);
 FillRect(n.dc,&rect,dcBrush);HookedFillRect(d.dc,&rect,dcBrush);
 Check("explicit-clipping-keeps-native-rgb-and-coverage",SameRgb(d,n)&&CoverageMatchesNative(d,n,0xB35576));
 SelectClipRgn(d.dc,nullptr);SelectClipRgn(n.dc,nullptr);
 SetViewportOrgEx(d.dc,7,9,nullptr);SetViewportOrgEx(n.dc,7,9,nullptr);
 d.Clear();n.Clear();FillRect(n.dc,&rect,dcBrush);HookedFillRect(d.dc,&rect,dcBrush);
 Check("viewport-origin-keeps-native-rgb-and-coverage",SameRgb(d,n)&&CoverageMatchesNative(d,n,0xB35576));
 SetViewportOrgEx(d.dc,0,0,nullptr);SetViewportOrgEx(n.dc,0,0,nullptr);
 SetGraphicsMode(d.dc,GM_ADVANCED);SetGraphicsMode(n.dc,GM_ADVANCED);
 XFORM xf={0.5f,0,0,0.5f,3,5};SetWorldTransform(d.dc,&xf);SetWorldTransform(n.dc,&xf);
 d.Clear();n.Clear();FillRect(n.dc,&rect,dcBrush);HookedFillRect(d.dc,&rect,dcBrush);
 Check("advanced-destination-transform-keeps-native-rgb-and-coverage",SameRgb(d,n)&&CoverageMatchesNative(d,n,0xB35576));
 xf={1,0,0,1,0,0};SetWorldTransform(d.dc,&xf);SetWorldTransform(n.dc,&xf);SetGraphicsMode(d.dc,GM_COMPATIBLE);SetGraphicsMode(n.dc,GM_COMPATIBLE);
 SetMapMode(d.dc,MM_ANISOTROPIC);SetMapMode(n.dc,MM_ANISOTROPIC);
 SetWindowExtEx(d.dc,2,2,nullptr);SetWindowExtEx(n.dc,2,2,nullptr);SetViewportExtEx(d.dc,3,3,nullptr);SetViewportExtEx(n.dc,3,3,nullptr);
 d.Clear();n.Clear();FillRect(n.dc,&rect,dcBrush);HookedFillRect(d.dc,&rect,dcBrush);
 Check("scaled-mapping-keeps-native-rgb-and-coverage",SameRgb(d,n)&&CoverageMatchesNative(d,n,0xB35576));
 SetMapMode(d.dc,MM_TEXT);SetMapMode(n.dc,MM_TEXT);

 SetLayout(d.dc,LAYOUT_RTL);SetLayout(n.dc,LAYOUT_RTL);
 d.Clear();n.Clear();FillRect(n.dc,&rect,dcBrush);HookedFillRect(d.dc,&rect,dcBrush);
 Check("right-to-left-layout-keeps-native-rgb-and-coverage",SameRgb(d,n)&&CoverageMatchesNative(d,n,0xB35576));
 SetLayout(d.dc,0);SetLayout(n.dc,0);
 SetDCBrushColor(d.dc,PALETTEINDEX(2));SetDCBrushColor(n.dc,PALETTEINDEX(2));
 d.Clear();n.Clear();FillRect(n.dc,&rect,dcBrush);HookedFillRect(d.dc,&rect,dcBrush);GdiFlush();
 Check("palette-index-colors-use-byte-exact-native-path",!memcmp(d.px,n.px,64*64*4));
 SetDCBrushColor(d.dc,RGB(179,85,118));SetDCBrushColor(n.dc,RGB(179,85,118));
 HBRUSH pattern=CreateHatchBrush(HS_DIAGCROSS,RGB(50,100,150));
 d.Clear();n.Clear();FillRect(n.dc,&rect,pattern);HookedFillRect(d.dc,&rect,pattern);GdiFlush();
 Check("pattern-brush-uses-byte-exact-native-path",!memcmp(d.px,n.px,64*64*4));DeleteObject(pattern);
 d.Clear();n.Clear();FillRect(n.dc,&rect,(HBRUSH)GetStockObject(BLACK_BRUSH));HookedFillRect(d.dc,&rect,(HBRUSH)GetStockObject(BLACK_BRUSH));GdiFlush();
 Check("glass-background-erase-keeps-native-alpha",!memcmp(d.px,n.px,64*64*4)&&d.px[5*64+4]==0);
 d.Clear();n.Clear();FillRect(n.dc,&rect,(HBRUSH)(COLOR_WINDOW+1));HookedFillRect(d.dc,&rect,(HBRUSH)(COLOR_WINDOW+1));GdiFlush();
 Check("pseudo-system-brush-is-still-resolved",!memcmp(d.px,n.px,64*64*4));
 for(int gate=0;gate<3;gate++){
  g_settings.FillBg=gate!=0;g_settings.Unload=gate==1;g_settings.BgType=gate==2?g_settings.Default:g_settings.Blur;
  d.Clear();n.Clear();FillRect(n.dc,&rect,dcBrush);HookedFillRect(d.dc,&rect,dcBrush);GdiFlush();
  Check(gate==0?"disabled-custom-rendering-uses-native-path":gate==1?"unload-uses-native-path":"ordinary-background-uses-native-path",!memcmp(d.px,n.px,64*64*4));
 }
 g_settings.FillBg=true;g_settings.Unload=false;g_settings.BgType=g_settings.Blur;
 RECT empty={5,5,5,6};d.Clear();n.Clear();int actual=HookedFillRect(d.dc,&empty,dcBrush),expected=FillRect(n.dc,&empty,dcBrush);GdiFlush();
 Check("empty-rectangle-keeps-native-result",actual==expected&&!memcmp(d.px,n.px,64*64*4));
 d.Clear();n.Clear();forceBlendFailure=true;SetLastError(123456);actual=HookedFillRect(d.dc,&rect,dcBrush);DWORD ae=GetLastError();forceBlendFailure=false;
 SetLastError(123456);expected=FillRect(n.dc,&rect,dcBrush);DWORD ee=GetLastError();GdiFlush();
 Check("blend-failure-falls-back-with-native-result-and-error",actual==expected&&ae==ee&&!memcmp(d.px,n.px,64*64*4));
 DWORD before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
 double nativeUs=Benchmark(FillRect,d,1000),fixedUs=Benchmark(HookedFillRect,d,1000);
 DWORD after=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
 Check("repeated-four-edge-fills-leak-no-gdi-objects",before==after);
 printf("four-edge-benchmark native-us=%.3f fixed-us=%.3f extra-us=%.3f temporary-source-bytes=4 GDI-before=%lu GDI-after=%lu\n",nativeUs,fixedUs,fixedUs-nativeUs,before,after);
 printf("Failures: %d\n",failures);return failures?1:0;
}
