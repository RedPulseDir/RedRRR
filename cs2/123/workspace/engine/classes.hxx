#pragma once

#include <cstdint>
#include <Windows.h>

#include "math.hxx"
#include "sdk/offsets.hxx"
#include "sdk/netvars.hxx"

namespace kompy::game {

	constexpr std::uintptr_t IDENTITY_STRIDE = 0x70;

	inline auto is_valid_ptr( std::uintptr_t addr ) -> bool {
		return addr > 0x10000 && addr < 0x7FFFFFFFFFFF;
	}

	template <typename T>
	inline auto read( std::uintptr_t addr ) -> T {
		if ( !is_valid_ptr( addr ) ) return T{};
		return *reinterpret_cast<T*>( addr );
	}

	inline auto module_base( const wchar_t* name ) -> std::uintptr_t {
		return reinterpret_cast<std::uintptr_t>( GetModuleHandleW( name ) );
	}

	namespace ctx {
		inline std::uintptr_t client{};
		inline std::uintptr_t engine{};

		inline auto initialize() -> void {
			client = module_base( L"client.dll" );
			engine = module_base( L"engine2.dll" );
		}
	}

	class c_entity_instance {
	protected:
		std::uintptr_t ptr_{};

	public:
		c_entity_instance() = default;
		explicit c_entity_instance( std::uintptr_t p ) : ptr_{ p } {}

		auto address() const -> std::uintptr_t { return ptr_; }
		auto valid() const -> bool { return is_valid_ptr( ptr_ ); }
		explicit operator bool() const { return valid(); }
	};

	class c_base_entity : public c_entity_instance {
	public:
		using c_entity_instance::c_entity_instance;

		auto health() const -> int {
			return read<int>( ptr_ + netvars::C_BaseEntity::m_iHealth );
		}

		auto life_state() const -> std::uint8_t {
			return read<std::uint8_t>( ptr_ + netvars::C_BaseEntity::m_lifeState );
		}

		auto is_alive() const -> bool {
			return life_state() == 0 && health() > 0;
		}

		auto team_num() const -> std::uint8_t {
			return read<std::uint8_t>( ptr_ + netvars::C_BaseEntity::m_iTeamNum );
		}

		auto flags() const -> std::uint32_t {
			return read<std::uint32_t>( ptr_ + netvars::C_BaseEntity::m_fFlags );
		}

		auto game_scene_node() const -> std::uintptr_t {
			return read<std::uintptr_t>( ptr_ + netvars::C_BaseEntity::m_pGameSceneNode );
		}

		auto origin() const -> math::vec3_t {
			auto node = game_scene_node();
			if ( !is_valid_ptr( node ) ) return {};
			return read<math::vec3_t>( node + netvars::CGameSceneNode::m_vecAbsOrigin );
		}

		auto move_type() const -> std::uint8_t {
			return read<std::uint8_t>( ptr_ + netvars::C_BaseEntity::m_MoveType );
		}

		auto set_move_type( std::uint8_t type ) const -> void {
			if ( !valid() ) return;
			*reinterpret_cast<std::uint8_t*>( ptr_ + netvars::C_BaseEntity::m_MoveType ) = type;
		}

		auto simulation_time() const -> float {
			return read<float>( ptr_ + netvars::C_BaseEntity::m_flSimulationTime );
		}

		auto old_simulation_time() const -> float {
			return read<float>( ptr_ + netvars::C_BaseEntity::m_flOldSimulationTime );
		}
	};

	class c_base_player_pawn : public c_base_entity {
	public:
		using c_base_entity::c_base_entity;

		auto old_origin() const -> math::vec3_t {
			return read<math::vec3_t>( ptr_ + netvars::C_BasePlayerPawn::m_vOldOrigin );
		}

		auto view_offset() const -> math::vec3_t {
			return read<math::vec3_t>( ptr_ + netvars::C_BaseModelEntity::m_vecViewOffset );
		}

		auto set_view_offset( const math::vec3_t& v ) const -> void {
			if ( !valid() ) return;
			*reinterpret_cast<math::vec3_t*>( ptr_ + netvars::C_BaseModelEntity::m_vecViewOffset ) = v;
		}

		auto eye_position() const -> math::vec3_t {
			return origin() + view_offset();
		}

		auto weapon_services() const -> std::uintptr_t {
			return read<std::uintptr_t>( ptr_ + netvars::C_BasePlayerPawn::m_pWeaponServices );
		}
	};

	class c_cs_player_pawn : public c_base_player_pawn {
	public:
		using c_base_player_pawn::c_base_player_pawn;

		auto shots_fired() const -> int {
			return read<int>( ptr_ + netvars::C_CSPlayerPawnBase::m_iShotsFired );
		}

		auto aim_punch_angle() const -> math::vec2_t {
			auto v = read<math::vec3_t>( ptr_ + netvars::C_CSPlayerPawnBase::m_aimPunchAngle );
			return { v.x, v.y };
		}

		auto is_scoped() const -> bool {
			return read<bool>( ptr_ + netvars::C_CSPlayerPawnBase::m_bIsScoped );
		}

		auto is_crouching() const -> bool {
			return ( flags() & ( 1 << 1 ) ) != 0;
		}

		auto is_on_ground() const -> bool {
			return ( flags() & 1 ) != 0;
		}

		auto head_position() const -> math::vec3_t {
			auto pos = origin();
			pos.z += is_crouching() ? 54.0f : 72.0f;
			return pos;
		}

		auto bone_position( int bone_id ) const -> math::vec3_t {
			auto node = game_scene_node();
			if ( !is_valid_ptr( node ) ) return {};
			auto bone_data = read<std::uintptr_t>( node + 0x160 + 0x80 );
			if ( !is_valid_ptr( bone_data ) ) return {};
			return read<math::vec3_t>( bone_data + bone_id * 0x20 );
		}

		auto clipping_weapon() const -> std::uintptr_t {
			return read<std::uintptr_t>( ptr_ + 0x3DC0 );
		}

		auto active_weapon_def_index() const -> std::uint16_t {
			auto weapon = clipping_weapon();
			if ( !is_valid_ptr( weapon ) ) return 0;
			return read<std::uint16_t>( weapon + 0x1378 + 0x50 + 0x1BA );
		}

		auto is_holding_grenade() const -> bool {
			auto idx = active_weapon_def_index();
			return ( idx >= 43 && idx <= 48 );
		}

		auto grenade_pin_pulled() const -> bool {
			auto weapon = clipping_weapon();
			if ( !is_valid_ptr( weapon ) ) return false;
			return read<bool>( weapon + netvars::C_BaseCSGrenade::m_bPinPulled );
		}

		auto grenade_throw_strength() const -> float {
			auto weapon = clipping_weapon();
			if ( !is_valid_ptr( weapon ) ) return 0.0f;
			return read<float>( weapon + netvars::C_BaseCSGrenade::m_flThrowStrength );
		}

		auto grenade_throw_velocity() const -> float {
			auto idx = active_weapon_def_index();
			if ( idx == 46 || idx == 48 ) return 700.0f;
			return 750.0f;
		}

		auto set_flash_alpha( float alpha ) const -> void {
			if ( !valid() ) return;
			*reinterpret_cast<float*>( ptr_ + netvars::C_CSPlayerPawnBase::m_flFlashMaxAlpha ) = alpha;
		}

		auto set_flash_duration( float duration ) const -> void {
			if ( !valid() ) return;
			*reinterpret_cast<float*>( ptr_ + netvars::C_CSPlayerPawnBase::m_flFlashDuration ) = duration;
		}

		auto set_glow( int type, int r, int g, int b, int a, int range ) const -> void {
			if ( !valid() ) return;
			auto glow = ptr_ + netvars::C_BaseModelEntity::m_Glow;
			*reinterpret_cast<int*>( glow + netvars::CGlowProperty::m_iGlowType ) = type;
			*reinterpret_cast<int*>( glow + netvars::CGlowProperty::m_nGlowRange ) = range;
			*reinterpret_cast<int*>( glow + netvars::CGlowProperty::m_nGlowRangeMin ) = 0;
			auto color_addr = glow + netvars::CGlowProperty::m_glowColorOverride;
			*reinterpret_cast<std::uint8_t*>( color_addr + 0 ) = static_cast<std::uint8_t>( r );
			*reinterpret_cast<std::uint8_t*>( color_addr + 1 ) = static_cast<std::uint8_t>( g );
			*reinterpret_cast<std::uint8_t*>( color_addr + 2 ) = static_cast<std::uint8_t>( b );
			*reinterpret_cast<std::uint8_t*>( color_addr + 3 ) = static_cast<std::uint8_t>( a );
			*reinterpret_cast<float*>( glow + netvars::CGlowProperty::m_flGlowTime ) = 99999.0f;
			*reinterpret_cast<float*>( glow + netvars::CGlowProperty::m_flGlowStartTime ) = -1.0f;
			*reinterpret_cast<bool*>( glow + netvars::CGlowProperty::m_bEligibleForScreenHighlight ) = true;
		}

		auto set_spotted( bool spotted ) const -> void {
			if ( !valid() ) return;
			*reinterpret_cast<bool*>( ptr_ + netvars::C_CSPlayerPawnBase::m_entitySpottedState +
				netvars::EntitySpottedState_t::m_bSpotted ) = spotted;
		}

		auto spotted_by_mask() const -> std::uint64_t {
			auto base = ptr_ + netvars::C_CSPlayerPawnBase::m_entitySpottedState +
				netvars::EntitySpottedState_t::m_bSpottedByMask;
			auto lo = read<std::uint32_t>( base );
			auto hi = read<std::uint32_t>( base + 4 );
			return static_cast<std::uint64_t>( hi ) << 32 | lo;
		}

		auto is_visible_to( int local_index ) const -> bool {
			if ( local_index <= 0 || local_index > 64 ) return false;
			auto mask = spotted_by_mask();
			return ( mask & ( 1ULL << ( local_index - 1 ) ) ) != 0;
		}

		auto observer_services() const -> std::uintptr_t {
			return read<std::uintptr_t>( ptr_ + netvars::C_BasePlayerPawn::m_pObserverServices );
		}

		auto camera_services() const -> std::uintptr_t {
			return read<std::uintptr_t>( ptr_ + netvars::C_BasePlayerPawn::m_pCameraServices );
		}

		auto get_fov() const -> std::uint32_t {
			auto cam = camera_services();
			if ( !is_valid_ptr( cam ) ) return 0;
			return read<std::uint32_t>( cam + netvars::CCSPlayerBase_CameraServices::m_iFOV );
		}

		auto set_fov( std::uint32_t fov ) const -> void {
			auto cam = camera_services();
			if ( !is_valid_ptr( cam ) ) return;
			*reinterpret_cast<std::uint32_t*>( cam + netvars::CCSPlayerBase_CameraServices::m_iFOV ) = fov;
			*reinterpret_cast<std::uint32_t*>( cam + netvars::CCSPlayerBase_CameraServices::m_iFOVStart ) = fov;
			*reinterpret_cast<float*>( cam + netvars::CCSPlayerBase_CameraServices::m_flFOVRate ) = 0.0f;
		}

		auto armor_value() const -> int {
			return read<int>( ptr_ + netvars::C_CSPlayerPawnBase::m_ArmorValue );
		}

		auto aim_punch() const -> math::vec3_t {
			return read<math::vec3_t>( ptr_ + netvars::C_CSPlayerPawnBase::m_aimPunchAngle );
		}

		auto zero_aim_punch() const -> void {
			if ( !valid() ) return;
			*reinterpret_cast<float*>( ptr_ + netvars::C_CSPlayerPawnBase::m_aimPunchAngle ) = 0.0f;
			*reinterpret_cast<float*>( ptr_ + netvars::C_CSPlayerPawnBase::m_aimPunchAngle + 4 ) = 0.0f;
			*reinterpret_cast<float*>( ptr_ + netvars::C_CSPlayerPawnBase::m_aimPunchAngle + 8 ) = 0.0f;
		}

		auto is_left_handed() const -> bool {
			return read<bool>( ptr_ + netvars::C_CSPlayerPawn::m_bLeftHanded );
		}

		auto set_left_handed( bool left ) const -> void {
			if ( !valid() ) return;
			*reinterpret_cast<bool*>( ptr_ + netvars::C_CSPlayerPawn::m_bLeftHanded ) = left;
		}

		auto viewmodel_fov() const -> float {
			return read<float>( ptr_ + netvars::C_CSPlayerPawn::m_flViewmodelFOV );
		}

		auto set_viewmodel_fov( float fov ) const -> void {
			if ( !valid() ) return;
			*reinterpret_cast<float*>( ptr_ + netvars::C_CSPlayerPawn::m_flViewmodelFOV ) = fov;
		}

		auto active_weapon_handle() const -> std::uint32_t {
			auto ws = weapon_services();
			if ( !is_valid_ptr( ws ) ) return 0;
			return read<std::uint32_t>( ws + netvars::CPlayer_WeaponServices::m_hActiveWeapon );
		}

		auto my_weapons_data() const -> std::uintptr_t {
			auto ws = weapon_services();
			if ( !is_valid_ptr( ws ) ) return 0;
			return read<std::uintptr_t>( ws + netvars::CPlayer_WeaponServices::m_hMyWeapons );
		}

		auto my_weapons_count() const -> int {
			auto ws = weapon_services();
			if ( !is_valid_ptr( ws ) ) return 0;
			auto count = read<int>( ws + netvars::CPlayer_WeaponServices::m_hMyWeapons + 0x10 );
			return ( count > 0 && count < 64 ) ? count : 0;
		}
	};

	class c_base_weapon : public c_entity_instance {
	public:
		using c_entity_instance::c_entity_instance;

		auto econ_item_view() const -> std::uintptr_t {
			return ptr_ + netvars::C_EconEntity::m_AttributeManager +
				netvars::CAttributeContainer::m_Item;
		}

		auto def_index() const -> std::uint16_t {
			return read<std::uint16_t>( econ_item_view() + netvars::C_EconItemView::m_iItemDefinitionIndex );
		}

		auto item_id_high() const -> std::uint32_t {
			return read<std::uint32_t>( econ_item_view() + netvars::C_EconItemView::m_iItemIDHigh );
		}

		auto set_item_id_high( std::uint32_t val ) const -> void {
			if ( !valid() ) return;
			*reinterpret_cast<std::uint32_t*>( econ_item_view() + netvars::C_EconItemView::m_iItemIDHigh ) = val;
		}

		auto set_item_id_low( std::uint32_t val ) const -> void {
			if ( !valid() ) return;
			*reinterpret_cast<std::uint32_t*>( econ_item_view() + netvars::C_EconItemView::m_iItemIDLow ) = val;
		}

		auto set_account_id( std::uint32_t val ) const -> void {
			if ( !valid() ) return;
			*reinterpret_cast<std::uint32_t*>( econ_item_view() + netvars::C_EconItemView::m_iAccountID ) = val;
		}

		auto set_entity_quality( int val ) const -> void {
			if ( !valid() ) return;
			*reinterpret_cast<int*>( econ_item_view() + netvars::C_EconItemView::m_iEntityQuality ) = val;
		}

		auto set_fallback_paint_kit( int kit ) const -> void {
			if ( !valid() ) return;
			*reinterpret_cast<int*>( ptr_ + netvars::C_EconEntity::m_nFallbackPaintKit ) = kit;
		}

		auto set_fallback_seed( int seed ) const -> void {
			if ( !valid() ) return;
			*reinterpret_cast<int*>( ptr_ + netvars::C_EconEntity::m_nFallbackSeed ) = seed;
		}

		auto set_fallback_wear( float wear ) const -> void {
			if ( !valid() ) return;
			*reinterpret_cast<float*>( ptr_ + netvars::C_EconEntity::m_flFallbackWear ) = wear;
		}

		auto set_fallback_stat_trak( int val ) const -> void {
			if ( !valid() ) return;
			*reinterpret_cast<int*>( ptr_ + netvars::C_EconEntity::m_nFallbackStatTrak ) = val;
		}

		auto set_initialized( bool val ) const -> void {
			if ( !valid() ) return;
			*reinterpret_cast<bool*>( econ_item_view() + netvars::C_EconItemView::m_bInitialized ) = val;
		}

		auto is_initialized() const -> bool {
			return read<bool>( econ_item_view() + netvars::C_EconItemView::m_bInitialized );
		}

		auto attribute_manager() const -> std::uintptr_t {
			return ptr_ + netvars::C_EconEntity::m_AttributeManager;
		}

		auto bump_reapply_parity() const -> void {
			if ( !valid() ) return;
			auto addr = attribute_manager() + netvars::CAttributeManager::m_iReapplyProvisionParity;
			auto current = read<int>( addr );
			*reinterpret_cast<int*>( addr ) = current + 1;
		}

		auto next_primary_attack_tick() const -> int {
			return read<int>( ptr_ + netvars::C_BasePlayerWeapon::m_nNextPrimaryAttackTick );
		}

		auto set_next_primary_attack_tick( int tick ) const -> void {
			if ( !valid() ) return;
			*reinterpret_cast<int*>( ptr_ + netvars::C_BasePlayerWeapon::m_nNextPrimaryAttackTick ) = tick;
		}

		auto clip1() const -> int {
			return read<int>( ptr_ + netvars::C_BasePlayerWeapon::m_iClip1 );
		}
	};

	class c_base_player_controller : public c_base_entity {
	public:
		using c_base_entity::c_base_entity;

		auto pawn_handle() const -> std::uint32_t {
			return read<std::uint32_t>( ptr_ + netvars::CBasePlayerController::m_hPawn );
		}
	};

	class c_cs_player_controller : public c_base_player_controller {
	public:
		using c_base_player_controller::c_base_player_controller;

		auto player_pawn_handle() const -> std::uint32_t {
			return read<std::uint32_t>( ptr_ + netvars::CCSPlayerController::m_hPlayerPawn );
		}

		auto sanitized_name() const -> const char* {
			auto name_ptr = read<std::uintptr_t>( ptr_ + netvars::CCSPlayerController::m_sSanitizedPlayerName );
			if ( !is_valid_ptr( name_ptr ) ) return nullptr;
			return reinterpret_cast<const char*>( name_ptr );
		}

		auto is_local() const -> bool {
			return read<bool>( ptr_ + netvars::CCSPlayerController::m_bIsLocalPlayerController );
		}

		auto pawn_is_alive() const -> bool {
			return read<bool>( ptr_ + netvars::CCSPlayerController::m_bPawnIsAlive );
		}

		auto pawn_health() const -> int {
			return read<int>( ptr_ + netvars::CCSPlayerController::m_iPawnHealth );
		}

		auto pawn_armor() const -> int {
			return read<int>( ptr_ + netvars::CCSPlayerController::m_iPawnArmor );
		}
	};

	class c_entity_list {
		std::uintptr_t base_{};

	public:
		c_entity_list() = default;
		explicit c_entity_list( std::uintptr_t b ) : base_{ b } {}

		auto valid() const -> bool { return base_ != 0; }

		auto get_controller( int index ) const -> c_cs_player_controller {
			auto chunk = read<std::uintptr_t>( base_ + 0x8 * ( ( index & 0x7FFF ) >> 9 ) + 0x10 );
			if ( !is_valid_ptr( chunk ) ) return {};
			auto entity = read<std::uintptr_t>( chunk + IDENTITY_STRIDE * ( index & 0x1FF ) );
			if ( !is_valid_ptr( entity ) ) return {};
			return c_cs_player_controller{ entity };
		}

		auto get_pawn_from_handle( std::uint32_t handle ) const -> c_cs_player_pawn {
			if ( !handle || handle == 0xFFFFFFFF ) return {};
			auto chunk = read<std::uintptr_t>( base_ + 0x8 * ( ( handle & 0x7FFF ) >> 9 ) + 0x10 );
			if ( !is_valid_ptr( chunk ) ) return {};
			auto entity = read<std::uintptr_t>( chunk + IDENTITY_STRIDE * ( handle & 0x1FF ) );
			if ( !is_valid_ptr( entity ) ) return {};
			return c_cs_player_pawn{ entity };
		}

		auto resolve_handle( std::uint32_t handle ) const -> std::uintptr_t {
			if ( !handle || handle == 0xFFFFFFFF ) return 0;
			auto chunk = read<std::uintptr_t>( base_ + 0x8 * ( ( handle & 0x7FFF ) >> 9 ) + 0x10 );
			if ( !is_valid_ptr( chunk ) ) return 0;
			return read<std::uintptr_t>( chunk + IDENTITY_STRIDE * ( handle & 0x1FF ) );
		}
	};

}
