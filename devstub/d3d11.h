#pragma once
#include <Windows.h>
#define D3D11_SDK_VERSION 7
#define IID_PPV_ARGS(p) 0, reinterpret_cast<void**>(p)
#define __uuidof(x) 0
#define SUCCEEDED(hr) ((hr)>=0)
#define DXGI_FORMAT_R8G8B8A8_UNORM 28
#define DXGI_USAGE_RENDER_TARGET_OUTPUT 0x20
#define DXGI_SWAP_EFFECT_DISCARD 0
#define D3D_FEATURE_LEVEL_11_0 0xb000
#define D3D_FEATURE_LEVEL_10_0 0xa000
#define D3D_DRIVER_TYPE_HARDWARE 1
typedef long HRESULT; typedef int DXGI_FORMAT; typedef int D3D_FEATURE_LEVEL; typedef int D3D_DRIVER_TYPE;
struct ID3D11Texture2D { void Release(); };
struct ID3D11RenderTargetView { void Release(); };
struct ID3D11DeviceContext { void Release(); void OMSetRenderTargets(unsigned, ID3D11RenderTargetView* const*, void*); };
struct ID3D11Device { void Release(); void GetImmediateContext(ID3D11DeviceContext**); HRESULT CreateRenderTargetView(ID3D11Texture2D*, void*, ID3D11RenderTargetView**); };
struct DXGI_SAMPLE_DESC { unsigned Count, Quality; };
struct DXGI_MODE_DESC { unsigned Width, Height; DXGI_FORMAT Format; };
struct DXGI_SWAP_CHAIN_DESC { DXGI_MODE_DESC BufferDesc; DXGI_SAMPLE_DESC SampleDesc; unsigned BufferUsage; unsigned BufferCount; HWND OutputWindow; int Windowed; int SwapEffect; unsigned Flags; };
struct IDXGISwapChain { void Release(); HRESULT GetBuffer(unsigned,int,void**); HRESULT GetDevice(int,void**); HRESULT GetDesc(DXGI_SWAP_CHAIN_DESC*); };
HRESULT D3D11CreateDeviceAndSwapChain(void*,D3D_DRIVER_TYPE,void*,unsigned,const D3D_FEATURE_LEVEL*,unsigned,unsigned,const DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**,ID3D11Device**,D3D_FEATURE_LEVEL*,ID3D11DeviceContext**);
