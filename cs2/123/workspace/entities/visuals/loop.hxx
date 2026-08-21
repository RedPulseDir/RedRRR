#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <utility>
#include <Windows.h>

#include "../../engine/classes.hxx"
#include "../../engine/sdk/offsets.hxx"
#include "cache.hxx"

#include <imgui.h>

namespace kompy::visuals {

	namespace bones {
		enum id : int {
			pelvis    = 0,
			spine_1   = 2,
			spine_2   = 4,
			neck      = 5,
			head      = 6,
			l_shoulder = 8,
			l_elbow   = 9,
			l_hand    = 11,
			r_shoulder = 13,
			r_elbow   = 14,
			r_hand    = 16,
			l_hip     = 22,
			l_knee    = 23,
			l_foot    = 24,
			r_hip     = 25,
			r_knee    = 26,
			r_foot    = 27
		};

		inline constexpr int connections[][2] {
			{ head, neck }, { neck, spine_2 }, { spine_2, spine_1 }, { spine_1, pelvis },
			{ neck, l_shoulder }, { l_shoulder, l_elbow }, { l_elbow, l_hand },
			{ neck, r_shoulder }, { r_shoulder, r_elbow }, { r_elbow, r_hand },
			{ pelvis, l_hip }, { l_hip, l_knee }, { l_knee, l_foot },
			{ pelvis, r_hip }, { r_hip, r_knee }, { r_knee, r_foot }
		};
	}

	struct esp_config_t {
		bool enabled{ true };
		int box_style{ 1 };
		bool skeleton{ true };
		bool health_bar{ true };
		bool armor_bar{};
		bool name{ true };
		bool weapon{ true };
		bool snap_lines{};
		bool head_dot{};
		bool enemy_only{ true };
		bool radar{};
		bool show_dormant{ true };

		ImU32 enemy_color{ IM_COL32( 230, 60, 60, 255 ) };
		ImU32 team_color{ IM_COL32( 60, 140, 230, 255 ) };
		ImU32 bone_color{ IM_COL32( 255, 255, 255, 200 ) };
	};

	struct aim_config_t {
		bool enabled{};
		bool silent{};
		bool rcs{ true };
		bool visible_only{ true };
		int bone_target{ bones::head };
		float fov{ 5.0f };
		float smooth{ 4.0f };
		int key{ VK_RBUTTON };
	};

	namespace cmd {
		inline constexpr std::ptrdiff_t buffer_ptr = 0x2030878;
		inline constexpr int cmd_size = 152;
		inline constexpr int cmd_count = 150;
		inline constexpr int seq_offset = 22800;
		inline constexpr std::ptrdiff_t input_angle_offset = 0x270;

		inline auto clear_subtick_angles( std::uintptr_t base_pb ) -> void {
			auto subtick_array = game::read<std::uintptr_t>( base_pb + 0x28 );
			if ( !game::is_valid_ptr( subtick_array ) )
				return;

			auto count = game::read<int>( base_pb + 0x20 );
			if ( count <= 0 || count > 64 )
				return;

			for ( int i{}; i < count; ++i ) {
				auto entry = game::read<std::uintptr_t>( subtick_array + 8 + i * 8 );
				if ( !game::is_valid_ptr( entry ) )
					continue;

				*reinterpret_cast<float*>( entry + 0x30 ) = 0.0f;
				*reinterpret_cast<float*>( entry + 0x34 ) = 0.0f;
				*reinterpret_cast<int*>( entry + 0x10 ) &= ~0x60;
			}
		}

		inline auto write_angles( std::uintptr_t client, float pitch, float yaw ) -> bool {
			auto array = game::read<std::uintptr_t>( client + buffer_ptr );
			if ( !game::is_valid_ptr( array ) )
				return false;

			auto buffer = game::read<std::uintptr_t>( array );
			if ( !game::is_valid_ptr( buffer ) )
				return false;

			auto sequence = game::read<int>( buffer + seq_offset );
			if ( sequence <= 0 )
				return false;

			auto usercmd = buffer + cmd_size * ( sequence % cmd_count );
			auto base_pb = game::read<std::uintptr_t>( usercmd + 0x40 );
			if ( !game::is_valid_ptr( base_pb ) )
				return false;

			*reinterpret_cast<float*>( base_pb + 0x58 ) = pitch;
			*reinterpret_cast<float*>( base_pb + 0x5C ) = yaw;
			*reinterpret_cast<int*>( base_pb + 0x10 ) |= 0xC0;

			clear_subtick_angles( base_pb );

			auto input_base = client + offsets::client::dwCSGOInput;
			auto* cmd_pitch = reinterpret_cast<float*>( input_base + input_angle_offset );
			auto* cmd_yaw = reinterpret_cast<float*>( input_base + input_angle_offset + 4 );
			*cmd_pitch = pitch;
			*cmd_yaw = yaw;

			return true;
		}
	}

	struct misc_config_t {
		bool no_flash{};
		bool no_smoke{};
		bool grenade_prediction{};
		bool no_recoil{};
		bool rapid_fire{};
		bool backtrack{};
		float backtrack_time{ 0.2f };
		bool hit_sound{};
		bool spectator_list{};
		bool anti_aim{};
	};

	inline esp_config_t g_config{};
	inline aim_config_t g_aim{};
	inline misc_config_t g_misc{};

	inline auto weapon_name_from_index( std::uint16_t idx ) -> const char* {
		switch ( idx ) {
		case 1:  return "desert eagle";
		case 2:  return "dual berettas";
		case 3:  return "five-seven";
		case 4:  return "glock";
		case 7:  return "ak-47";
		case 8:  return "aug";
		case 9:  return "awp";
		case 10: return "famas";
		case 11: return "g3sg1";
		case 13: return "galil";
		case 14: return "m249";
		case 16: return "m4a4";
		case 17: return "mac-10";
		case 19: return "p90";
		case 23: return "mp5-sd";
		case 24: return "ump-45";
		case 25: return "xm1014";
		case 26: return "pp-bizon";
		case 27: return "mag-7";
		case 28: return "negev";
		case 29: return "sawed-off";
		case 30: return "tec-9";
		case 31: return "zeus";
		case 32: return "p2000";
		case 33: return "mp7";
		case 34: return "mp9";
		case 35: return "nova";
		case 36: return "p250";
		case 38: return "scar-20";
		case 39: return "sg 553";
		case 40: return "ssg 08";
		case 42: return "knife";
		case 43: return "flashbang";
		case 44: return "he grenade";
		case 45: return "smoke";
		case 46: return "molotov";
		case 47: return "decoy";
		case 48: return "incendiary";
		case 49: return "c4";
		case 57: return "mac-10";
		case 59: return "knife";
		case 60: return "m4a1-s";
		case 61: return "usp-s";
		case 63: return "cz75";
		case 64: return "revolver";
		default: return nullptr;
		}
	}

	inline auto health_color( int health ) -> ImU32 {
		auto t = std::clamp( health / 100.0f, 0.0f, 1.0f );
		auto r = static_cast<int>( ( 1.0f - t ) * 230.0f + t * 60.0f );
		auto g = static_cast<int>( t * 230.0f + ( 1.0f - t ) * 60.0f );
		return IM_COL32( r, g, 60, 255 );
	}

	inline auto draw_shadow_text( ImDrawList* draw, float x, float y, ImU32 color, const char* text ) -> void {
		draw->AddText( ImVec2{ x + 1.0f, y + 1.0f }, IM_COL32( 0, 0, 0, 180 ), text );
		draw->AddText( ImVec2{ x, y }, color, text );
	}

	inline auto dim_color( ImU32 col, float alpha_mult ) -> ImU32 {
		auto a = static_cast<int>( ( ( col >> 24 ) & 0xFF ) * alpha_mult );
		return ( col & 0x00FFFFFF ) | ( static_cast<ImU32>( a ) << 24 );
	}

	inline auto render_player( ImDrawList* draw, const cached_player_t& player,
		const math::view_matrix_t& matrix, float display_w, float display_h ) -> void {

		math::vec2_t screen_head{};
		math::vec2_t screen_feet{};

		if ( !math::world_to_screen( player.head_pos, matrix, screen_head, display_w, display_h ) )
			return;
		if ( !math::world_to_screen( player.origin, matrix, screen_feet, display_w, display_h ) )
			return;

		auto box_h = screen_feet.y - screen_head.y;
		if ( box_h < 4.0f )
			return;

		float alpha = player.dormant ? 0.4f : 1.0f;
		auto box_w = box_h * 0.45f;
		auto box_x = screen_head.x - box_w * 0.5f;
		auto box_y = screen_head.y;
		auto color = player.is_enemy ? g_config.enemy_color : g_config.team_color;
		if ( player.dormant )
			color = dim_color( color, alpha );

		auto outline = dim_color( IM_COL32( 0, 0, 0, 120 ), alpha );

		if ( g_config.box_style == 1 ) {
			draw->AddRect(
				ImVec2{ box_x - 1.0f, box_y - 1.0f },
				ImVec2{ box_x + box_w + 1.0f, box_y + box_h + 1.0f },
				outline, 0.0f, 0, 1.0f );
			draw->AddRect(
				ImVec2{ box_x, box_y },
				ImVec2{ box_x + box_w, box_y + box_h },
				color, 0.0f, 0, 1.6f );
		}
		else if ( g_config.box_style == 2 ) {
			auto len = box_h * 0.22f;
			auto bx2 = box_x + box_w;
			auto by2 = box_y + box_h;

			draw->AddLine( ImVec2{ box_x, box_y }, ImVec2{ box_x + len, box_y }, color, 2.0f );
			draw->AddLine( ImVec2{ box_x, box_y }, ImVec2{ box_x, box_y + len }, color, 2.0f );
			draw->AddLine( ImVec2{ bx2, box_y }, ImVec2{ bx2 - len, box_y }, color, 2.0f );
			draw->AddLine( ImVec2{ bx2, box_y }, ImVec2{ bx2, box_y + len }, color, 2.0f );
			draw->AddLine( ImVec2{ box_x, by2 }, ImVec2{ box_x + len, by2 }, color, 2.0f );
			draw->AddLine( ImVec2{ box_x, by2 }, ImVec2{ box_x, by2 - len }, color, 2.0f );
			draw->AddLine( ImVec2{ bx2, by2 }, ImVec2{ bx2 - len, by2 }, color, 2.0f );
			draw->AddLine( ImVec2{ bx2, by2 }, ImVec2{ bx2, by2 - len }, color, 2.0f );
		}
		else if ( g_config.box_style == 3 ) {
			auto head_h = player.head_pos.z - player.origin.z + 8.0f;
			constexpr float hw = 16.0f;

			math::vec3_t verts[8] {
				{ player.origin.x - hw, player.origin.y - hw, player.origin.z },
				{ player.origin.x + hw, player.origin.y - hw, player.origin.z },
				{ player.origin.x + hw, player.origin.y + hw, player.origin.z },
				{ player.origin.x - hw, player.origin.y + hw, player.origin.z },
				{ player.origin.x - hw, player.origin.y - hw, player.origin.z + head_h },
				{ player.origin.x + hw, player.origin.y - hw, player.origin.z + head_h },
				{ player.origin.x + hw, player.origin.y + hw, player.origin.z + head_h },
				{ player.origin.x - hw, player.origin.y + hw, player.origin.z + head_h },
			};

			math::vec2_t sv[8]{};
			bool ok[8]{};
			for ( int i{}; i < 8; ++i )
				ok[i] = math::world_to_screen( verts[i], matrix, sv[i], display_w, display_h );

			constexpr int edges[][2] {
				{0,1},{1,2},{2,3},{3,0},
				{4,5},{5,6},{6,7},{7,4},
				{0,4},{1,5},{2,6},{3,7}
			};

			for ( const auto& [a, b] : edges ) {
				if ( !ok[a] || !ok[b] ) continue;
				draw->AddLine( ImVec2{ sv[a].x, sv[a].y }, ImVec2{ sv[b].x, sv[b].y }, outline, 2.6f );
				draw->AddLine( ImVec2{ sv[a].x, sv[a].y }, ImVec2{ sv[b].x, sv[b].y }, color, 1.2f );
			}
		}

		if ( g_config.snap_lines && !player.dormant ) {
			draw->AddLine(
				ImVec2{ display_w * 0.5f, display_h },
				ImVec2{ screen_feet.x, screen_feet.y },
				dim_color( color, 0.5f ), 1.0f );
		}

		if ( g_config.head_dot && !player.dormant ) {
			draw->AddCircleFilled( ImVec2{ screen_head.x, screen_head.y - 2.0f }, 3.0f, color );
		}

		if ( g_config.skeleton && !player.dormant ) {
			for ( const auto& [from, to] : bones::connections ) {
				if ( player.bones[from].is_zero() || player.bones[to].is_zero() )
					continue;

				math::vec2_t s0{};
				math::vec2_t s1{};

				if ( !math::world_to_screen( player.bones[from], matrix, s0, display_w, display_h ) )
					continue;
				if ( !math::world_to_screen( player.bones[to], matrix, s1, display_w, display_h ) )
					continue;

				draw->AddLine( ImVec2{ s0.x, s0.y }, ImVec2{ s1.x, s1.y }, g_config.bone_color, 1.4f );
			}
		}

		if ( g_config.health_bar ) {
			auto bar_x = box_x - 5.0f;
			auto bar_h = box_h * ( std::clamp( player.health, 0, 100 ) / 100.0f );
			auto bar_y = box_y + box_h - bar_h;

			draw->AddRectFilled(
				ImVec2{ bar_x - 1.0f, box_y - 1.0f },
				ImVec2{ bar_x + 2.0f, box_y + box_h + 1.0f },
				outline );
			draw->AddRectFilled(
				ImVec2{ bar_x, bar_y },
				ImVec2{ bar_x + 1.0f, box_y + box_h },
				dim_color( health_color( player.health ), alpha ) );
		}

		if ( g_config.armor_bar && player.armor > 0 ) {
			auto bar_x = box_x + box_w + 3.0f;
			auto bar_h = box_h * ( std::clamp( player.armor, 0, 100 ) / 100.0f );
			auto bar_y = box_y + box_h - bar_h;

			draw->AddRectFilled(
				ImVec2{ bar_x - 1.0f, box_y - 1.0f },
				ImVec2{ bar_x + 2.0f, box_y + box_h + 1.0f },
				outline );
			draw->AddRectFilled(
				ImVec2{ bar_x, bar_y },
				ImVec2{ bar_x + 1.0f, box_y + box_h },
				dim_color( IM_COL32( 60, 140, 230, 255 ), alpha ) );
		}

		auto name_y = box_y - ImGui::CalcTextSize( "A" ).y - 2.0f;

		if ( g_config.name && player.name[0] ) {
			auto text_size = ImGui::CalcTextSize( player.name );
			draw_shadow_text( draw,
				screen_head.x - text_size.x * 0.5f,
				name_y,
				dim_color( IM_COL32( 255, 255, 255, 255 ), alpha ), player.name );
		}

		if ( player.dormant ) {
			auto tag = "[dormant]";
			auto tag_size = ImGui::CalcTextSize( tag );
			draw_shadow_text( draw,
				screen_head.x - tag_size.x * 0.5f,
				box_y + box_h + 2.0f,
				IM_COL32( 255, 200, 50, 160 ), tag );
		} else if ( g_config.weapon && player.weapon[0] ) {
			auto text_size = ImGui::CalcTextSize( player.weapon );
			draw_shadow_text( draw,
				screen_feet.x - text_size.x * 0.5f,
				box_y + box_h + 2.0f,
				dim_color( IM_COL32( 200, 200, 200, 220 ), alpha ), player.weapon );
		}
	}

	inline auto render_grenade_prediction( ImDrawList* draw, float display_w, float display_h ) -> void {
		if ( !g_misc.grenade_prediction || !game::ctx::client )
			return;

		auto local_pawn_addr = game::read<std::uintptr_t>( game::ctx::client + offsets::client::dwLocalPlayerPawn );
		if ( !game::is_valid_ptr( local_pawn_addr ) )
			return;

		game::c_cs_player_pawn local_pawn{ local_pawn_addr };
		if ( !local_pawn.is_alive() || !local_pawn.is_holding_grenade() )
			return;

		if ( !local_pawn.grenade_pin_pulled() )
			return;

		auto throw_strength = local_pawn.grenade_throw_strength();
		if ( throw_strength <= 0.01f )
			throw_strength = 1.0f;

		auto eye_pos = local_pawn.eye_position();
		auto player_origin = local_pawn.origin();
		if ( eye_pos.is_zero() || player_origin.is_zero() )
			return;

		auto* angle_ptr = reinterpret_cast<math::vec2_t*>( game::ctx::client + offsets::client::dwViewAngles );
		auto angles = *angle_ptr;

		auto pitch_rad = angles.x * math::deg2rad;
		auto yaw_rad = angles.y * math::deg2rad;

		math::vec3_t forward{
			std::cosf( pitch_rad ) * std::cosf( yaw_rad ),
			std::cosf( pitch_rad ) * std::sinf( yaw_rad ),
			-std::sinf( pitch_rad )
		};

		math::vec3_t right{
			-std::sinf( yaw_rad ),
			std::cosf( yaw_rad ),
			0.0f
		};

		math::vec3_t up{
			-std::sinf( pitch_rad ) * std::cosf( yaw_rad ),
			-std::sinf( pitch_rad ) * std::sinf( yaw_rad ),
			-std::cosf( pitch_rad )
		};

		auto base_speed = local_pawn.grenade_throw_velocity();
		auto throw_vel = std::clamp( throw_strength, 0.0f, 1.0f );
		auto speed = base_speed * throw_vel * 0.9f + 100.0f;

		auto throw_height = ( throw_vel * 12.0f ) - 2.0f;

		math::vec3_t vel{
			forward.x * speed + up.x * throw_height,
			forward.y * speed + up.y * throw_height,
			forward.z * speed + up.z * throw_height
		};

		auto start = eye_pos + forward * 16.0f;

		auto matrix = game::read<math::view_matrix_t>( game::ctx::client + offsets::client::dwViewMatrix );

		constexpr float dt = 1.0f / 64.0f;
		constexpr float gravity = 400.0f;
		constexpr float drag_per_tick = 0.99f;
		constexpr float bounce_factor = 0.45f;
		constexpr int max_steps = 300;
		constexpr int max_bounces = 5;

		auto pos = start;
		auto ground_z = player_origin.z;
		math::vec2_t prev_screen{};
		bool has_prev{};
		int bounces{};
		bool landed{};
		math::vec3_t land_pos{};

		auto line_color = IM_COL32( 255, 200, 50, 200 );
		auto bounce_color = IM_COL32( 255, 140, 40, 200 );
		auto end_color = IM_COL32( 255, 60, 60, 240 );

		for ( int step{}; step < max_steps; ++step ) {
			auto next = pos;
			next.x += vel.x * dt;
			next.y += vel.y * dt;
			next.z += vel.z * dt;

			vel.z -= gravity * dt;

			vel.x *= drag_per_tick;
			vel.y *= drag_per_tick;
			vel.z *= drag_per_tick;

			if ( next.z <= ground_z ) {
				next.z = ground_z;
				++bounces;

				if ( bounces >= max_bounces || std::fabsf( vel.z ) < 20.0f ) {
					land_pos = next;
					landed = true;
					math::vec2_t screen{};
					if ( has_prev && math::world_to_screen( next, matrix, screen, display_w, display_h ) )
						draw->AddLine(
							ImVec2{ prev_screen.x, prev_screen.y },
							ImVec2{ screen.x, screen.y },
							bounce_color, 1.8f );
					break;
				}

				vel.z = -vel.z * bounce_factor;
				vel.x *= 0.6f;
				vel.y *= 0.6f;
			}

			math::vec2_t screen{};
			if ( math::world_to_screen( next, matrix, screen, display_w, display_h ) ) {
				if ( has_prev ) {
					auto col = bounces > 0 ? bounce_color : line_color;
					draw->AddLine(
						ImVec2{ prev_screen.x, prev_screen.y },
						ImVec2{ screen.x, screen.y },
						col, 1.8f );
				}
				prev_screen = screen;
				has_prev = true;
			} else {
				has_prev = false;
			}

			pos = next;
		}

		if ( landed ) {
			math::vec2_t ls{};
			if ( math::world_to_screen( land_pos, matrix, ls, display_w, display_h ) ) {
				draw->AddCircleFilled( ImVec2{ ls.x, ls.y }, 8.0f, IM_COL32( 255, 60, 60, 100 ) );
				draw->AddCircle( ImVec2{ ls.x, ls.y }, 8.0f, end_color, 0, 2.0f );
				draw->AddLine( ImVec2{ ls.x - 5.0f, ls.y - 5.0f },
					ImVec2{ ls.x + 5.0f, ls.y + 5.0f }, end_color, 2.0f );
				draw->AddLine( ImVec2{ ls.x + 5.0f, ls.y - 5.0f },
					ImVec2{ ls.x - 5.0f, ls.y + 5.0f }, end_color, 2.0f );
			}
		} else if ( has_prev ) {
			draw->AddCircleFilled( ImVec2{ prev_screen.x, prev_screen.y }, 6.0f, end_color );
		}
	}

	struct backtrack_record_t {
		math::vec3_t origin{};
		math::vec3_t head{};
		float sim_time{};
		bool valid{};
	};

	constexpr int backtrack_max_ticks{ 16 };
	inline backtrack_record_t g_backtrack_records[64][backtrack_max_ticks]{};
	inline int g_backtrack_head[64]{};

	inline auto render_backtrack_dots( ImDrawList* draw, float display_w, float display_h ) -> void {
		if ( !g_misc.backtrack )
			return;

		auto matrix = game::read<math::view_matrix_t>( game::ctx::client + offsets::client::dwViewMatrix );
		auto& buf = g_cache.read_buffer();

		auto cur_time = 0.0f;
		auto gv = game::read<std::uintptr_t>( game::ctx::client + offsets::client::dwGlobalVars );
		if ( game::is_valid_ptr( gv ) )
			cur_time = game::read<float>( gv + 0x34 );

		for ( int i{}; i < max_cache_slots; ++i ) {
			auto& ent = buf[i];
			if ( !ent.valid || !ent.alive || !ent.is_enemy )
				continue;

			for ( int t{}; t < backtrack_max_ticks; ++t ) {
				auto& rec = g_backtrack_records[i][t];
				if ( !rec.valid )
					continue;

				auto delta = cur_time - rec.sim_time;
				if ( delta < 0.0f || delta > g_misc.backtrack_time )
					continue;

				math::vec2_t screen{};
				if ( math::world_to_screen( rec.head, matrix, screen, display_w, display_h ) ) {
					auto alpha = static_cast<int>( 255.0f * ( 1.0f - delta / g_misc.backtrack_time ) );
					draw->AddCircleFilled( ImVec2{ screen.x, screen.y }, 2.5f,
						IM_COL32( 255, 180, 50, std::clamp( alpha, 40, 200 ) ) );
				}
			}
		}
	}

	inline int g_prev_health[64]{};
	inline float g_hit_marker_alpha{};

	inline auto render_hit_marker( ImDrawList* draw, float cx, float cy ) -> void {
		if ( g_hit_marker_alpha <= 0.0f )
			return;

		auto a = static_cast<int>( g_hit_marker_alpha * 255.0f );
		auto col = IM_COL32( 255, 255, 255, a );
		constexpr float len = 8.0f;
		constexpr float gap = 3.0f;

		draw->AddLine( ImVec2{ cx - len, cy - len }, ImVec2{ cx - gap, cy - gap }, col, 2.0f );
		draw->AddLine( ImVec2{ cx + gap, cy - gap }, ImVec2{ cx + len, cy - len }, col, 2.0f );
		draw->AddLine( ImVec2{ cx - len, cy + len }, ImVec2{ cx - gap, cy + gap }, col, 2.0f );
		draw->AddLine( ImVec2{ cx + gap, cy + gap }, ImVec2{ cx + len, cy + len }, col, 2.0f );

		g_hit_marker_alpha -= 0.02f;
	}

	inline auto render_spectator_list( ImDrawList* draw ) -> void {
		if ( !g_misc.spectator_list || !game::ctx::client )
			return;

		auto entity_list_addr = game::read<std::uintptr_t>( game::ctx::client + offsets::client::dwEntityList );
		if ( !game::is_valid_ptr( entity_list_addr ) )
			return;
		game::c_entity_list entity_list{ entity_list_addr };

		auto local_pawn_addr = game::read<std::uintptr_t>( game::ctx::client + offsets::client::dwLocalPlayerPawn );
		if ( !game::is_valid_ptr( local_pawn_addr ) )
			return;

		float y = 60.0f;
		bool header_drawn{};

		for ( int i{ 1 }; i <= 64; ++i ) {
			auto controller = entity_list.get_controller( i );
			if ( !controller )
				continue;

			if ( controller.pawn_is_alive() )
				continue;

			auto pawn_handle = controller.player_pawn_handle();
			auto pawn = entity_list.get_pawn_from_handle( pawn_handle );
			if ( !pawn )
				continue;

			auto obs = pawn.observer_services();
			if ( !game::is_valid_ptr( obs ) )
				continue;

			auto target_handle = game::read<std::uint32_t>(
				obs + netvars::CPlayer_ObserverServices::m_hObserverTarget );
			auto target_addr = entity_list.resolve_handle( target_handle );

			if ( target_addr != local_pawn_addr )
				continue;

			if ( !header_drawn ) {
				draw_shadow_text( draw, 10.0f, y, IM_COL32( 255, 200, 50, 255 ), "spectators" );
				y += 16.0f;
				header_drawn = true;
			}

			auto name = controller.sanitized_name();
			draw_shadow_text( draw, 20.0f, y, IM_COL32( 255, 255, 255, 200 ),
				name ? name : "unknown" );
			y += 14.0f;
		}
	}

	inline auto render() -> void {
		if ( !g_config.enabled || !game::ctx::client )
			return;

		auto matrix = game::read<math::view_matrix_t>( game::ctx::client + offsets::client::dwViewMatrix );

		auto& io = ImGui::GetIO();
		if ( io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f )
			return;

		auto draw = ImGui::GetBackgroundDrawList();

		math::vec3_t local_origin{};
		auto local_pawn_addr = game::read<std::uintptr_t>( game::ctx::client + offsets::client::dwLocalPlayerPawn );
		if ( game::is_valid_ptr( local_pawn_addr ) ) {
			game::c_cs_player_pawn lp{ local_pawn_addr };
			local_origin = lp.origin();
		}

		for ( const auto& player : g_cache.read_buffer() ) {
			if ( !player.valid || !player.alive )
				continue;
			if ( player.dormant && !g_config.show_dormant )
				continue;
			if ( g_config.enemy_only && !player.is_enemy )
				continue;
			render_player( draw, player, matrix, io.DisplaySize.x, io.DisplaySize.y );
		}

		if ( g_aim.enabled && g_aim.fov > 0.0f ) {
			auto cx = io.DisplaySize.x * 0.5f;
			auto cy = io.DisplaySize.y * 0.5f;
			auto radius = ( g_aim.fov / 90.0f ) * ( io.DisplaySize.x * 0.5f );
			auto fov_color = g_aim.silent
				? IM_COL32( 100, 255, 100, 40 )
				: IM_COL32( 255, 255, 255, 40 );
			draw->AddCircle( ImVec2{ cx, cy }, radius, fov_color, 64, 1.0f );
		}

		render_grenade_prediction( draw, io.DisplaySize.x, io.DisplaySize.y );
		render_backtrack_dots( draw, io.DisplaySize.x, io.DisplaySize.y );
		render_hit_marker( draw, io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f );
		render_spectator_list( draw );
	}

	inline auto validate_bone( const math::vec3_t& bone, const math::vec3_t& origin, int bone_id ) -> bool {
		if ( bone.is_zero() )
			return false;

		auto dz = bone.z - origin.z;
		auto dxy = std::sqrtf(
			( bone.x - origin.x ) * ( bone.x - origin.x ) +
			( bone.y - origin.y ) * ( bone.y - origin.y ) );

		if ( dxy > 80.0f )
			return false;

		if ( bone_id == bones::head && dz < 40.0f )
			return false;
		if ( bone_id == bones::neck && dz < 35.0f )
			return false;
		if ( bone_id == bones::spine_2 && dz < 20.0f )
			return false;

		return dz < 90.0f && dz > -10.0f;
	}

	inline auto find_aim_target( const math::vec3_t& eye_pos, const math::vec2_t& view_angles ) -> math::vec3_t {
		float best_fov{ g_aim.fov };
		math::vec3_t best_target{};

		for ( const auto& player : g_cache.read_buffer() ) {
			if ( !player.valid || !player.alive || !player.is_enemy || player.dormant )
				continue;
			if ( !game::is_valid_ptr( player.pawn_addr ) )
				continue;

			game::c_cs_player_pawn pawn{ player.pawn_addr };
			if ( !pawn.is_alive() )
				continue;

			if ( g_aim.visible_only && g_local_index > 0 ) {
				if ( !pawn.is_visible_to( g_local_index ) )
					continue;
			}

			auto origin = pawn.origin();
			if ( origin.is_zero() )
				continue;

			auto target = pawn.bone_position( g_aim.bone_target );
			if ( !validate_bone( target, origin, g_aim.bone_target ) )
				continue;

			auto angle = math::calc_angle( eye_pos, target );
			auto fov = math::angle_fov( view_angles, angle );

			if ( fov < best_fov ) {
				best_fov = fov;
				best_target = target;
			}
		}

		return best_target;
	}

	inline auto compute_aim_angle( const math::vec3_t& eye_pos, const math::vec3_t& target,
		const game::c_cs_player_pawn& local_pawn ) -> math::vec2_t {

		auto angle = math::calc_angle( eye_pos, target );

		if ( g_aim.rcs ) {
			auto punch = local_pawn.aim_punch_angle();
			angle.x -= punch.x * 2.0f;
			angle.y -= punch.y * 2.0f;
		}

		angle.x = std::clamp( math::normalize_angle( angle.x ), -89.0f, 89.0f );
		angle.y = math::normalize_angle( angle.y );
		return angle;
	}

	inline auto run_aimbot() -> void {
		if ( !g_aim.enabled || !game::ctx::client )
			return;

		if ( g_aim.silent )
			return;

		if ( !( GetAsyncKeyState( g_aim.key ) & 0x8000 ) )
			return;

		auto local_pawn_addr = game::read<std::uintptr_t>( game::ctx::client + offsets::client::dwLocalPlayerPawn );
		if ( !game::is_valid_ptr( local_pawn_addr ) )
			return;

		game::c_cs_player_pawn local_pawn{ local_pawn_addr };
		if ( !local_pawn.is_alive() || local_pawn.is_holding_grenade() )
			return;

		auto eye_pos = local_pawn.eye_position();
		if ( eye_pos.is_zero() )
			return;

		auto* angle_ptr = reinterpret_cast<math::vec2_t*>( game::ctx::client + offsets::client::dwViewAngles );
		auto view_angles = *angle_ptr;
		auto target = find_aim_target( eye_pos, view_angles );
		if ( target.is_zero() )
			return;

		auto target_angle = compute_aim_angle( eye_pos, target, local_pawn );

		math::vec2_t final_angle;
		if ( g_aim.smooth > 1.0f )
			final_angle = math::smooth_angle( view_angles, target_angle, g_aim.smooth );
		else
			final_angle = target_angle;

		final_angle.x = std::clamp( final_angle.x, -89.0f, 89.0f );
		final_angle.y = math::normalize_angle( final_angle.y );

		*angle_ptr = final_angle;
	}

	inline auto store_backtrack_record( int slot, const math::vec3_t& origin,
		const math::vec3_t& head, float sim_time ) -> void {
		if ( slot < 0 || slot >= 64 ) return;

		auto& idx = g_backtrack_head[slot];
		auto& record = g_backtrack_records[slot][idx % backtrack_max_ticks];
		record.origin = origin;
		record.head = head;
		record.sim_time = sim_time;
		record.valid = true;
		++idx;
	}

	inline auto find_best_backtrack_target( const math::vec2_t& view_angles,
		const math::vec3_t& eye_pos, float max_fov, float max_time ) ->
		std::pair<math::vec3_t, float> {

		auto& buf = g_cache.read_buffer();
		float best_fov{ max_fov };
		math::vec3_t best_pos{};
		float best_sim_time{};
		bool found{};

		auto cur_time = 0.0f;
		if ( game::ctx::client ) {
			auto gv = game::read<std::uintptr_t>( game::ctx::client + offsets::client::dwGlobalVars );
			if ( game::is_valid_ptr( gv ) )
				cur_time = game::read<float>( gv + 0x34 );
		}

		if ( cur_time <= 0.0f )
			return { {}, 0.0f };

		for ( int i{}; i < max_cache_slots; ++i ) {
			auto& ent = buf[i];
			if ( !ent.valid || !ent.alive || !ent.is_enemy )
				continue;

			for ( int t{}; t < backtrack_max_ticks; ++t ) {
				auto& rec = g_backtrack_records[i][t];
				if ( !rec.valid )
					continue;

				auto delta = cur_time - rec.sim_time;
				if ( delta < 0.0f || delta > max_time )
					continue;

				auto dir = rec.head - eye_pos;
				auto dist = std::sqrtf( dir.x * dir.x + dir.y * dir.y + dir.z * dir.z );
				if ( dist < 1.0f )
					continue;

				auto pitch = -std::asinf( dir.z / dist ) * math::rad2deg;
				auto yaw = std::atan2f( dir.y, dir.x ) * math::rad2deg;

				auto dpitch = pitch - view_angles.x;
				auto dyaw = yaw - view_angles.y;
				while ( dyaw > 180.0f ) dyaw -= 360.0f;
				while ( dyaw < -180.0f ) dyaw += 360.0f;

				auto fov = std::sqrtf( dpitch * dpitch + dyaw * dyaw );
				if ( fov < best_fov ) {
					best_fov = fov;
					best_pos = rec.head;
					best_sim_time = rec.sim_time;
					found = true;
				}
			}
		}

		return found ? std::pair{ best_pos, best_sim_time } : std::pair{ math::vec3_t{}, 0.0f };
	}


	inline auto apply_anti_aim() -> void {
		if ( !g_misc.anti_aim || !game::ctx::client )
			return;

		if ( GetAsyncKeyState( VK_LBUTTON ) & 0x8000 )
			return;

		auto local_pawn_addr = game::read<std::uintptr_t>( game::ctx::client + offsets::client::dwLocalPlayerPawn );
		if ( !game::is_valid_ptr( local_pawn_addr ) )
			return;

		game::c_cs_player_pawn local_pawn{ local_pawn_addr };
		if ( !local_pawn.is_alive() || local_pawn.is_holding_grenade() )
			return;

		auto* angle_ptr = reinterpret_cast<math::vec2_t*>( game::ctx::client + offsets::client::dwViewAngles );
		auto real = *angle_ptr;

		auto fake_yaw = math::normalize_angle( real.y + 180.0f );
		cmd::write_angles( game::ctx::client, real.x, fake_yaw );
	}

	inline auto apply_no_flash( ) -> void {
		if ( !g_misc.no_flash || !game::ctx::client )
			return;

		auto local_pawn_addr = game::read<std::uintptr_t>( game::ctx::client + offsets::client::dwLocalPlayerPawn );
		if ( !game::is_valid_ptr( local_pawn_addr ) )
			return;

		game::c_cs_player_pawn local_pawn{ local_pawn_addr };
		if ( !local_pawn.valid() )
			return;

		local_pawn.set_flash_alpha( 0.0f );
		local_pawn.set_flash_duration( 0.0f );
	}

	inline auto is_smoke_entity( std::uintptr_t entity ) -> bool {
		auto identity = game::read<std::uintptr_t>( entity + 0x10 );
		if ( !game::is_valid_ptr( identity ) )
			return false;

		auto name_ptr = game::read<std::uintptr_t>( identity + 0x20 );
		if ( !game::is_valid_ptr( name_ptr ) )
			return false;

		auto name = reinterpret_cast<const char*>( name_ptr );
		return std::strncmp( name, "smokegrenade_projectile", 23 ) == 0;
	}

	inline math::vec3_t g_last_punch{};

	inline auto apply_no_recoil() -> void {
		if ( !g_misc.no_recoil || !game::ctx::client )
			return;

		auto local_pawn_addr = game::read<std::uintptr_t>( game::ctx::client + offsets::client::dwLocalPlayerPawn );
		if ( !game::is_valid_ptr( local_pawn_addr ) )
			return;

		game::c_cs_player_pawn local_pawn{ local_pawn_addr };
		if ( !local_pawn.is_alive() )
			return;

		auto punch = local_pawn.aim_punch();
		auto shots = local_pawn.shots_fired();

		if ( shots <= 0 ) {
			g_last_punch = {};
			return;
		}

		auto delta_x = ( punch.x - g_last_punch.x ) * 2.0f;
		auto delta_y = ( punch.y - g_last_punch.y ) * 2.0f;
		g_last_punch = punch;

		if ( std::fabsf( delta_x ) < 0.001f && std::fabsf( delta_y ) < 0.001f )
			return;

		auto* angle_ptr = reinterpret_cast<math::vec2_t*>( game::ctx::client + offsets::client::dwViewAngles );
		angle_ptr->x -= delta_x;
		angle_ptr->y -= delta_y;

		if ( angle_ptr->x > 89.0f ) angle_ptr->x = 89.0f;
		if ( angle_ptr->x < -89.0f ) angle_ptr->x = -89.0f;
	}

	inline auto apply_rapid_fire() -> void {
		if ( !g_misc.rapid_fire || !game::ctx::client )
			return;

		auto local_pawn_addr = game::read<std::uintptr_t>( game::ctx::client + offsets::client::dwLocalPlayerPawn );
		if ( !game::is_valid_ptr( local_pawn_addr ) )
			return;

		game::c_cs_player_pawn local_pawn{ local_pawn_addr };
		if ( !local_pawn.is_alive() || local_pawn.is_holding_grenade() )
			return;

		auto weapon_addr = local_pawn.clipping_weapon();
		if ( !game::is_valid_ptr( weapon_addr ) )
			return;

		game::c_base_weapon weapon{ weapon_addr };
		weapon.set_next_primary_attack_tick( 0 );
	}

	inline auto apply_no_smoke() -> void {
		if ( !g_misc.no_smoke || !game::ctx::client )
			return;

		auto entity_list_addr = game::read<std::uintptr_t>( game::ctx::client + offsets::client::dwEntityList );
		if ( !game::is_valid_ptr( entity_list_addr ) )
			return;

		for ( int i{ 65 }; i < 1024; ++i ) {
			auto chunk = game::read<std::uintptr_t>( entity_list_addr + 0x8 * ( ( i & 0x7FFF ) >> 9 ) + 0x10 );
			if ( !game::is_valid_ptr( chunk ) )
				continue;

			auto entity = game::read<std::uintptr_t>( chunk + game::IDENTITY_STRIDE * ( i & 0x1FF ) );
			if ( !game::is_valid_ptr( entity ) )
				continue;

			if ( !is_smoke_entity( entity ) )
				continue;

			auto tick_begin = game::read<int>( entity + netvars::C_SmokeGrenadeProjectile::m_nSmokeEffectTickBegin );
			if ( tick_begin > 0 )
				*reinterpret_cast<int*>( entity + netvars::C_SmokeGrenadeProjectile::m_nSmokeEffectTickBegin ) = 0;
		}
	}



	inline auto update_internal() -> void {
		if ( !game::ctx::client )
			return;

		auto entity_list_addr = game::read<std::uintptr_t>( game::ctx::client + offsets::client::dwEntityList );
		if ( !game::is_valid_ptr( entity_list_addr ) )
			return;

		game::c_entity_list entity_list{ entity_list_addr };

		auto local_controller_addr = game::read<std::uintptr_t>( game::ctx::client + offsets::client::dwLocalPlayerController );
		game::c_cs_player_controller local_controller{ local_controller_addr };
		auto local_team = local_controller.valid() ? local_controller.team_num() : 0;

		g_local_index = 0;
		for ( int i{ 1 }; i <= 64; ++i ) {
			auto ctrl = entity_list.get_controller( i );
			if ( ctrl && ctrl.address() == local_controller_addr ) {
				g_local_index = i;
				break;
			}
		}

		auto& prev_buf = g_cache.read_buffer();
		g_cache.clear_write_buffer();
		auto& write_buf = g_cache.write_buffer();

		for ( int i{ 1 }; i <= 64; ++i ) {
			auto controller = entity_list.get_controller( i );
			if ( !controller || controller.address() == local_controller_addr )
				continue;

			auto ctrl_team = controller.team_num();
			bool is_enemy = ( ctrl_team != local_team );

			auto pawn_handle = controller.player_pawn_handle();
			auto pawn = entity_list.get_pawn_from_handle( pawn_handle );

			bool pawn_valid = pawn && pawn.is_alive();
			auto pos = pawn_valid ? pawn.origin() : math::vec3_t{};
			bool has_pos = pawn_valid && !pos.is_zero();

			if ( has_pos ) {
				auto& entry = write_buf[i - 1];
				entry.valid = true;
				entry.alive = true;
				entry.dormant = false;
				entry.index = i;
				entry.pawn_addr = pawn.address();
				entry.health = pawn.health();
				entry.armor = pawn.armor_value();
				entry.team = ctrl_team;
				entry.is_enemy = is_enemy;
				entry.origin = pos;
				entry.head_pos = pawn.head_position();

				if ( is_enemy && g_misc.hit_sound ) {
					auto prev = g_prev_health[i - 1];
					if ( prev > 0 && entry.health < prev && entry.health > 0 )
						g_hit_marker_alpha = 1.0f;
					g_prev_health[i - 1] = entry.health;
				}

				for ( int b{}; b < 28; ++b )
					entry.bones[b] = pawn.bone_position( b );

				auto name = controller.sanitized_name();
				if ( name )
					std::strncpy( entry.name, name, sizeof( entry.name ) - 1 );

				auto weapon_idx = pawn.active_weapon_def_index();
				auto wname = weapon_name_from_index( weapon_idx );
				if ( wname )
					std::strncpy( entry.weapon, wname, sizeof( entry.weapon ) - 1 );

				if ( is_enemy ) {
					if ( g_config.radar )
						pawn.set_spotted( true );
					if ( g_misc.backtrack ) {
						auto sim = pawn.simulation_time();
						if ( sim > 0.0f )
							store_backtrack_record( i - 1, pos, entry.head_pos, sim );
					}
				}
			} else if ( controller.pawn_is_alive() ) {
				auto& prev = prev_buf[i - 1];
				if ( prev.valid && !prev.origin.is_zero() ) {
					write_buf[i - 1] = prev;
					write_buf[i - 1].dormant = true;
					write_buf[i - 1].health = controller.pawn_health();
				} else {
					auto& entry = write_buf[i - 1];
					entry.valid = true;
					entry.alive = true;
					entry.dormant = true;
					entry.index = i;
					entry.health = controller.pawn_health();
					entry.team = ctrl_team;
					entry.is_enemy = is_enemy;

					auto name = controller.sanitized_name();
					if ( name )
						std::strncpy( entry.name, name, sizeof( entry.name ) - 1 );
				}
			}
		}

		g_cache.swap();
	}

	inline auto update() -> void {
		__try {
			update_internal();
			apply_no_flash();
			apply_no_smoke();
			apply_no_recoil();
			apply_rapid_fire();
			apply_anti_aim();
		} __except ( EXCEPTION_EXECUTE_HANDLER ) {}
	}

}
