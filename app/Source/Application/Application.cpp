#include "Stdafx.hpp"
#include "Application/Application.hpp"
#include "Searcher/Searcher.hpp"

Application::Application( )
{
	register_service( std::make_shared<Searcher>( ) );
	LOG( "Application initialized" );
}

int Application::run( ) const
{
	try
	{
		auto searcher = get_service<Searcher>( );

		constexpr char target_drive = 'C';

		LOG( "Starting scan on Drive {}...", target_drive );

		auto start_time = std::chrono::high_resolution_clock::now( );

		if ( !searcher->scan( target_drive ) )
		{
			LOG( "Scan failed." );
			return 1;
		}

		auto end_time = std::chrono::high_resolution_clock::now( );

		std::chrono::duration<double> elapsed = end_time - start_time;

		LOG( "Scan completed in {:.4f} seconds.", elapsed.count() );

		return 0;
	}
	catch ( const std::exception& e )
	{
		LOG( "Critical exception: {}", e.what() );
		MessageBoxA( nullptr, e.what( ), "Critical Error", MB_OK | MB_ICONERROR );
		return 1;
	}
}
