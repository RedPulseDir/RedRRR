#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#define __fastcall
#define __stdcall
#define WINAPI
#define APIENTRY
#define CALLBACK
#define IMGUI_IMPL_API
#define MAX_PATH 260
#define IMAGE_DOS_SIGNATURE 0x5A4D
#define IMAGE_NT_SIGNATURE 0x00004550
#define VK_SPACE 0x20
#define VK_END 0x23
#define VK_INSERT 0x2D
#define VK_LBUTTON 0x01
#define DLL_PROCESS_ATTACH 1
#define DLL_PROCESS_DETACH 0
#define TRUE 1
#define FALSE 0
#define MEM_COMMIT 0x1000
#define PAGE_NOACCESS 0x01
#define PAGE_GUARD 0x100
#define PAGE_READWRITE 0x04
#define PAGE_WRITECOPY 0x08
#define PAGE_EXECUTE_READWRITE 0x40
#define PAGE_EXECUTE_WRITECOPY 0x80
#define WS_OVERLAPPEDWINDOW 0xCF0000
#define GWLP_WNDPROC -4
#define WM_LBUTTONDOWN 0x201
#define WM_LBUTTONUP 0x202
#define WM_LBUTTONDBLCLK 0x203
#define WM_RBUTTONDOWN 0x204
#define WM_RBUTTONUP 0x205
#define WM_RBUTTONDBLCLK 0x206
#define WM_MBUTTONDOWN 0x207
#define WM_MBUTTONUP 0x208
#define WM_MBUTTONDBLCLK 0x209
#define WM_MOUSEWHEEL 0x20A
#define WM_MOUSEMOVE 0x200
#define WM_KEYDOWN 0x100
#define WM_KEYUP 0x101
#define WM_SYSKEYDOWN 0x104
#define WM_SYSKEYUP 0x105
#define WM_CHAR 0x102
typedef void* HMODULE; typedef void* HWND; typedef void* LPVOID; typedef const void* LPCVOID;
typedef void* HANDLE; typedef void* HINSTANCE; typedef void* HBRUSH;
typedef unsigned long DWORD; typedef int BOOL; typedef short SHORT; typedef unsigned int UINT;
typedef uintptr_t WPARAM; typedef intptr_t LPARAM; typedef intptr_t LRESULT; typedef intptr_t LONG_PTR;
struct IMAGE_DOS_HEADER { uint16_t e_magic; long e_lfanew; };
struct IMAGE_OPTIONAL_HEADER { uint32_t SizeOfImage; };
struct IMAGE_NT_HEADERS { uint32_t Signature; IMAGE_OPTIONAL_HEADER OptionalHeader; };
struct MEMORY_BASIC_INFORMATION { void* BaseAddress; void* AllocationBase; DWORD AllocationProtect; size_t RegionSize; DWORD State; DWORD Protect; DWORD Type; };
typedef LRESULT (CALLBACK* WNDPROC)(HWND,UINT,WPARAM,LPARAM);
struct WNDCLASSEXA { UINT cbSize; UINT style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra; HINSTANCE hInstance; void* hIcon; void* hCursor; HBRUSH hbrBackground; const char* lpszMenuName; const char* lpszClassName; void* hIconSm; };
inline size_t VirtualQuery(LPCVOID,MEMORY_BASIC_INFORMATION*,size_t){return 0;}
inline BOOL DisableThreadLibraryCalls(HMODULE){return 1;}
inline HANDLE CreateThread(void*,size_t,DWORD(*)(LPVOID),LPVOID,DWORD,DWORD*){return nullptr;}
inline DWORD WaitForSingleObject(HANDLE,DWORD){return 0;}
inline BOOL CloseHandle(HANDLE){return 1;}
inline long InterlockedExchange(volatile long*,long){return 0;}
inline long InterlockedCompareExchange(volatile long*,long,long){return 0;}
inline void Sleep(DWORD){}
inline HMODULE GetModuleHandleA(const char*){return nullptr;}
inline DWORD GetModuleFileNameA(HMODULE,char*,DWORD){return 0;}
inline DWORD GetTempPathA(DWORD,char*){return 0;}
inline DWORD GetLastError(){return 0;}
inline SHORT GetAsyncKeyState(int){return 0;}
inline HWND GetForegroundWindow(){return nullptr;}
inline DWORD GetWindowThreadProcessId(HWND,DWORD*){return 0;}
inline DWORD GetCurrentProcessId(){return 0;}
inline BOOL AllocConsole(){return 1;}
inline BOOL FreeConsole(){return 1;}
inline void FreeLibraryAndExitThread(HMODULE,DWORD){}
inline unsigned short RegisterClassExA(const WNDCLASSEXA*){return 1;}
inline BOOL UnregisterClassA(const char*,HINSTANCE){return 1;}
inline HWND CreateWindowExA(DWORD,const char*,const char*,DWORD,int,int,int,int,HWND,void*,HINSTANCE,void*){return nullptr;}
inline BOOL DestroyWindow(HWND){return 1;}
inline LRESULT DefWindowProcA(HWND,UINT,WPARAM,LPARAM){return 0;}
inline LRESULT CallWindowProcA(WNDPROC,HWND,UINT,WPARAM,LPARAM){return 0;}
inline LONG_PTR SetWindowLongPtrA(HWND,int,LONG_PTR){return 0;}
inline int fopen_s(FILE** f,const char* n,const char* m){*f=fopen(n,m);return *f?0:1;}
inline int freopen_s(FILE** f,const char* n,const char* m,FILE* s){*f=freopen(n,m,s);return 0;}
inline int strcpy_s(char* d,size_t n,const char* s){strncpy(d,s,n);return 0;}
inline int strcat_s(char* d,size_t n,const char* s){strncat(d,s,n);return 0;}
template<size_t N,typename... A> inline int sprintf_s(char (&d)[N],const char* f,A... a){return snprintf(d,N,f,a...);}
struct RECT { long left, top, right, bottom; };
inline BOOL GetClientRect(HWND,RECT*){return 1;}
inline unsigned long long GetTickCount64(){return 0;}
typedef unsigned long long ULONGLONG;
#define WM_INPUT 0x00FF
