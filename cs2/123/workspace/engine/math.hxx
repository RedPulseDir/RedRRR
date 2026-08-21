#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace kompy::math {

	inline constexpr float pi = 3.14159265358979323846f;
	inline constexpr float rad2deg = 180.0f / pi;
	inline constexpr float deg2rad = pi / 180.0f;

	struct vec2_t {
		float x{};
		float y{};

		auto operator+( const vec2_t& o ) const -> vec2_t { return { x + o.x, y + o.y }; }
		auto operator-( const vec2_t& o ) const -> vec2_t { return { x - o.x, y - o.y }; }
		auto operator*( float s ) const -> vec2_t { return { x * s, y * s }; }
		auto length() const -> float { return std::sqrtf( x * x + y * y ); }
	};

	struct vec3_t {
		float x{};
		float y{};
		float z{};

		auto operator+( const vec3_t& o ) const -> vec3_t { return { x + o.x, y + o.y, z + o.z }; }
		auto operator-( const vec3_t& o ) const -> vec3_t { return { x - o.x, y - o.y, z - o.z }; }
		auto operator*( float s ) const -> vec3_t { return { x * s, y * s, z * s }; }
		auto length() const -> float { return std::sqrtf( x * x + y * y + z * z ); }
		auto length_2d() const -> float { return std::sqrtf( x * x + y * y ); }
		auto is_zero() const -> bool { return x == 0.f && y == 0.f && z == 0.f; }
	};

	inline auto normalize_angle( float a ) -> float {
		while ( a > 180.0f ) a -= 360.0f;
		while ( a < -180.0f ) a += 360.0f;
		return a;
	}

	inline auto calc_angle( const vec3_t& src, const vec3_t& dst ) -> vec2_t {
		auto delta = dst - src;
		auto hyp = delta.length_2d();
		return {
			normalize_angle( std::atan2f( -delta.z, hyp ) * rad2deg ),
			normalize_angle( std::atan2f( delta.y, delta.x ) * rad2deg )
		};
	}

	inline auto angle_fov( const vec2_t& view, const vec2_t& target ) -> float {
		auto dx = normalize_angle( target.x - view.x );
		auto dy = normalize_angle( target.y - view.y );
		return std::sqrtf( dx * dx + dy * dy );
	}

	inline auto smooth_angle( const vec2_t& current, const vec2_t& target, float factor ) -> vec2_t {
		return {
			current.x + normalize_angle( target.x - current.x ) / factor,
			current.y + normalize_angle( target.y - current.y ) / factor
		};
	}

	struct view_matrix_t {
		float data[16]{};

		auto operator[]( int i ) const -> float { return data[i]; }
	};

	inline auto world_to_screen( const vec3_t& world, const view_matrix_t& matrix, vec2_t& screen, float display_w, float display_h ) -> bool {
		auto w = matrix[12] * world.x + matrix[13] * world.y + matrix[14] * world.z + matrix[15];

		if ( w < 0.001f )
			return false;

		auto x = matrix[0] * world.x + matrix[1] * world.y + matrix[2] * world.z + matrix[3];
		auto y = matrix[4] * world.x + matrix[5] * world.y + matrix[6] * world.z + matrix[7];

		screen.x = ( display_w * 0.5f ) + ( x / w ) * ( display_w * 0.5f );
		screen.y = ( display_h * 0.5f ) - ( y / w ) * ( display_h * 0.5f );

		return true;
	}

}
