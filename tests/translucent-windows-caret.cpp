struct Dib {
 HDC dc=CreateCompatibleDC(nullptr);HBITMAP bitmap=nullptr;HGDIOBJ old=nullptr;DWORD* pixels=nullptr;int w,h;
 Dib(int width,int height):w(width),h(height){BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,(void**)&pixels,nullptr,0);old=SelectObject(dc,bitmap);}
 ~Dib(){SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);}
};
thread_local Dib* background=nullptr;
int failures=0;
void Check(const char* name,bool ok){printf("%s %s\n",ok?"PASS":"FAIL",name);if(!ok)++failures;}
LRESULT CALLBACK WindowProc(HWND window,UINT message,WPARAM w,LPARAM l){
 if(message==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(window,&ps);if(background)BitBlt(dc,0,0,background->w,background->h,background->dc,0,0,SRCCOPY);EndPaint(window,&ps);return 0;}
 return DefWindowProc(window,message,w,l);
}
HWND CreateGlassWindow(){
 HWND h=CreateWindowW(L"WindhawkCaretRegression",L"Temporary native caret check",WS_OVERLAPPEDWINDOW,30,30,400,220,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 MARGINS margins={-1,-1,-1,-1};DwmExtendFrameIntoClientArea(h,&margins);ShowWindow(h,SW_SHOWNOACTIVATE);UpdateWindow(h);return h;
}
void FillBackground(HWND h,Dib& bg,DWORD value){
 GdiFlush();std::fill_n(bg.pixels,bg.w*bg.h,value);InvalidateRect(h,nullptr,FALSE);UpdateWindow(h);
}
DWORD ReadPixel(HWND window,int x=20,int y=20){
 Dib sample(1,1);HDC source=GetDC(window);BitBlt(sample.dc,0,0,1,1,source,x,y,SRCCOPY);ReleaseDC(window,source);GdiFlush();return sample.pixels[0];
}
bool IsVisible(){GUITHREADINFO info={sizeof(info)};return GetGUIThreadInfo(GetCurrentThreadId(),&info)&&(info.flags&GUI_CARETBLINKING);}
void Pump(DWORD milliseconds){
 DWORD start=GetTickCount();do{MSG msg;while(PeekMessage(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessage(&msg);}MsgWaitForMultipleObjects(0,nullptr,FALSE,5,QS_ALLINPUT);}while(GetTickCount()-start<milliseconds);
}
BOOL BeginCaret(HWND window){BOOL result=HookedCreateCaret(window,nullptr,3,24);HookedSetCaretPos(20,20);HookedShowCaret(window);return result;}
struct ThreadCheck {HANDLE ready,done;bool restored=false;} threadCheck;
DWORD WINAPI OtherThread(void*){
 Dib bg(400,220);background=&bg;HWND h=CreateGlassWindow();FillBackground(h,bg,0);BeginCaret(h);
 SetEvent(threadCheck.ready);
 while(WaitForSingleObject(threadCheck.done,0)!=WAIT_OBJECT_0)Pump(10);
 threadCheck.restored=!FindLegacyCaret()&&ReadPixel(h)==0x00ffffff;
 DestroyCaret();DestroyWindow(h);background=nullptr;return 0;
}
int main(){
 setvbuf(stdout,nullptr,_IONBF,0);
 HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,GetCurrentProcessId());MODULEENTRY32 m={};m.dwSize=sizeof(m);
 if(snap!=INVALID_HANDLE_VALUE&&Module32First(snap,&m))do{if(wcsstr(m.szModule,L"translucent-windows")){fprintf(stderr,"Test contaminated by active mod injection\n");CloseHandle(snap);return 2;}}while(Module32Next(snap,&m));if(snap!=INVALID_HANDLE_VALUE)CloseHandle(snap);
 HMODULE user=GetModuleHandleW(L"win32u.dll");
 CreateCaret_orig=reinterpret_cast<decltype(&CreateCaret)>(GetProcAddress(user,"NtUserCreateCaret"));
 DestroyCaret_orig=reinterpret_cast<decltype(&DestroyCaret)>(GetProcAddress(user,"NtUserDestroyCaret"));
 SetCaretPos_orig=reinterpret_cast<decltype(&SetCaretPos)>(GetProcAddress(user,"NtUserSetCaretPos"));
 ShowCaret_orig=reinterpret_cast<decltype(&ShowCaret)>(GetProcAddress(user,"NtUserShowCaret"));
 HideCaret_orig=reinterpret_cast<decltype(&HideCaret)>(GetProcAddress(user,"NtUserHideCaret"));
 if(!CreateCaret_orig||!DestroyCaret_orig||!SetCaretPos_orig||!ShowCaret_orig||!HideCaret_orig){fprintf(stderr,"Native caret entry points unavailable\n");return 2;}
 SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
 WNDCLASSW wc={};wc.lpfnWndProc=WindowProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"WindhawkCaretRegression";RegisterClassW(&wc);
 Dib bg(400,220);background=&bg;HWND h=CreateGlassWindow();FillBackground(h,bg,0);
 CreateCaret(h,nullptr,3,24);SetCaretPos(20,20);ShowCaret(h);
 DWORD native=ReadPixel(h);Check("native-caret-reproduces-zero-alpha-on-glass",native==0x00ffffff);HideCaret(h);Check("native-xor-hide-restores-background",ReadPixel(h)==0);DestroyCaret();
 CreateCaret(h,nullptr,3,24);SetCaretPos(20,20);ShowCaret(h);HideCaret(h);HideCaret(h);ShowCaret(h);ShowCaret(nullptr);
 Check("native-null-owner-show-is-supported",IsVisible()&&ReadPixel(h)==0x00ffffff);DestroyCaret();
 Check("default-caret-creation-succeeds",BeginCaret(h));
 DWORD fixed=ReadPixel(h);printf("native=%08lx fixed=%08lx blink-ms=%u\n",native,fixed,GetCaretBlinkTime());
 Check("glass-caret-is-opaque-with-exact-native-rgb",fixed==0xffffffff);
 Check("only-caret-sized-storage-is-created",FindLegacyCaret()&&FindLegacyCaret()->width*FindLegacyCaret()->height*sizeof(DWORD)==288);
 HookedHideCaret(h);Check("hide-restores-all-original-transparent-bits",ReadPixel(h)==0);
 HookedShowCaret(h);
 UINT blink=GetCaretBlinkTime();bool sawOn=false,sawOff=false;
 if(blink!=INFINITE&&blink>0&&blink<=1000){
  DWORD start=GetTickCount();while(GetTickCount()-start<blink*3+50){DWORD pixel=ReadPixel(h);sawOn|=pixel==0xffffffff;sawOff|=pixel==0;Pump(5);}
  Check("windows-native-blink-alternates-opaque-and-original-pixels",sawOn&&sawOff);
 }else printf("SKIP native-blink user-setting=%u\n",blink);
 HookedHideCaret(h);HookedShowCaret(h);HookedHideCaret(h);HookedHideCaret(h);
 Check("two-hide-calls-keep-caret-hidden",!IsVisible()&&ReadPixel(h)==0);
 HookedShowCaret(h);Check("first-show-does-not-cancel-second-hide",!IsVisible()&&ReadPixel(h)==0);
 HookedShowCaret(nullptr);
 DWORD retryStart=GetTickCount();bool nullOwnerOn=ReadPixel(h)==0xffffffff;
 while(!nullOwnerOn&&GetTickCount()-retryStart<1500){Pump(10);nullOwnerOn=ReadPixel(h)==0xffffffff;}
 // GUI_CARETBLINKING reports logical visibility, not the current blink phase.
 Check("second-show-with-null-owner-restores-caret",IsVisible()&&nullOwnerOn);
 HookedHideCaret(nullptr);Check("hide-with-null-owner-restores-background",!IsVisible()&&ReadPixel(h)==0);
 HookedShowCaret(h);
 HookedDestroyCaret();Check("destroy-releases-tracked-caret",!FindLegacyCaret()&&ReadPixel(h)==0);
 for(DWORD value:{0xff224466u,0x40201030u}){
  FillBackground(h,bg,value);BeginCaret(h);
  DWORD expected=0xff000000|((value^0x00ffffff)&0x00ffffff);
  printf("background=%08lx visible=%08lx expected=%08lx\n",value,ReadPixel(h),expected);
  Check(value>>24==255?"opaque-background-keeps-native-rgb-and-alpha":"partial-alpha-background-produces-valid-opaque-caret",ReadPixel(h)==expected);
  HookedHideCaret(h);Check("hide-restores-original-alpha-and-color-exactly",ReadPixel(h)==value);HookedDestroyCaret();
 }
 FillBackground(h,bg,0);GdiFlush();for(int y=0;y<bg.h;y++)for(int x=100;x<bg.w;x++)bg.pixels[y*bg.w+x]=0xff224466;
 InvalidateRect(h,nullptr,FALSE);UpdateWindow(h);BeginCaret(h);HookedSetCaretPos(120,20);
 Check("move-restores-old-transparent-position",ReadPixel(h,20,20)==0);
 Check("move-resamples-opaque-background",ReadPixel(h,120,20)==0xffddbb99);
 HookedSetCaretPos(20,20);Check("move-back-restores-opaque-position-and-visible-glass-caret",ReadPixel(h,120,20)==0xff224466&&ReadPixel(h)==0xffffffff);
 FillBackground(h,bg,0x40201030);Check("paint-refreshes-mask-for-changed-background-alpha",ReadPixel(h)==0xffdfefcf);
 HookedHideCaret(h);Check("hide-after-repaint-restores-new-background",ReadPixel(h)==0x40201030);HookedDestroyCaret();
 FillBackground(h,bg,0);
 Dib custom(3,24);std::fill_n(custom.pixels,72,0x00aabbccu);
 SelectObject(custom.dc,custom.old);
 HookedCreateCaret(h,custom.bitmap,0,0);HookedSetCaretPos(20,20);HookedShowCaret(h);
 Check("application-bitmap-caret-keeps-native-shape-and-alpha",!FindLegacyCaret()&&ReadPixel(h)==0x00aabbcc);
 HookedDestroyCaret();HookedCreateCaret(h,(HBITMAP)1,3,24);
 Check("gray-pattern-caret-stays-native",!FindLegacyCaret());HookedDestroyCaret();
 for(int gate=0;gate<3;gate++){
  g_settings.FillBg=gate!=0;g_settings.Unload=gate==1;g_settings.BgType=gate==2?g_settings.Default:g_settings.Blur;
  BeginCaret(h);Check(gate==0?"custom-rendering-off-retains-native-caret":gate==1?"unloading-retains-native-caret":"default-background-retains-native-caret",!FindLegacyCaret()&&ReadPixel(h)==0x00ffffff);HookedDestroyCaret();
 }
 g_settings.FillBg=true;g_settings.Unload=false;g_settings.BgType=g_settings.Blur;
 forceDcFailure=true;BeginCaret(h);forceDcFailure=false;Check("allocation-failure-retains-native-caret",!FindLegacyCaret()&&ReadPixel(h)==0x00ffffff);HookedDestroyCaret();
 forceSubclassFailure=true;BeginCaret(h);forceSubclassFailure=false;Check("subclass-failure-retains-native-caret",!FindLegacyCaret()&&ReadPixel(h)==0x00ffffff);HookedDestroyCaret();
 BeginCaret(h);RestoreLegacyCarets();Check("unload-restores-native-caret-before-freeing-bitmap",!FindLegacyCaret()&&ReadPixel(h)==0x00ffffff);DestroyCaret();
 BeginCaret(h);HookedHideCaret(h);HookedHideCaret(h);RestoreLegacyCarets();
 ShowCaret(h);Check("unload-preserves-nested-hide-count",!IsVisible());ShowCaret(h);Check("native-show-after-unload-restores-visibility",IsVisible()&&ReadPixel(h)==0x00ffffff);DestroyCaret();
 BeginCaret(h);HookedDestroyCaret();DWORD before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);LARGE_INTEGER a,b,f;QueryPerformanceFrequency(&f);QueryPerformanceCounter(&a);
 for(int i=0;i<500;i++){BeginCaret(h);HookedSetCaretPos(40,20);HookedSetCaretPos(20,20);HookedDestroyCaret();}
 QueryPerformanceCounter(&b);DWORD after=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
 Check("repeated-create-move-destroy-leaks-no-gdi-objects",before==after);
 printf("500-cycles GDI-before=%lu GDI-after=%lu cycle-us=%.3f source-bytes=288\n",before,after,1e6*(b.QuadPart-a.QuadPart)/f.QuadPart/500);
 BeginCaret(h);DestroyWindow(h);Check("destroying-owner-window-releases-caret-state",g_caretSurfaces.empty());background=nullptr;
 threadCheck.ready=CreateEvent(nullptr,TRUE,FALSE,nullptr);threadCheck.done=CreateEvent(nullptr,TRUE,FALSE,nullptr);HANDLE thread=CreateThread(nullptr,0,OtherThread,nullptr,0,nullptr);
 WaitForSingleObject(threadCheck.ready,5000);g_settings.Unload=true;RestoreLegacyCarets();SetEvent(threadCheck.done);WaitForSingleObject(thread,5000);
 Check("cross-thread-unload-restores-native-caret-and-releases-state",threadCheck.restored&&g_caretSurfaces.empty());
 CloseHandle(thread);CloseHandle(threadCheck.ready);CloseHandle(threadCheck.done);
 printf("Failures: %d\n",failures);return failures?1:0;
}
