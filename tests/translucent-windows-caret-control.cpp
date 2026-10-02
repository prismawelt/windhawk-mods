#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstring>
#include <string>
bool expectedMod=false;
std::wstring expectedVersion;
LRESULT CALLBACK WindowProc(HWND h,UINT m,WPARAM w,LPARAM l){
 if(m==WM_CTLCOLOREDIT){SetBkColor((HDC)w,0);SetTextColor((HDC)w,RGB(255,255,255));return (LRESULT)GetStockObject(BLACK_BRUSH);}
 if(m==WM_PAINT){PAINTSTRUCT ps;HDC d=BeginPaint(h,&ps);FillRect(d,&ps.rcPaint,(HBRUSH)GetStockObject(BLACK_BRUSH));EndPaint(h,&ps);return 0;}
 return DefWindowProc(h,m,w,l);
}
bool FindMod(){
 bool found=false;HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,GetCurrentProcessId());MODULEENTRY32 m={sizeof(m)};
 if(snap!=INVALID_HANDLE_VALUE&&Module32First(snap,&m))do{if(wcsstr(m.szModule,L"translucent-windows")){wprintf(L"loaded=%ls\n",m.szModule);found|=!expectedMod||wcsstr(m.szModule,expectedVersion.c_str())!=nullptr;}}while(Module32Next(snap,&m));
 if(snap!=INVALID_HANDLE_VALUE)CloseHandle(snap);return found;
}
void Pump(DWORD ms){DWORD a=GetTickCount();do{MSG m;while(PeekMessage(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessage(&m);}MsgWaitForMultipleObjects(0,nullptr,FALSE,5,QS_ALLINPUT);}while(GetTickCount()-a<ms);}
DWORD ReadPixel(HWND h,int x,int y){
 HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=1;bi.bmiHeader.biHeight=-1;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;DWORD* p;
 HBITMAP b=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,(void**)&p,nullptr,0);HGDIOBJ old=SelectObject(dc,b);HDC source=GetDC(h);BitBlt(dc,0,0,1,1,source,x,y,SRCCOPY);ReleaseDC(h,source);GdiFlush();DWORD result=*p;SelectObject(dc,old);DeleteObject(b);DeleteDC(dc);return result;
}
int main(int argc,char** argv){
 setvbuf(stdout,nullptr,_IONBF,0);expectedMod=argc>1;
 if(expectedMod){expectedVersion=L"_";for(const char* p=argv[1];*p;p++)expectedVersion+=static_cast<wchar_t>(*p);expectedVersion+=L"_";}
 if(expectedMod){DWORD a=GetTickCount();while(!FindMod()&&GetTickCount()-a<15000)Sleep(200);if(!FindMod()){printf("FAIL required-production-mod-not-loaded\n");return 2;}}
 else if(FindMod()){printf("FAIL baseline-contaminated\n");return 2;}
 SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
 WNDCLASSW wc={};wc.lpfnWndProc=WindowProc;wc.hInstance=GetModuleHandle(nullptr);wc.lpszClassName=L"WindhawkNativeEditVerification";RegisterClassW(&wc);
 HWND h=CreateWindowW(wc.lpszClassName,L"Temporary edit caret verification",WS_OVERLAPPEDWINDOW,40,40,500,260,nullptr,nullptr,wc.hInstance,nullptr);
 MARGINS margins={-1,-1,-1,-1};DwmExtendFrameIntoClientArea(h,&margins);
 HWND edit=CreateWindowW(L"Edit",L"",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,30,40,350,65,h,(HMENU)17,wc.hInstance,nullptr);
 SendMessageW(edit,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),FALSE);
 ShowWindow(h,SW_SHOWNOACTIVATE);UpdateWindow(h);UpdateWindow(edit);SetFocus(edit);SendMessageW(edit,EM_SETSEL,0,0);
 GUITHREADINFO info={sizeof(info)};GetGUIThreadInfo(GetCurrentThreadId(),&info);wchar_t cls[80]={};GetClassNameW(info.hwndCaret,cls,80);
 printf("caret-is-owned-edit=%d rc=%ld,%ld,%ld,%ld logicalVisible=%d\n",info.hwndCaret==edit,info.rcCaret.left,info.rcCaret.top,info.rcCaret.right,info.rcCaret.bottom,(info.flags&GUI_CARETBLINKING)!=0);
 bool on=false,off=false;DWORD last=~0u,start=GetTickCount();
 while(GetTickCount()-start<2400){
  GUITHREADINFO i={sizeof(i)};GetGUIThreadInfo(GetCurrentThreadId(),&i);
  if(i.hwndCaret==edit){DWORD p=ReadPixel(edit,i.rcCaret.left,(i.rcCaret.top+i.rcCaret.bottom)/2);if(p!=last){printf("pixel +%lu = %08lx\n",GetTickCount()-start,p);last=p;}on|=expectedMod?p==0xffffffff:p==0x00ffffff;off|=p==0;}
  Pump(10);
 }
 bool caretPass=info.hwndCaret==edit&&on&&off;
 printf("%s native-edit-caret-%s-with-native-blink\n",caretPass?"PASS":"FAIL",expectedMod?"opaque":"zero-alpha");
 for(int i=0;i<5;i++){SetFocus(h);SetFocus(edit);}
 DWORD before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
 for(int i=0;i<100;i++){SetFocus(h);SetFocus(edit);}
 DWORD after=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
 printf("%s native-edit-focus-cycles-leak-no-gdi-objects before=%lu after=%lu\n",before==after?"PASS":"FAIL",before,after);
 SetFocus(h);DestroyWindow(h);return caretPass&&before==after?0:1;
}
