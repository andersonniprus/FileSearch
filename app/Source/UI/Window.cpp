#include "Stdafx.hpp"
#include "UI/Window.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

namespace UI
{
	Window::Window( const std::wstring& title )
	{
		wc_ = {
			.cbSize = sizeof( WNDCLASSEXW ),
			.style = CS_CLASSDC,
			.lpfnWndProc = wnd_proc,
			.hInstance = GetModuleHandle( nullptr ),
			.lpszClassName = L"CleanWindow",
		};

		RegisterClassExW( &wc_ );

		const int screen_x = GetSystemMetrics( SM_CXSCREEN );
		const int screen_y = GetSystemMetrics( SM_CYSCREEN );

		hwnd_ = CreateWindowW(
			wc_.lpszClassName,
			title.c_str(),
			WS_OVERLAPPEDWINDOW,
			(screen_x - WIDTH) / 2,
			(screen_y - HEIGHT) / 2,
			WIDTH, HEIGHT,
			nullptr, nullptr, wc_.hInstance, this
		);

		renderer_ = std::make_unique<Renderer>( hwnd_ );

		ShowWindow( hwnd_, SW_SHOWDEFAULT );
		UpdateWindow( hwnd_ );

		initialize_imgui( );
	}

	Window::~Window( )
	{
		shutdown_imgui( );
		renderer_.reset( );

		DestroyWindow( hwnd_ );
		UnregisterClassW( wc_.lpszClassName, wc_.hInstance );
	}

	void Window::close( )
	{
		running_ = false;
	}

	void Window::handle_resize( UINT width, UINT height ) const
	{
		if ( renderer_ )
		{
			renderer_->resize( static_cast<int>( width ), static_cast<int>( height ) );
		}
	}

	void Window::initialize_imgui( ) const
	{
		IMGUI_CHECKVERSION( );

		ImGui::CreateContext( );
		ImGui::StyleColorsDark( );

		ImGui::GetIO( ).IniFilename = nullptr;

		ImGui_ImplWin32_Init( hwnd_ );
		ImGui_ImplDX11_Init( renderer_->get_device( ), renderer_->get_device_context( ) );
	}

	void Window::shutdown_imgui( )
	{
		ImGui_ImplDX11_Shutdown( );
		ImGui_ImplWin32_Shutdown( );

		ImGui::DestroyContext( );
	}

	LRESULT CALLBACK Window::wnd_proc( const HWND hwnd, const UINT msg, const WPARAM wparam, const LPARAM lparam )
	{
		if ( ImGui_ImplWin32_WndProcHandler( hwnd, msg, wparam, lparam ) ) return true;

		Window* window = nullptr;
		if ( msg == WM_NCCREATE )
		{
			auto cs = reinterpret_cast<CREATESTRUCT*>( lparam );
			window  = static_cast<Window*>( cs->lpCreateParams );
			SetWindowLongPtr( hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( window ) );
		}
		else
		{
			window = reinterpret_cast<Window*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
		}

		if ( window )
		{
			if ( msg == WM_SIZE )
			{
				if ( wparam != SIZE_MINIMIZED )
				{
					window->handle_resize( LOWORD( lparam ), HIWORD( lparam ) );
				}
				return 0;
			}
		}

		if ( msg == WM_DESTROY )
		{
			PostQuitMessage( 0 );
			return 0;
		}

		return DefWindowProc( hwnd, msg, wparam, lparam );
	}
}
