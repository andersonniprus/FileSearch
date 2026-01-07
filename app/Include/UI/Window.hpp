#pragma once
#include "UI/Renderer.hpp"

namespace UI
{
	class Window
	{
	public:
		explicit Window( const std::wstring& title );
		~Window( );

		void run( const std::function<void( )>& on_render );
		void close( );

	private:
		void handle_resize( UINT width, UINT height ) const;

		void initialize_imgui( ) const;
		static void shutdown_imgui( );

		static LRESULT CALLBACK wnd_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam );

		WNDCLASSEXW wc_;
		HWND hwnd_;
		bool running_ = true;

		std::unique_ptr<Renderer> renderer_;

		static constexpr int WIDTH  = 1280;
		static constexpr int HEIGHT = 800;
	};
}
