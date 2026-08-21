#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <Windows.h>

#include "offsets.hxx"

namespace kompy::scanner {

	inline auto hex_char( char c ) -> int {
		if ( c >= '0' && c <= '9' ) return c - '0';
		if ( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
		if ( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
		return 0;
	}

	inline auto pattern_to_bytes( const char* pattern, std::uint8_t* out, bool* mask, int& count ) -> void {
		count = 0;
		for ( auto p = pattern; *p; ) {
			while ( *p == ' ' ) ++p;
			if ( !*p ) break;
			if ( *p == '?' ) {
				out[count] = 0;
				mask[count] = false;
				++count;
				++p;
				if ( *p == '?' ) ++p;
			} else {
				out[count] = static_cast<std::uint8_t>( ( hex_char( *p ) << 4 ) | hex_char( *( p + 1 ) ) );
				mask[count] = true;
				++count;
				p += 2;
			}
		}
	}

	inline auto find_pattern( std::uintptr_t base, std::size_t size, const char* pattern ) -> std::uintptr_t {
		std::uint8_t sig[256]{};
		bool sig_mask[256]{};
		int sig_len{};
		pattern_to_bytes( pattern, sig, sig_mask, sig_len );

		if ( sig_len <= 0 || sig_len > 256 )
			return 0;

		auto data = reinterpret_cast<const std::uint8_t*>( base );

		for ( std::size_t i{}; i < size - sig_len; ++i ) {
			bool found{ true };
			for ( int j{}; j < sig_len; ++j ) {
				if ( sig_mask[j] && data[i + j] != sig[j] ) {
					found = false;
					break;
				}
			}
			if ( found )
				return base + i;
		}

		return 0;
	}

	inline auto resolve_rip( std::uintptr_t addr, int disp_offset, int instr_size ) -> std::uintptr_t {
		auto disp = *reinterpret_cast<std::int32_t*>( addr + disp_offset );
		return addr + instr_size + disp;
	}

	inline auto module_size( std::uintptr_t base ) -> std::size_t {
		auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>( base );
		auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>( base + dos->e_lfanew );
		return nt->OptionalHeader.SizeOfImage;
	}

	inline auto init_offsets( std::uintptr_t client ) -> void {
		if ( !client ) {
			std::printf( "[kompy] client.dll's loaded module just doesnt exist GG\n" );
			return;
		}

		auto size = module_size( client );

		auto view_matrix = find_pattern( client, size, "48 63 C1 48 8D 0D ? ? ? ? 48 C1 E0 06 48 03 C1 C3" );
		if ( view_matrix ) {
			offsets::client::dwViewMatrix = static_cast<std::ptrdiff_t>( resolve_rip( view_matrix, 6, 10 ) - client );
			std::printf( "[kompy] dwViewMatrix       = 0x%llX\n", static_cast<unsigned long long>( offsets::client::dwViewMatrix ) );
		}

		auto entity_list = find_pattern( client, size, "B8 FF FF FF 7F 89 45 B3 48 8B 05 ? ? ? ? 48 89 BC 24 F0 00 00 00" );
		if ( entity_list ) {
			offsets::client::dwEntityList = static_cast<std::ptrdiff_t>(resolve_rip( entity_list, 11, 15 ) - client );
			std::printf( "[kompy] dwEntityList       = 0x%llX\n", 	static_cast<unsigned long long>( offsets::client::dwEntityList ) );
		}

		auto local_controller = find_pattern( client, size, "48 8D 0D ? ? ? ? 33 F6 4C 89 34 F9" );
		if ( local_controller ) {
			offsets::client::dwLocalPlayerController = static_cast<std::ptrdiff_t>( resolve_rip( local_controller, 3, 7 ) - client );
			std::printf( "[kompy] dwLocalPlayerController = 0x%llX\n", static_cast<unsigned long long>( offsets::client::dwLocalPlayerController ) );
		}

		std::printf( "[kompy] offset scan complete\n" );
	}

}
