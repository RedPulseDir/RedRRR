#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <intrin.h>

#include "../../dependencies/ImGui/imgui.h"
#include "../../dependencies/ImGui/imgui_impl_dx11.h"
#include "../../dependencies/ImGui/imgui_impl_win32.h"

#include "../entities/visuals/loop.hxx"
#include "../engine/hooks.hxx"

#pragma comment( lib, "d3d11.lib" )
#pragma comment( lib, "dxgi.lib" )

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

namespace kompy::render {

	using present_fn = HRESULT( __stdcall* )( IDXGISwapChain*, UINT, UINT );
	using resize_fn = HRESULT( __stdcall* )( IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT );

	inline constexpr std::uintptr_t real_present_offset{ 0x42DC8 };
	inline constexpr std::uintptr_t real_resize_offset{ 0x42DD0 };

	inline present_fn* g_real_present_ptr{ nullptr };
	inline resize_fn* g_real_resize_ptr{ nullptr };
	inline present_fn g_original_present{ nullptr };
	inline resize_fn g_original_resize{ nullptr };

	inline ID3D11Device* g_device{ nullptr };
	inline ID3D11DeviceContext* g_context{ nullptr };
	inline ID3D11RenderTargetView* g_render_target{ nullptr };
	inline HWND g_hwnd{ nullptr };
	inline WNDPROC g_original_wndproc{ nullptr };

	inline bool g_menu_open{ true };
	inline bool g_initialized{ false };

	inline auto create_render_target( IDXGISwapChain* swap_chain ) -> void {
		ID3D11Texture2D* back_buffer{ nullptr };
		swap_chain->GetBuffer( 0, IID_PPV_ARGS( &back_buffer ) );
		if ( back_buffer ) {
			g_device->CreateRenderTargetView( back_buffer, nullptr, &g_render_target );
			back_buffer->Release();
		}
	}

	inline auto cleanup_render_target() -> void {
		if ( g_render_target ) {
			g_render_target->Release();
			g_render_target = nullptr;
		}
	}

	inline auto style_kompy() -> void {
		auto& style = ImGui::GetStyle();
		auto& colors = style.Colors;

		style.WindowRounding = 6.0f;
		style.FrameRounding = 4.0f;
		style.GrabRounding = 4.0f;
		style.TabRounding = 4.0f;
		style.WindowPadding = ImVec2{ 10.0f, 10.0f };
		style.FramePadding = ImVec2{ 8.0f, 4.0f };
		style.ItemSpacing = ImVec2{ 8.0f, 6.0f };

		colors[ImGuiCol_WindowBg] = ImVec4{ 0.08f, 0.08f, 0.10f, 0.94f };
		colors[ImGuiCol_TitleBg] = ImVec4{ 0.10f, 0.10f, 0.12f, 1.00f };
		colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.14f, 0.14f, 0.18f, 1.00f };
		colors[ImGuiCol_FrameBg] = ImVec4{ 0.12f, 0.12f, 0.15f, 1.00f };
		colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.18f, 0.18f, 0.22f, 1.00f };
		colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.22f, 0.22f, 0.28f, 1.00f };
		colors[ImGuiCol_Button] = ImVec4{ 0.45f, 0.20f, 0.75f, 0.70f };
		colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.55f, 0.30f, 0.85f, 0.80f };
		colors[ImGuiCol_ButtonActive] = ImVec4{ 0.65f, 0.40f, 0.95f, 0.90f };
		colors[ImGuiCol_Header] = ImVec4{ 0.45f, 0.20f, 0.75f, 0.50f };
		colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.55f, 0.30f, 0.85f, 0.60f };
		colors[ImGuiCol_HeaderActive] = ImVec4{ 0.65f, 0.40f, 0.95f, 0.70f };
		colors[ImGuiCol_CheckMark] = ImVec4{ 0.70f, 0.40f, 1.00f, 1.00f };
		colors[ImGuiCol_SliderGrab] = ImVec4{ 0.55f, 0.30f, 0.85f, 1.00f };
		colors[ImGuiCol_SliderGrabActive] = ImVec4{ 0.70f, 0.40f, 1.00f, 1.00f };
		colors[ImGuiCol_Tab] = ImVec4{ 0.14f, 0.14f, 0.18f, 1.00f };
		colors[ImGuiCol_TabHovered] = ImVec4{ 0.55f, 0.30f, 0.85f, 0.80f };
		colors[ImGuiCol_TabSelected] = ImVec4{ 0.45f, 0.20f, 0.75f, 1.00f };
		colors[ImGuiCol_Border] = ImVec4{ 0.30f, 0.15f, 0.50f, 0.50f };
		colors[ImGuiCol_ScrollbarBg] = ImVec4{ 0.06f, 0.06f, 0.08f, 0.94f };
		colors[ImGuiCol_ScrollbarGrab] = ImVec4{ 0.30f, 0.15f, 0.50f, 0.80f };
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4{ 0.45f, 0.20f, 0.75f, 0.80f };
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4{ 0.55f, 0.30f, 0.85f, 1.00f };
	}

	inline auto CALLBACK wnd_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam ) -> LRESULT {
		if ( g_initialized && ImGui_ImplWin32_WndProcHandler( hwnd, msg, wparam, lparam ) )
			return 1;

		if ( g_menu_open && g_initialized ) {
			auto& io = ImGui::GetIO();
			switch ( msg ) {
			case WM_LBUTTONDOWN: case WM_LBUTTONUP:
			case WM_RBUTTONDOWN: case WM_RBUTTONUP:
			case WM_MBUTTONDOWN: case WM_MBUTTONUP:
			case WM_MOUSEWHEEL: case WM_MOUSEMOVE:
				if ( io.WantCaptureMouse )
					return 1;
				break;
			case WM_KEYDOWN: case WM_KEYUP: case WM_CHAR:
				if ( io.WantCaptureKeyboard )
					return 1;
				break;
			}
		}

		return CallWindowProcW( g_original_wndproc, hwnd, msg, wparam, lparam );
	}

	inline auto initialize( IDXGISwapChain* swap_chain ) -> void {
		swap_chain->GetDevice( IID_PPV_ARGS( &g_device ) );
		if ( !g_device ) return;

		g_device->GetImmediateContext( &g_context );
		if ( !g_context ) return;

		DXGI_SWAP_CHAIN_DESC desc{};
		swap_chain->GetDesc( &desc );
		g_hwnd = desc.OutputWindow;

		ImGui::CreateContext();

		auto& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.IniFilename = nullptr;

		style_kompy();

		ImGui_ImplWin32_Init( g_hwnd );
		ImGui_ImplDX11_Init( g_device, g_context );

		create_render_target( swap_chain );

		g_original_wndproc = reinterpret_cast<WNDPROC>(
			SetWindowLongPtrW( g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( wnd_proc ) )
		);

		g_initialized = true;
		std::printf( "[kompy] render initialized\n" );
	}

	inline auto render_menu() -> void {
		ImGui::SetNextWindowSize( ImVec2{ 520.0f, 460.0f }, ImGuiCond_FirstUseEver );

		ImGui::Begin( "kompy", &g_menu_open, ImGuiWindowFlags_NoCollapse );

		if ( ImGui::BeginTabBar( "##tabs" ) ) {
			if ( ImGui::BeginTabItem( "visuals" ) ) {
				ImGui::Spacing();
				auto& cfg = visuals::g_config;
				ImGui::Checkbox( "esp enabled", &cfg.enabled );

				const char* box_names[]{ "off", "normal", "corner", "3d" };
				ImGui::Combo( "box style", &cfg.box_style, box_names, 4 );

				ImGui::Checkbox( "skeleton", &cfg.skeleton );
				ImGui::Checkbox( "head dot", &cfg.head_dot );
				ImGui::Checkbox( "snap lines", &cfg.snap_lines );
				ImGui::Separator();
				ImGui::Checkbox( "health bar", &cfg.health_bar );
				ImGui::Checkbox( "armor bar", &cfg.armor_bar );
				ImGui::Checkbox( "name", &cfg.name );
				ImGui::Checkbox( "weapon", &cfg.weapon );
				ImGui::Checkbox( "enemy only", &cfg.enemy_only );
				ImGui::Separator();
				ImGui::Checkbox( "map radar", &cfg.radar );
				ImGui::Checkbox( "show dormant", &cfg.show_dormant );
				ImGui::Spacing();
				ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( "aim" ) ) {
				ImGui::Spacing();
				auto& aim = visuals::g_aim;
				ImGui::Checkbox( "aimbot", &aim.enabled );
				ImGui::Checkbox( "silent aim", &aim.silent );
				ImGui::Checkbox( "recoil compensation", &aim.rcs );
				ImGui::Checkbox( "visible only", &aim.visible_only );
				ImGui::SliderFloat( "fov", &aim.fov, 1.0f, 30.0f, "%.1f" );

				if ( !aim.silent )
					ImGui::SliderFloat( "smooth", &aim.smooth, 1.0f, 20.0f, "%.1f" );

				const char* bone_names[]{ "head", "neck", "chest" };
				int bone_sel = ( aim.bone_target == visuals::bones::head ) ? 0 :
					( aim.bone_target == visuals::bones::neck ) ? 1 : 2;
				if ( ImGui::Combo( "bone", &bone_sel, bone_names, 3 ) ) {
					constexpr int bone_ids[]{ visuals::bones::head, visuals::bones::neck, visuals::bones::spine_2 };
					aim.bone_target = bone_ids[bone_sel];
				}

				ImGui::Spacing();
				if ( aim.silent )
					ImGui::TextColored( ImVec4{ 0.4f, 1.0f, 0.4f, 1.0f }, "shoots anywhere, bullets hit target" );
				else
					ImGui::Text( "hold right click to aim" );
				ImGui::Spacing();
				ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( "misc" ) ) {
				ImGui::Spacing();
				auto& misc = visuals::g_misc;
				ImGui::Checkbox( "no flash", &misc.no_flash );
				ImGui::Checkbox( "no smoke", &misc.no_smoke );
				ImGui::Checkbox( "grenade prediction", &misc.grenade_prediction );
				ImGui::Separator();
				ImGui::Checkbox( "no recoil", &misc.no_recoil );
				ImGui::Checkbox( "rapid fire", &misc.rapid_fire );
				ImGui::Checkbox( "hit sound", &misc.hit_sound );
				ImGui::Separator();
				ImGui::Checkbox( "backtrack", &misc.backtrack );
				if ( misc.backtrack )
					ImGui::SliderFloat( "bt time", &misc.backtrack_time, 0.05f, 0.3f, "%.2fs" );
				ImGui::Separator();
				ImGui::Checkbox( "spectator list", &misc.spectator_list );
				ImGui::Checkbox( "anti-aim", &misc.anti_aim );
				if ( misc.anti_aim )
					ImGui::TextColored( ImVec4{ 0.4f, 1.0f, 0.4f, 1.0f }, "server sees you facing backwards" );
				ImGui::Spacing();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

		ImGui::End();
	}

	inline auto handle_input() -> void {
		static bool insert_held{ false };
		if ( GetAsyncKeyState( VK_F8 ) & 0x8000 ) {
			if ( !insert_held ) {
				g_menu_open = !g_menu_open;
				insert_held = true;
			}
		} else {
			insert_held = false;
		}
	}

	inline auto __stdcall hk_present( IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags ) -> HRESULT {
		if ( !g_initialized )
			initialize( swap_chain );

		if ( g_initialized ) {
			handle_input();

			__try {
				hooks::input_history::g_exploit.silent_aim = visuals::g_aim.silent;
				hooks::input_history::g_exploit.visible_only = visuals::g_aim.visible_only;
				hooks::input_history::g_exploit.aim_bone = visuals::g_aim.bone_target;
				hooks::input_history::g_exploit.aim_fov = visuals::g_aim.fov;
				hooks::input_history::g_exploit.local_index = visuals::g_local_index;

				visuals::update();
				visuals::run_aimbot();
			} __except ( EXCEPTION_EXECUTE_HANDLER ) {}

			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			__try {
				visuals::render();
			} __except ( EXCEPTION_EXECUTE_HANDLER ) {}

			if ( g_menu_open )
				render_menu();

			ImGui::EndFrame();
			ImGui::Render();

			g_context->OMSetRenderTargets( 1, &g_render_target, nullptr );
			ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData() );
		}

		return g_original_present( swap_chain, sync_interval, flags );
	}

	inline auto __stdcall hk_resize( IDXGISwapChain* swap_chain, UINT buffer_count,
		UINT width, UINT height, DXGI_FORMAT format, UINT swap_flags ) -> HRESULT {

		cleanup_render_target();

		auto result = g_original_resize( swap_chain, buffer_count, width, height, format, swap_flags );

		if ( g_initialized )
			create_render_target( swap_chain );

		return result;
	}

	inline auto install() -> bool {
		auto hook_base = reinterpret_cast<std::uintptr_t>( GetModuleHandleA( "graphics-hook64.dll" ) );
		if ( !hook_base ) {
			std::printf( "[kompy] graphics-hook64.dll not found, is OBS game capture active?\n" );
			return false;
		}

		std::printf( "[kompy] graphics-hook64.dll at 0x%llX\n", hook_base );

		g_real_present_ptr = reinterpret_cast<present_fn*>( hook_base + real_present_offset );
		g_real_resize_ptr = reinterpret_cast<resize_fn*>( hook_base + real_resize_offset );

		g_original_present = *g_real_present_ptr;
		g_original_resize = *g_real_resize_ptr;

		if ( !g_original_present ) {
			std::printf( "[kompy] RealPresent is null, OBS hook not active yet\n" );
			return false;
		}

		std::printf( "[kompy] original present: 0x%llX\n", reinterpret_cast<std::uintptr_t>( g_original_present ) );
		std::printf( "[kompy] original resize:  0x%llX\n", reinterpret_cast<std::uintptr_t>( g_original_resize ) );

		_InterlockedExchangePointer( reinterpret_cast<void* volatile*>( g_real_present_ptr ),
			reinterpret_cast<void*>( hk_present ) );

		if ( g_original_resize ) {
			_InterlockedExchangePointer( reinterpret_cast<void* volatile*>( g_real_resize_ptr ),
				reinterpret_cast<void*>( hk_resize ) );
		}

		std::printf( "[kompy] hooks installed\n" );
		return true;
	}

	inline auto remove() -> void {
		if ( g_real_present_ptr && g_original_present )
			_InterlockedExchangePointer( reinterpret_cast<void* volatile*>( g_real_present_ptr ),
				reinterpret_cast<void*>( g_original_present ) );

		if ( g_real_resize_ptr && g_original_resize )
			_InterlockedExchangePointer( reinterpret_cast<void* volatile*>( g_real_resize_ptr ),
				reinterpret_cast<void*>( g_original_resize ) );

		Sleep( 100 );

		if ( g_initialized ) {
			if ( g_original_wndproc && g_hwnd )
				SetWindowLongPtrW( g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( g_original_wndproc ) );

			ImGui_ImplDX11_Shutdown();
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();

			cleanup_render_target();

			if ( g_context ) { g_context->Release(); g_context = nullptr; }
			if ( g_device ) { g_device->Release(); g_device = nullptr; }

			g_initialized = false;
		}

		std::printf( "[kompy] hooks removed\n" );
	}

}
