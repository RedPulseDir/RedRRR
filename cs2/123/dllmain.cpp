#include <Windows.h>
#include <cstdio>

#include "workspace/engine/sdk/scanner.hxx"
#include "workspace/engine/hooks.hxx"
#include "workspace/render/present.hxx"

auto WINAPI kompy_init( LPVOID module ) -> DWORD {
	AllocConsole();
	SetConsoleTitleA( "kompy" );

	FILE* console_stream{ nullptr };
	freopen_s( &console_stream, "CONOUT$", "w", stdout );

	std::printf( "[kompy] initializing...\n" );

	kompy::game::ctx::initialize();
	kompy::scanner::init_offsets( kompy::game::ctx::client );

	if ( kompy::render::install() ) {
		std::printf( "[kompy] initialization successful\n" );
	} else {
		std::printf( "[kompy] initialization failed\n" );
		return 5;
	}

	kompy::hooks::input_history::install();

	while ( !( GetAsyncKeyState( VK_END ) & 0x8000 ) )
		Sleep( 50 );

	std::printf( "[kompy] unloading...\n" );

	kompy::hooks::input_history::remove();
	kompy::render::remove();

	Sleep( 200 );

	if ( console_stream ) std::fclose( console_stream );
	FreeConsole();
	return 0;
}

auto APIENTRY DllMain( HMODULE module, DWORD reason, LPVOID reserved ) -> BOOL {
	if ( reason == DLL_PROCESS_ATTACH ) {
		DisableThreadLibraryCalls( module );
		auto thread = CreateThread( nullptr, 0, kompy_init, module, 0, nullptr );
		if ( thread )
			CloseHandle( thread );
	}
	return TRUE;
}
