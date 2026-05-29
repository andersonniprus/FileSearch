#include "Stdafx.hpp"

Application::Application( )
	: searcher_( std::make_shared<Searcher>( ) )
{
	LOG( "Application initialized" );
}

int Application::run( ) const
{
	try
	{
		UI::Window window( L"File Searcher" );
		UI::ResultsPage results_page;

		results_page.set_searcher( searcher_ );

		window.run( [&results_page]
		{
			results_page.render( );
		} );

		return 0;
	}
	catch ( const std::exception& e )
	{
		LOG( "Critical exception: {}", e.what() );
		MessageBoxA( nullptr, e.what( ), "Critical Error", MB_OK | MB_ICONERROR );
		return 1;
	}
}
