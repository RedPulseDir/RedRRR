#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <Windows.h>

#include "classes.hxx"
#include "sdk/scanner.hxx"

namespace kompy::hooks {

	inline constexpr int ABS_JMP_SIZE = 14;

	struct trampoline_t {
		std::uint8_t* gateway{};
		std::uint8_t saved[32]{};
		std::uintptr_t target{};
		int stolen{};
		bool active{};

		auto install( std::uintptr_t fn, std::uintptr_t detour, int bytes ) -> bool {
			if ( bytes < ABS_JMP_SIZE )
				return false;

			target = fn;
			stolen = bytes;

			gateway = static_cast<std::uint8_t*>(
				VirtualAlloc( nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE ) );
			if ( !gateway )
				return false;

			std::memcpy( saved, reinterpret_cast<void*>( fn ), bytes );

			std::memcpy( gateway, saved, bytes );
			auto ret = fn + bytes;
			gateway[bytes + 0] = 0xFF;
			gateway[bytes + 1] = 0x25;
			*reinterpret_cast<std::uint32_t*>( gateway + bytes + 2 ) = 0;
			*reinterpret_cast<std::uintptr_t*>( gateway + bytes + 6 ) = ret;

			DWORD protect{};
			VirtualProtect( reinterpret_cast<void*>( fn ), bytes, PAGE_EXECUTE_READWRITE, &protect );

			auto p = reinterpret_cast<std::uint8_t*>( fn );
			p[0] = 0xFF;
			p[1] = 0x25;
			*reinterpret_cast<std::uint32_t*>( p + 2 ) = 0;
			*reinterpret_cast<std::uintptr_t*>( p + 6 ) = detour;
			for ( int i = ABS_JMP_SIZE; i < bytes; ++i )
				p[i] = 0x90;

			VirtualProtect( reinterpret_cast<void*>( fn ), bytes, protect, &protect );
			FlushInstructionCache( GetCurrentProcess(), reinterpret_cast<void*>( fn ), bytes );

			active = true;
			return true;
		}

		auto remove() -> void {
			if ( !active )
				return;

			DWORD protect{};
			VirtualProtect( reinterpret_cast<void*>( target ), stolen, PAGE_EXECUTE_READWRITE, &protect );
			std::memcpy( reinterpret_cast<void*>( target ), saved, stolen );
			VirtualProtect( reinterpret_cast<void*>( target ), stolen, protect, &protect );
			FlushInstructionCache( GetCurrentProcess(), reinterpret_cast<void*>( target ), stolen );

			if ( gateway ) {
				VirtualFree( gateway, 0, MEM_RELEASE );
				gateway = nullptr;
			}

			active = false;
		}

		template <typename T>
		auto original() const -> T { return reinterpret_cast<T>( gateway ); }
	};

	namespace input_history {

		struct c_input_message {
			std::int32_t frame_tick_count;
			float frame_tick_fraction;
			std::int32_t player_tick_count;
			float player_tick_fraction;
			float view_angles[3];
			float shoot_position[3];
			std::int32_t target_ent_index;
			float target_head_position[3];
			float target_abs_origin[3];
			float target_angle[3];
			std::int32_t sv_show_hit_reg;
			std::int32_t entry_index_max;
			std::int32_t index;
		};

		struct exploit_config_t {
			bool silent_aim{};
			bool visible_only{ true };
			int aim_bone{ 6 };
			float aim_fov{ 15.0f };
			int local_index{};
		};

		struct aim_target_t {
			bool valid{};
			int entity_index{};
			math::vec3_t head_pos{};
			math::vec3_t origin{};
			float eye_angles[3]{};
		};

		using populate_fn = std::int64_t( __fastcall* )(
			c_input_message* msg,
			std::int64_t entry,
			char verify,
			double a4,
			int a5,
			std::int64_t player_pawn );

		inline trampoline_t g_hook{};
		inline volatile bool g_active{};
		inline exploit_config_t g_exploit{};

		inline auto find_target( const math::vec3_t& eye_pos, const math::vec2_t& view_angles ) -> aim_target_t {
			aim_target_t best{};
			float best_fov = g_exploit.aim_fov;

			auto list_addr = game::read<std::uintptr_t>( game::ctx::client + offsets::client::dwEntityList );
			if ( !game::is_valid_ptr( list_addr ) )
				return {};

			game::c_entity_list list{ list_addr };

			auto local_addr = game::read<std::uintptr_t>( game::ctx::client + offsets::client::dwLocalPlayerController );
			game::c_cs_player_controller local_ctrl{ local_addr };
			auto local_team = local_ctrl.valid() ? local_ctrl.team_num() : static_cast<std::uint8_t>( 0 );

			for ( int i{ 1 }; i <= 64; ++i ) {
				auto ctrl = list.get_controller( i );
				if ( !ctrl || ctrl.address() == local_addr )
					continue;

				auto handle = ctrl.player_pawn_handle();
				auto pawn = list.get_pawn_from_handle( handle );
				if ( !pawn || !pawn.is_alive() )
					continue;

				if ( pawn.team_num() == local_team )
					continue;

				if ( g_exploit.visible_only && g_exploit.local_index > 0 ) {
					if ( !pawn.is_visible_to( g_exploit.local_index ) )
						continue;
				}

				auto origin = pawn.origin();
				if ( origin.is_zero() )
					continue;

				auto dist = ( origin - eye_pos ).length();
				if ( dist > 4000.0f )
					continue;

				auto bone = pawn.bone_position( g_exploit.aim_bone );
				if ( bone.is_zero() )
					continue;

				auto dz = bone.z - origin.z;
				if ( g_exploit.aim_bone == 6 && dz < 40.0f )
					continue;
				if ( dz < -10.0f || dz > 90.0f )
					continue;

				auto angle = math::calc_angle( eye_pos, bone );
				auto fov = math::angle_fov( view_angles, angle );

				if ( fov < best_fov ) {
					best_fov = fov;
					best.valid = true;
					best.entity_index = static_cast<int>( handle & 0x7FFF );
					best.head_pos = bone;
					best.origin = origin;
					best.eye_angles[0] = game::read<float>( pawn.address() + netvars::C_CSPlayerPawnBase::m_angEyeAngles );
					best.eye_angles[1] = game::read<float>( pawn.address() + netvars::C_CSPlayerPawnBase::m_angEyeAngles + 4 );
					best.eye_angles[2] = 0.0f;
				}
			}

			return best;
		}

		inline std::int64_t __fastcall hk_populate(
			c_input_message* msg,
			std::int64_t entry,
			char verify,
			double a4,
			int a5,
			std::int64_t player_pawn )
		{
			__try {
				float saved_angles[3]{};
				bool modified{};

				if (  g_exploit.silent_aim && msg && player_pawn ) {
				
						auto pawn_addr = static_cast<std::uintptr_t>( player_pawn );

						if ( game::is_valid_ptr( pawn_addr ) ) {
							game::c_cs_player_pawn local{ pawn_addr };
							auto eye_pos = local.eye_position();

							if ( !eye_pos.is_zero() && !local.is_holding_grenade() ) {
								math::vec2_t view{ msg->view_angles[0], msg->view_angles[1] };
								auto target = find_target( eye_pos, view );

								if ( target.valid ) {
									saved_angles[0] = msg->view_angles[0];
									saved_angles[1] = msg->view_angles[1];
									saved_angles[2] = msg->view_angles[2];

									auto aim = math::calc_angle( eye_pos, target.head_pos );

									auto punch = local.aim_punch_angle();
									aim.x -= punch.x * 2.0f;
									aim.y -= punch.y * 2.0f;
									aim.x = std::clamp( math::normalize_angle( aim.x ), -89.0f, 89.0f );
									aim.y = math::normalize_angle( aim.y );

									msg->view_angles[0] = aim.x;
									msg->view_angles[1] = aim.y;
									msg->view_angles[2] = 0.0f;

									msg->shoot_position[0] = eye_pos.x;
									msg->shoot_position[1] = eye_pos.y;
									msg->shoot_position[2] = eye_pos.z;

									msg->target_ent_index = target.entity_index;

									msg->target_head_position[0] = target.head_pos.x;
									msg->target_head_position[1] = target.head_pos.y;
									msg->target_head_position[2] = target.head_pos.z;

									msg->target_abs_origin[0] = target.origin.x;
									msg->target_abs_origin[1] = target.origin.y;
									msg->target_abs_origin[2] = target.origin.z;

									msg->target_angle[0] = target.eye_angles[0];
									msg->target_angle[1] = target.eye_angles[1];
									msg->target_angle[2] = target.eye_angles[2];

									modified = true;
								}
							}
						
					}
				}

				auto result = g_hook.original<populate_fn>()( msg, entry, verify, a4, a5, player_pawn );

				if ( modified ) {
					msg->view_angles[0] = saved_angles[0];
					msg->view_angles[1] = saved_angles[1];
					msg->view_angles[2] = saved_angles[2];
				}

				return result;
			} __except ( EXCEPTION_EXECUTE_HANDLER ) {
				return g_hook.original<populate_fn>()( msg, entry, verify, a4, a5, player_pawn );
			}
		}

		inline auto install() -> bool {
			auto client = game::ctx::client;
			if ( !client )
				return false;

			auto size = scanner::module_size( client );

			auto addr = scanner::find_pattern( client, size,
				"48 89 5C 24 18 55 57 41 56 48 8D 6C 24 ?? 48 81 EC ?? 00 00 00 8B 01 48 8B F9 81 4A 10 00 02 00 00" );

			if ( !addr ) {
				std::printf( "[kompy] populate_history_entry: pattern not found\n" );
				return false;
			}

			std::printf( "[kompy] populate_history_entry at 0x%llX (rva 0x%llX)\n",
				static_cast<unsigned long long>( addr ),
				static_cast<unsigned long long>( addr - client ) );

			if ( !g_hook.install( addr, reinterpret_cast<std::uintptr_t>( hk_populate ), ABS_JMP_SIZE ) ) {
				std::printf( "[kompy] populate_history_entry: hook install failed\n" );
				return false;
			}

			g_active = true;
			std::printf( "[kompy] input history hook active\n" );
			return true;
		}

		inline auto remove() -> void {
			g_active = false;
			Sleep( 100 );
			g_hook.remove();
			std::printf( "[kompy] input history hook removed\n" );
		}
	}

}
