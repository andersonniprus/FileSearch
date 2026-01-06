#include "Stdafx.hpp"
#include "Application/Application.hpp"

#ifdef NDEBUG
int __stdcall WinMain( HINSTANCE, HINSTANCE, LPSTR, int )
#else
int main( int, char*[ ] )
#endif
{
	Application app;
	return app.run( );
}
