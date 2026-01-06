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
		const auto searcher = get_service<Searcher>( );

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


		LOG( "Scan completed in {:.2f} seconds.", elapsed.count() );
		LOG( "Files Found:       {}", searcher->get_file_count() );
		LOG( "Directories Found: {}", searcher->get_directory_count() );
		LOG( "Total Index Size:  {}", searcher->get_file_count() + searcher->get_directory_count() );

		{
			constexpr auto log_files_count = 5;

			size_t count      = 0;
			char drive_letter = target_drive;

			searcher->for_each_file( [&]( const std::wstring& path )
			{
				if ( count < log_files_count )
				{
					std::string full_path_str;
					full_path_str += drive_letter;
					full_path_str += ":\\";

					std::string path_utf8( path.begin( ), path.end( ) );
					full_path_str += path_utf8;

					LOG( "{}", full_path_str );
				}
				count++;
			} );
		}

		return 0;
	}
	catch ( const std::exception& e )
	{
		LOG( "Critical exception: {}", e.what() );
		MessageBoxA( nullptr, e.what( ), "Critical Error", MB_OK | MB_ICONERROR );
		return 1;
	}
}
