#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>

#include "../../engine/math.hxx"

namespace kompy::visuals {

	inline constexpr int max_cache_slots{ 64 };

	struct cached_player_t {
		bool valid{};
		bool alive{};
		bool is_enemy{};
		bool dormant{};

		int health{};
		int team{};
		int index{};
		int armor{};

		std::uintptr_t pawn_addr{};

		math::vec3_t origin{};
		math::vec3_t head_pos{};
		math::vec3_t bones[28]{};

		char name[128]{};
		char weapon[64]{};
	};

	class entity_cache {
		std::array<cached_player_t, max_cache_slots> buffers_[2]{};
		std::atomic<int> read_idx_{ 0 };

	public:
		auto read_buffer() const -> const std::array<cached_player_t, max_cache_slots>& {
			return buffers_[read_idx_.load( std::memory_order_acquire )];
		}

		auto write_buffer() -> std::array<cached_player_t, max_cache_slots>& {
			return buffers_[1 - read_idx_.load( std::memory_order_acquire )];
		}

		auto swap() -> void {
			read_idx_.store( 1 - read_idx_.load( std::memory_order_acquire ), std::memory_order_release );
		}

		auto clear_write_buffer() -> void {
			auto& buf = write_buffer();
			for ( auto& p : buf )
				p = cached_player_t{};
		}
	};

	inline entity_cache g_cache{};
	inline int g_local_index{};

}
