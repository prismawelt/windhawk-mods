
struct Bitmap {
 HDC dc=CreateCompatibleDC(nullptr);HBITMAP bmp;HGDIOBJ old;DWORD* px=nullptr;int w,h;
 Bitmap(int width=64,int height=64):w(width),h(height){BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,(void**)&px,nullptr,0);old=SelectObject(dc,bmp);}
 ~Bitmap(){SelectObject(dc,old);DeleteObject(bmp);DeleteDC(dc);}
 void Fill(DWORD value){GdiFlush();for(int i=0;i<w*h;++i)px[i]=value;}
};
int failures=0;BLENDFUNCTION lastBlend={};HDC lastSource=nullptr;
BOOL WINAPI Recording(HDC d,int x,int y,int w,int h,HDC s,int sx,int sy,int sw,int sh,BLENDFUNCTION b){lastBlend=b;lastSource=s;return AlphaBlend(d,x,y,w,h,s,sx,sy,sw,sh,b);}
void Check(const char* n,bool pass){printf("%s %s\n",pass?"PASS":"FAIL",n);if(!pass)++failures;}
using Routine=decltype(&AlphaBlend);
double Benchmark(Routine routine,Bitmap& d,Bitmap& s,int count,BYTE alpha=128){LARGE_INTEGER a,b,f;QueryPerformanceFrequency(&f);BLENDFUNCTION blend={AC_SRC_OVER,0,alpha,0};QueryPerformanceCounter(&a);for(int i=0;i<count;++i)routine(d.dc,0,0,d.w,d.h,s.dc,0,0,s.w,s.h,blend);GdiFlush();QueryPerformanceCounter(&b);return 1e6*(b.QuadPart-a.QuadPart)/f.QuadPart/count;}
int main(){
 HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,GetCurrentProcessId());MODULEENTRY32 m={};m.dwSize=sizeof(m);
 if(snap!=INVALID_HANDLE_VALUE&&Module32First(snap,&m))do{if(wcsstr(m.szModule,L"translucent-windows")){fprintf(stderr,"Test contaminated by active mod injection\n");CloseHandle(snap);return 2;}}while(Module32Next(snap,&m));if(snap!=INVALID_HANDLE_VALUE)CloseHandle(snap);
 AlphaBlend_orig=Recording;Bitmap s,d,n;BLENDFUNCTION b={AC_SRC_OVER,0,128,0};
 s.Fill(0x00123456);d.Fill(0);n.Fill(0);AlphaBlend(n.dc,0,0,64,64,s.dc,0,0,64,64,b);HookedLegacyAlphaBlend(d.dc,0,0,64,64,s.dc,0,0,64,64,b);GdiFlush();
 printf("native=%08lx fixed=%08lx\n",n.px[0],d.px[0]);
 Check("native-constant-alpha-reproduces-missing-alpha",(n.px[0]>>24)==0&&(n.px[0]&0xFFFFFF)!=0);
 Check("constant-alpha-retains-exact-coverage",d.px[0]==0x80091A2B);
 Check("conversion-uses-existing-mod-blend-contract",lastBlend.SourceConstantAlpha==255&&lastBlend.AlphaFormat==AC_SRC_ALPHA);
 Check("source-is-not-mutated",s.px[0]==0x00123456);
 s.Fill(0x01FFFFFF);d.Fill(0);HookedLegacyAlphaBlend(d.dc,0,0,64,64,s.dc,0,0,64,64,b);GdiFlush();Check("ignored-source-alpha-is-handled",d.px[0]==0x80808080);
 s.Fill(0);d.Fill(0);HookedLegacyAlphaBlend(d.dc,0,0,64,64,s.dc,0,0,64,64,b);GdiFlush();Check("black-source-retains-coverage",d.px[0]==0x80000000);
 s.Fill(0x00123456);d.Fill(0);b.SourceConstantAlpha=255;HookedLegacyAlphaBlend(d.dc,0,0,64,64,s.dc,0,0,64,64,b);GdiFlush();Check("full-opacity-keeps-exact-rgb",d.px[0]==0xFF123456);
 s.Fill(0x80123456);d.Fill(0x80402010);n.Fill(0x80402010);BLENDFUNCTION p={AC_SRC_OVER,0,160,AC_SRC_ALPHA};AlphaBlend(n.dc,0,0,64,64,s.dc,0,0,64,64,p);HookedLegacyAlphaBlend(d.dc,0,0,64,64,s.dc,0,0,64,64,p);GdiFlush();Check("per-pixel-blend-is-byte-exact",!memcmp(d.px,n.px,64*64*4));Check("per-pixel-blend-uses-original-source",lastSource==s.dc&&lastBlend.SourceConstantAlpha==160);
 d.Fill(0x80402010);b.SourceConstantAlpha=0;HookedLegacyAlphaBlend(d.dc,0,0,64,64,s.dc,0,0,64,64,b);GdiFlush();Check("zero-alpha-uses-native-noop",d.px[0]==0x80402010&&lastSource==s.dc);
 Bitmap tint(1,1);tint.Fill(0x000080FF);d.Fill(0);b.SourceConstantAlpha=128;HookedLegacyAlphaBlend(d.dc,0,0,64,64,tint.dc,0,0,1,1,b);GdiFlush();Check("one-pixel-tint-scales-with-valid-alpha",d.px[0]==0x80004080&&d.px[4095]==0x80004080);
 d.Fill(0);HRGN clip=CreateRectRgn(4,4,20,20);SelectClipRgn(d.dc,clip);DeleteObject(clip);HookedLegacyAlphaBlend(d.dc,0,0,64,64,tint.dc,0,0,1,1,b);GdiFlush();Check("destination-clipping-is-kept",d.px[0]==0&&d.px[650]==0x80004080&&d.px[1950]==0);SelectClipRgn(d.dc,nullptr);
 d.Fill(0);s.Fill(0x00123456);SetViewportOrgEx(s.dc,8,12,nullptr);HookedLegacyAlphaBlend(d.dc,0,0,16,16,s.dc,-8,-12,16,16,b);GdiFlush();Check("source-origin-is-kept",d.px[0]==0x80091A2B);SetViewportOrgEx(s.dc,0,0,nullptr);
 SetGraphicsMode(s.dc,GM_ADVANCED);d.Fill(0);HookedLegacyAlphaBlend(d.dc,0,0,16,16,s.dc,0,0,16,16,b);GdiFlush();Check("advanced-transforms-use-native-fallback",lastSource==s.dc&&lastBlend.AlphaFormat==0);SetGraphicsMode(s.dc,GM_COMPATIBLE);
 SetLastError(123456);BOOL actual=HookedLegacyAlphaBlend(d.dc,0,0,16,16,s.dc,1000,0,16,16,b);DWORD ae=GetLastError();SetLastError(123456);BOOL expected=AlphaBlend(d.dc,0,0,16,16,s.dc,1000,0,16,16,b);DWORD ee=GetLastError();Check("invalid-source-keeps-native-result-and-error",actual==expected&&ae==ee&&lastSource==s.dc);
 BLENDFUNCTION invalid=b;invalid.BlendFlags=1;actual=HookedLegacyAlphaBlend(d.dc,0,0,16,16,s.dc,0,0,16,16,invalid);expected=AlphaBlend(d.dc,0,0,16,16,s.dc,0,0,16,16,invalid);Check("invalid-flags-use-native-validation",actual==expected&&lastSource==s.dc&&lastBlend.BlendFlags==1);

 {
  Bitmap transformedDest,transformedNative,transformSource;
  transformSource.Fill(0x00123456);transformedDest.Fill(0);transformedNative.Fill(0);
  SetGraphicsMode(transformedDest.dc,GM_ADVANCED);SetGraphicsMode(transformedNative.dc,GM_ADVANCED);
  XFORM transform={0.5f,0,0,0.5f,2,3};
  SetWorldTransform(transformedDest.dc,&transform);SetWorldTransform(transformedNative.dc,&transform);
  BLENDFUNCTION coverage={AC_SRC_OVER,0,128,0};
  AlphaBlend(transformedNative.dc,0,0,64,64,transformSource.dc,0,0,64,64,coverage);
  HookedLegacyAlphaBlend(transformedDest.dc,0,0,64,64,transformSource.dc,0,0,64,64,coverage);
  GdiFlush();bool sameRgb=true,hasCoverage=false;
  for(int i=0;i<64*64;++i){sameRgb&=(transformedDest.px[i]&0xffffff)==(transformedNative.px[i]&0xffffff);hasCoverage|=(transformedDest.px[i]>>24)!=0;}
  Check("destination-world-transform-keeps-native-rgb-and-repairs-alpha",sameRgb&&hasCoverage&&lastBlend.AlphaFormat==AC_SRC_ALPHA);
 }
 g_settings.BgType=g_settings.Default;HookedLegacyAlphaBlend(d.dc,0,0,16,16,s.dc,0,0,16,16,b);Check("ordinary-rendering-uses-native-path",lastSource==s.dc&&lastBlend.AlphaFormat==0);g_settings.BgType=g_settings.Blur;
 DWORD before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);double nativeUs=Benchmark(AlphaBlend,d,tint,2000);double fixedUs=Benchmark(HookedLegacyAlphaBlend,d,tint,2000);DWORD after=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);Check("repeated-blends-leak-no-gdi-resources",before==after);printf("benchmark-1x1-to-64x64 native-us=%.3f fixed-us=%.3f extra-us=%.3f GDI-before=%lu GDI-after=%lu\n",nativeUs,fixedUs,fixedUs-nativeUs,before,after);

 {
  Bitmap marqueeSource(100,100),marqueeDest(327,777),marqueeNative(327,777);
  marqueeSource.Fill(0x00AABBCC);marqueeDest.Fill(0);marqueeNative.Fill(0);
  BLENDFUNCTION observed={AC_SRC_OVER,0,89,0};
  AlphaBlend(marqueeNative.dc,0,0,327,777,marqueeSource.dc,0,0,100,100,observed);
  HookedLegacyAlphaBlend(marqueeDest.dc,0,0,327,777,marqueeSource.dc,0,0,100,100,observed);
  GdiFlush();
  Check("observed-marquee-shape-retains-alpha-in-memory-destination",
        (marqueeNative.px[0]>>24)==0&&marqueeDest.px[0]==0x593B4147&&
        marqueeDest.px[327*777-1]==0x593B4147);
  double nativeMarquee=Benchmark(AlphaBlend,marqueeDest,marqueeSource,200,89);
  double repairedMarquee=Benchmark(HookedLegacyAlphaBlend,marqueeDest,marqueeSource,200,89);
  printf("benchmark-observed-marquee-100x100-to-327x777 native-us=%.3f fixed-us=%.3f extra-us=%.3f temporary-source-bytes=40000\n",
         nativeMarquee,repairedMarquee,repairedMarquee-nativeMarquee);
 }
 printf("Failures: %d\n",failures);return failures?1:0;
}
