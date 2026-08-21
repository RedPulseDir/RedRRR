#pragma once
#include <Windows.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <d3d11.h>
#include <dxgi.h>

#include "Config.h"
#include "MinHook/MinHook.h"
#include "SilentAim.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"

void Log(const char* fmt, ...);

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);

// ---------------------------------------------------------------------------
// ImGui-меню поверх игры.
//
// Рисуем в её же swapchain: поднимаем временный D3D11-девайс, снимаем vtable
// IDXGISwapChain и хукаем Present (индекс 8) и ResizeBuffers (13) через MinHook.
// Это надёжнее слоёного окна: работает и в exclusive fullscreen, не мигает и
// не зависит от композитора.
//
// INSERT открывает/закрывает меню. Пока меню открыто, ввод мыши и клавиатуры
// съедается в WndProc, чтобы игра не крутила камеру, когда ты тыкаешь галочки.
// ---------------------------------------------------------------------------

namespace menu {

using present_fn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
using resize_fn  = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

inline present_fn oPresent = nullptr;
inline resize_fn  oResize  = nullptr;

inline ID3D11Device*           g_device  = nullptr;
inline ID3D11DeviceContext*    g_context = nullptr;
inline ID3D11RenderTargetView* g_rtv     = nullptr;
inline HWND                    g_hwnd    = nullptr;
inline WNDPROC                 g_oWndProc = nullptr;

inline bool g_init      = false;
inline bool g_installed = false;
inline bool g_prevMenuKey = false;

// база стиля до масштабирования: ScaleAllSizes умножает на месте, поэтому
// каждый раз пересчитываем от нетронутой копии, а не от уже растянутой.
inline IDXGISwapChain* g_swap = nullptr; // свапчейн, на котором подняли ImGui

// CS2 держит мышь в relative mode (raw input + удержание курсора в центре),
// поэтому GetCursorPos, на который опирается стандартный бэкенд, всё время
// возвращает одну точку. Из-за этого курсор ImGui стоял на месте, а клик
// уходил в тот виджет, что оказался под центром экрана. Ведём свою позицию
// из WM_INPUT.
inline ImVec2 g_cursor{-1.0f, -1.0f};
inline bool   g_mouse[3]{};
inline float  g_wheel = 0.0f;

inline ImGuiStyle g_baseStyle{};
inline float      g_scale      = 0.0f;
inline ImVec2     g_lastSize{0.0f, 0.0f};
inline bool       g_replaceMenu = true;

inline void CreateRTV(IDXGISwapChain* swap) {
    ID3D11Texture2D* back = nullptr;
    if (SUCCEEDED(swap->GetBuffer(0, IID_PPV_ARGS(&back))) && back) {
        g_device->CreateRenderTargetView(back, nullptr, &g_rtv);
        back->Release();
    }
}

inline void ReleaseRTV() {
    if (g_rtv) {
        g_rtv->Release();
        g_rtv = nullptr;
    }
}

inline void PushCursor(const float dx, const float dy) {
    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    if (g_cursor.x < 0.0f) {
        g_cursor = ImVec2(disp.x * 0.5f, disp.y * 0.5f);
    }
    g_cursor.x += dx;
    g_cursor.y += dy;
    if (g_cursor.x < 0.0f) g_cursor.x = 0.0f;
    if (g_cursor.y < 0.0f) g_cursor.y = 0.0f;
    if (g_cursor.x > disp.x) g_cursor.x = disp.x;
    if (g_cursor.y > disp.y) g_cursor.y = disp.y;
}

inline void HandleRawInput(const LPARAM lp) {
    UINT size = 0;
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT, nullptr, &size,
                        sizeof(RAWINPUTHEADER)) != 0)
        return;

    BYTE buf[sizeof(RAWINPUT) + 32]{};
    if (size > sizeof(buf))
        return;
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT, buf, &size,
                        sizeof(RAWINPUTHEADER)) != size)
        return;

    const auto* ri = reinterpret_cast<RAWINPUT*>(buf);
    if (ri->header.dwType != RIM_TYPEMOUSE)
        return;

    const RAWMOUSE& m = ri->data.mouse;
    if ((m.usFlags & MOUSE_MOVE_ABSOLUTE) == 0)
        PushCursor(static_cast<float>(m.lLastX), static_cast<float>(m.lLastY));

    const USHORT f = m.usButtonFlags;
    if (f & RI_MOUSE_LEFT_BUTTON_DOWN)   g_mouse[0] = true;
    if (f & RI_MOUSE_LEFT_BUTTON_UP)     g_mouse[0] = false;
    if (f & RI_MOUSE_RIGHT_BUTTON_DOWN)  g_mouse[1] = true;
    if (f & RI_MOUSE_RIGHT_BUTTON_UP)    g_mouse[1] = false;
    if (f & RI_MOUSE_MIDDLE_BUTTON_DOWN) g_mouse[2] = true;
    if (f & RI_MOUSE_MIDDLE_BUTTON_UP)   g_mouse[2] = false;
    if (f & RI_MOUSE_WHEEL)
        g_wheel += static_cast<float>(static_cast<SHORT>(m.usButtonData)) / 120.0f;
}

inline LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (g_init && cfg::g.menuOpen) {
        switch (msg) {
        // мышь ведём сами, бэкенду её не отдаём: иначе он перезапишет позицию
        // замороженным GetCursorPos
        case WM_INPUT:
            HandleRawInput(lp);
            return DefWindowProcA(hwnd, msg, wp, lp); // системе нужна уборка

        case WM_MOUSEMOVE: {
            // на случай если игра не в relative mode
            const float x = static_cast<float>(static_cast<short>(LOWORD(lp)));
            const float y = static_cast<float>(static_cast<short>(HIWORD(lp)));
            RECT        rc{};
            ImVec2      disp = ImGui::GetIO().DisplaySize;
            if (GetClientRect(hwnd, &rc)) {
                const float cw = static_cast<float>(rc.right - rc.left);
                const float ch = static_cast<float>(rc.bottom - rc.top);
                if (cw > 0.0f && ch > 0.0f)
                    g_cursor = ImVec2(x * disp.x / cw, y * disp.y / ch);
            }
            return 1;
        }

        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: g_mouse[0] = true;  return 1;
        case WM_LBUTTONUP:                          g_mouse[0] = false; return 1;
        case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: g_mouse[1] = true;  return 1;
        case WM_RBUTTONUP:                          g_mouse[1] = false; return 1;
        case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: g_mouse[2] = true;  return 1;
        case WM_MBUTTONUP:                          g_mouse[2] = false; return 1;
        case WM_MOUSEWHEEL:
            g_wheel += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp)) / 120.0f;
            return 1;

        // клавиатуру отдаём бэкенду как есть и не пускаем в игру
        case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP: case WM_CHAR:
            ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp);
            return 1;

        default:
            ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp);
            break;
        }
    }
    return CallWindowProcA(g_oWndProc, hwnd, msg, wp, lp);
}

inline void Style() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 6.0f;
    s.FrameRounding     = 4.0f;
    s.GrabRounding      = 4.0f;
    s.WindowBorderSize  = 1.0f;
    s.WindowPadding     = ImVec2(14, 12);
    s.ItemSpacing       = ImVec2(9, 8);
    s.FramePadding      = ImVec2(8, 4);
    s.ScrollbarSize     = 12.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]        = ImVec4(0.07f, 0.07f, 0.09f, 0.96f);
    c[ImGuiCol_Border]          = ImVec4(0.24f, 0.26f, 0.33f, 0.85f);
    c[ImGuiCol_TitleBg]         = ImVec4(0.10f, 0.11f, 0.14f, 1.00f);
    c[ImGuiCol_TitleBgActive]   = ImVec4(0.14f, 0.16f, 0.21f, 1.00f);
    c[ImGuiCol_FrameBg]         = ImVec4(0.14f, 0.15f, 0.19f, 1.00f);
    c[ImGuiCol_FrameBgHovered]  = ImVec4(0.19f, 0.21f, 0.27f, 1.00f);
    c[ImGuiCol_FrameBgActive]   = ImVec4(0.23f, 0.26f, 0.33f, 1.00f);
    c[ImGuiCol_CheckMark]       = ImVec4(0.35f, 0.82f, 0.48f, 1.00f);
    c[ImGuiCol_SliderGrab]      = ImVec4(0.35f, 0.78f, 1.00f, 1.00f);
    c[ImGuiCol_SliderGrabActive]= ImVec4(0.48f, 0.86f, 1.00f, 1.00f);
    c[ImGuiCol_Header]          = ImVec4(0.18f, 0.20f, 0.26f, 1.00f);
    c[ImGuiCol_HeaderHovered]   = ImVec4(0.24f, 0.27f, 0.35f, 1.00f);
    c[ImGuiCol_Separator]       = ImVec4(0.24f, 0.26f, 0.33f, 1.00f);

    g_baseStyle = s;
}

// множитель интерфейса: 1080p = 1.0. На 4K меню не превращается в марку, на
// 1024x768 не занимает пол-экрана.
inline float WantedScale(const float displayH) {
    if (!cfg::g.autoScale)
        return cfg::g.uiScale < 0.4f ? 0.4f : (cfg::g.uiScale > 4.0f ? 4.0f : cfg::g.uiScale);
    float s = displayH / 1080.0f;
    if (s < 0.6f) s = 0.6f;
    if (s > 3.0f) s = 3.0f;
    return s;
}

inline void ApplyScale(const float s) {
    if (s > g_scale - 0.005f && s < g_scale + 0.005f)
        return;
    ImGuiStyle& st = ImGui::GetStyle();
    st             = g_baseStyle;
    st.ScaleAllSizes(s);
    ImGui::GetIO().FontGlobalScale = s;
    g_scale                        = s;
}

// FOV-круг и водяной знак. Рисуем в background draw list, чтобы это жило
// независимо от того, открыто меню или нет.
inline void DrawVisuals() {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImVec2 sz = ImGui::GetIO().DisplaySize;
    const ImVec2 mid(sz.x * 0.5f, sz.y * 0.5f);

    if (cfg::g.showFovCircle && sz.y > 1.0f) {
        // Пикселей на радиан по вертикали: (H/2) / tan(vFov/2). Вертикальный
        // угол не зависит от соотношения сторон, поэтому одна формула честно
        // работает и на 4:3, и на 16:9, и на 21:9, и на растянутых режимах.
        //
        // gameFov задан как горизонтальный при 4:3 (так его понимает Source),
        // отсюда vFov = 2*atan(tan(hFov/2) / (4/3)).
        const float k    = 3.14159265358979f / 180.0f;
        const float hHalf = tanf(cfg::g.gameFov * 0.5f * k);
        const float vHalf = hHalf / (4.0f / 3.0f);
        if (vHalf > 0.0f) {
            const float pxPerRad = (sz.y * 0.5f) / vHalf;
            float       r        = tanf(cfg::g.fov * k) * pxPerRad;
            const float cap      = (sz.x < sz.y ? sz.x : sz.y) * 0.5f * 1.6f;
            if (r > cap)
                r = cap;
            if (r > 2.0f) {
                const ImU32 col = silent::g_hasTarget && cfg::g.showTargetRing
                                      ? IM_COL32(255, 190, 60, 220)
                                      : IM_COL32(90, 200, 255, 160);
                // сегментов по размеру круга, иначе на 4K видно многоугольник
                int seg = static_cast<int>(r * 0.4f);
                if (seg < 32)  seg = 32;
                if (seg > 256) seg = 256;
                dl->AddCircle(mid, r, col, seg, cfg::g.circleThick * g_scale);
            }
        }
    }

    if (cfg::g.showWatermark) {
        char buf[96];
        sprintf_s(buf, "SpaggetiHop | silent %s | %s | %.0fx%.0f", cfg::g.silent ? "on" : "off",
                  silent::g_hooked ? "input-history" : "fallback", sz.x, sz.y);
        const float fs = ImGui::GetFontSize();
        dl->AddText(ImGui::GetFont(), fs, ImVec2(12.0f * g_scale, 10.0f * g_scale),
                    IM_COL32(235, 235, 235, 220), buf);
    }
}

inline void DrawMenu() {
    const ImVec2 disp = ImGui::GetIO().DisplaySize;

    // при смене разрешения окно возвращаем в кадр: иначе после перехода с 4K
    // на 1080p меню оказывается за границей экрана и его не достать
    const ImGuiCond cond = g_replaceMenu ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
    g_replaceMenu        = false;

    float w = 380.0f * g_scale;
    if (w > disp.x * 0.9f)
        w = disp.x * 0.9f;
    float h = 620.0f * g_scale;
    if (h > disp.y * 0.85f)
        h = disp.y * 0.85f;

    // ФИКСИРОВАННЫЙ размер. Раньше тут стоял AlwaysAutoResize вместе с явным
    // SetNextWindowSize и ограничениями — они дрались каждый кадр, окно
    // пульсировало, виджеты прыгали, и клик попадал не в ту кнопку.
    ImGui::SetNextWindowSize(ImVec2(w, h), cond);
    ImGui::SetNextWindowPos(ImVec2(60.0f * g_scale, 60.0f * g_scale), cond);

    if (ImGui::Begin("SpaggetiHop", &cfg::g.menuOpen, ImGuiWindowFlags_NoCollapse)) {
        if (ImGui::CollapsingHeader("Bunnyhop", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Enabled (hold SPACE)", &cfg::g.bhop);
            ImGui::Checkbox("Subtick jump timing", &cfg::g.subtick);
            ImGui::SliderFloat("Stamina cap", &cfg::g.staminaCap, 0.0f, 200.0f, "%.0f");
            ImGui::TextDisabled("200 = gate off");
        }

        if (ImGui::CollapsingHeader("Autostrafe")) {
            ImGui::Checkbox("Move strafe (cmd input)", &cfg::g.moveStrafe);
            ImGui::Checkbox("View strafe (experimental)", &cfg::g.viewStrafe);
            ImGui::SliderFloat("Strafe amount", &cfg::g.strafeMove, 50.0f, 450.0f, "%.0f");
            ImGui::SliderFloat("Yaw deadzone", &cfg::g.yawDeadzone, 0.0f, 5.0f, "%.2f");
            ImGui::TextDisabled("Both fight your own input, keep off if bhop feels wrong");
        }

        if (ImGui::CollapsingHeader("Silent aim", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Enabled (hold LMB)", &cfg::g.silent);
            ImGui::SliderFloat("FOV", &cfg::g.fov, 1.0f, 90.0f, "%.0f deg");
            ImGui::Checkbox("Visible only", &cfg::g.visibleOnly);
            ImGui::TextDisabled("spotted-mask filter, not a real trace");
            ImGui::SliderFloat("Max distance", &cfg::g.maxDist, 512.0f, 8192.0f, "%.0f");
            ImGui::SliderInt("Bone", &cfg::g.aimBone, 0, 30);
            ImGui::TextDisabled("6 = head");
            ImGui::BeginDisabled(true);
            ImGui::Checkbox("Aim punch compensation", &cfg::g.aimPunch);
            ImGui::EndDisabled();
            ImGui::TextDisabled("needs m_aimPunchAngle offset");
        }

        if (ImGui::CollapsingHeader("Triggerbot")) {
            ImGui::Checkbox("Auto fire on lock", &cfg::g.triggerbot);
            ImGui::SliderInt("Delay", &cfg::g.triggerMs, 0, 500, "%d ms");
            ImGui::TextDisabled("fires whenever a target is inside the FOV circle");
        }

        if (ImGui::CollapsingHeader("Visuals", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Show FOV circle", &cfg::g.showFovCircle);
            ImGui::Checkbox("Highlight when locked", &cfg::g.showTargetRing);
            ImGui::Checkbox("Watermark", &cfg::g.showWatermark);
            ImGui::SliderFloat("Circle thickness", &cfg::g.circleThick, 0.5f, 5.0f, "%.1f");
            ImGui::SliderFloat("Game FOV", &cfg::g.gameFov, 60.0f, 120.0f, "%.0f");
            ImGui::TextDisabled("your CS2 fov setting, horizontal at 4:3");
        }

        if (ImGui::CollapsingHeader("Interface")) {
            ImGui::Checkbox("Auto scale by resolution", &cfg::g.autoScale);
            ImGui::BeginDisabled(cfg::g.autoScale);
            ImGui::SliderFloat("UI scale", &cfg::g.uiScale, 0.5f, 3.0f, "%.2fx");
            ImGui::EndDisabled();
            const ImVec2 d = ImGui::GetIO().DisplaySize;
            ImGui::Text("render %.0fx%.0f  scale %.2fx", d.x, d.y, g_scale);
        }

        ImGui::Separator();
        ImGui::Text("target: %s", silent::g_hasTarget ? "LOCKED" : "-");
        ImGui::Text("hook:   %s", silent::g_hooked ? "input-history" : "fallback");
        ImGui::TextDisabled("INSERT menu | END unload");
    }
    ImGui::End();
}

inline HRESULT __stdcall hkPresent(IDXGISwapChain* swap, UINT sync, UINT flags) {
    // Движок и сторонние оверлеи могут держать несколько свапчейнов. Рисуем
    // только в свой, иначе ImGui получает по два NewFrame на кадр и интерфейс
    // мерцает. Но при смене режима экрана игра пересоздаёт свапчейн — тогда
    // подхватываем новый, если он принадлежит тому же окну, иначе меню
    // исчезнет навсегда после первого же alt-enter.
    if (g_init && swap != g_swap) {
        DXGI_SWAP_CHAIN_DESC sd{};
        if (SUCCEEDED(swap->GetDesc(&sd)) && sd.OutputWindow == g_hwnd) {
            ReleaseRTV();
            g_swap = swap;
            CreateRTV(swap);
            g_replaceMenu = true;
            Log("[menu] swapchain replaced, re-attached\n");
        } else {
            return oPresent(swap, sync, flags);
        }
    }

    if (!g_init) {
        if (SUCCEEDED(swap->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&g_device)))) {
            g_device->GetImmediateContext(&g_context);

            DXGI_SWAP_CHAIN_DESC desc{};
            swap->GetDesc(&desc);
            g_hwnd = desc.OutputWindow;

            CreateRTV(swap);

            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
            Style();

            ImGui_ImplWin32_Init(g_hwnd);
            ImGui_ImplDX11_Init(g_device, g_context);

            g_oWndProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));

            g_swap = swap;
            g_init = true;
            Log("[menu] imgui up, hwnd=%p\n", g_hwnd);
        } else {
            return oPresent(swap, sync, flags);
        }
    }

    // INSERT: тумблер меню. Читаем здесь, а не в WndProc, чтобы работало даже
    // когда ввод съеден.
    const bool keyDown = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    if (keyDown && !g_prevMenuKey) {
        cfg::g.menuOpen = !cfg::g.menuOpen;
        if (cfg::g.menuOpen) {
            // курсор в центр и кнопки отпущены: иначе меню открывается с
            // зажатой ЛКМ, если ты как раз стрелял
            g_cursor  = ImVec2(-1.0f, -1.0f);
            g_mouse[0] = g_mouse[1] = g_mouse[2] = false;
            g_wheel    = 0.0f;
        }
    }
    g_prevMenuKey = keyDown;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();

    // win32-бэкенд берёт размер из клиентской области окна, но игра может
    // рендерить в буфер другого размера (масштаб рендера, DSR, оконный режим
    // с ненативным разрешением). Рисовать надо в координатах бэкбуфера, иначе
    // на таких настройках интерфейс уезжает и мышь не совпадает с курсором.
    {
        ImGuiIO&             io = ImGui::GetIO();
        DXGI_SWAP_CHAIN_DESC sd{};
        if (SUCCEEDED(swap->GetDesc(&sd)) && sd.BufferDesc.Width && sd.BufferDesc.Height) {
            const float bw = static_cast<float>(sd.BufferDesc.Width);
            const float bh = static_cast<float>(sd.BufferDesc.Height);

            io.DisplaySize = ImVec2(bw, bh);
        }

        if (io.DisplaySize.x != g_lastSize.x || io.DisplaySize.y != g_lastSize.y) {
            g_lastSize    = io.DisplaySize;
            g_replaceMenu = true;
            Log("[menu] render target %.0fx%.0f\n", io.DisplaySize.x, io.DisplaySize.y);
        }
        ApplyScale(WantedScale(io.DisplaySize.y));
    }

    // Свой ввод кладём в очередь событий ПОСЛЕ бэкенда: UpdateInputEvents
    // применяет их по порядку, поэтому последнее событие побеждает. Прямая
    // запись в io.MousePos тут не работает вовсе — её затирает очередь.
    {
        ImGuiIO& io = ImGui::GetIO();
        if (cfg::g.menuOpen) {
            if (g_cursor.x < 0.0f)
                g_cursor = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
            io.AddMousePosEvent(g_cursor.x, g_cursor.y);
            io.AddMouseButtonEvent(0, g_mouse[0]);
            io.AddMouseButtonEvent(1, g_mouse[1]);
            io.AddMouseButtonEvent(2, g_mouse[2]);
            if (g_wheel != 0.0f) {
                io.AddMouseWheelEvent(0.0f, g_wheel);
                g_wheel = 0.0f;
            }
        }
        io.MouseDrawCursor = cfg::g.menuOpen;
    }

    ImGui::NewFrame();

    DrawVisuals();
    if (cfg::g.menuOpen)
        DrawMenu();

    ImGui::Render();
    if (!g_rtv)
        CreateRTV(swap);
    if (g_rtv) {
        g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    return oPresent(swap, sync, flags);
}

inline HRESULT __stdcall hkResize(IDXGISwapChain* swap, UINT count, UINT w, UINT h,
                                  DXGI_FORMAT fmt, UINT flags) {
    ReleaseRTV();
    const HRESULT hr = oResize(swap, count, w, h, fmt, flags);
    if (g_init)
        CreateRTV(swap);
    return hr;
}

// временный swapchain, только чтобы снять указатели из vtable
inline bool GrabVTable(void** presentOut, void** resizeOut) {
    WNDCLASSEXA wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = DefWindowProcA;
    wc.hInstance     = GetModuleHandleA(nullptr);
    wc.lpszClassName = "SpgDummy";
    RegisterClassExA(&wc);

    HWND wnd = CreateWindowExA(0, wc.lpszClassName, "", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100,
                               nullptr, nullptr, wc.hInstance, nullptr);
    if (!wnd) {
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return false;
    }

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount                        = 1;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = wnd;
    sd.SampleDesc.Count                   = 1;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL       level{};
    const D3D_FEATURE_LEVEL want[]{D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};

    IDXGISwapChain*      swap = nullptr;
    ID3D11Device*        dev  = nullptr;
    ID3D11DeviceContext* ctx  = nullptr;

    const HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                                     want, 2, D3D11_SDK_VERSION, &sd, &swap, &dev,
                                                     &level, &ctx);
    bool ok = false;
    if (SUCCEEDED(hr) && swap) {
        void** vt   = *reinterpret_cast<void***>(swap);
        *presentOut = vt[8];  // IDXGISwapChain::Present
        *resizeOut  = vt[13]; // IDXGISwapChain::ResizeBuffers
        ok          = true;
    } else {
        Log("[menu] D3D11CreateDeviceAndSwapChain failed: 0x%lX\n", static_cast<unsigned long>(hr));
    }

    if (swap) swap->Release();
    if (ctx)  ctx->Release();
    if (dev)  dev->Release();
    DestroyWindow(wnd);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);
    return ok;
}

inline bool Install() {
    void* present = nullptr;
    void* resize  = nullptr;
    if (!GrabVTable(&present, &resize))
        return false;

    if (MH_CreateHook(present, reinterpret_cast<LPVOID>(&hkPresent),
                      reinterpret_cast<LPVOID*>(&oPresent)) != MH_OK) {
        Log("[menu] MH_CreateHook(Present) failed\n");
        return false;
    }
    if (MH_CreateHook(resize, reinterpret_cast<LPVOID>(&hkResize),
                      reinterpret_cast<LPVOID*>(&oResize)) != MH_OK) {
        Log("[menu] MH_CreateHook(ResizeBuffers) failed\n");
        return false;
    }
    if (MH_EnableHook(present) != MH_OK || MH_EnableHook(resize) != MH_OK) {
        Log("[menu] MH_EnableHook failed\n");
        return false;
    }

    g_installed = true;
    Log("[menu] present hook @ %p, resize @ %p\n", present, resize);
    return true;
}

inline void Remove() {
    if (!g_installed)
        return;

    // сначала снимаем хуки и даём кадру в полёте закончиться, потом рушим ImGui
    MH_DisableHook(MH_ALL_HOOKS);
    Sleep(200);

    if (g_init) {
        if (g_oWndProc && g_hwnd)
            SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_oWndProc));

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        ReleaseRTV();
        if (g_context) { g_context->Release(); g_context = nullptr; }
        if (g_device)  { g_device->Release();  g_device  = nullptr; }
        g_init = false;
    }

    g_installed = false;
    Log("[menu] removed\n");
}

} // namespace menu
