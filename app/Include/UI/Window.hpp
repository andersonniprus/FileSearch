#pragma once

namespace UI
{
	/// @brief Manages the Win32 window lifecycle and ImGui event loop.
	class Window
	{
	public:
		/// @brief Creates the window and initializes D3D11.
		explicit Window( const std::wstring& title );
		~Window( );

		/// @brief The main message and render loop.
		/// @param on_render Callback invoked every frame for ImGui drawing.
		template<std::invocable F>
		void run( F&& on_render )
		{
			MSG msg;
			while ( running_ )
			{
				if ( PeekMessage( &msg, nullptr, 0U, 0U, PM_REMOVE ) )
				{
					TranslateMessage( &msg );
					DispatchMessage( &msg );
					if ( msg.message == WM_QUIT ) running_ = false;
					continue;
				}

				ImGui_ImplDX11_NewFrame( );
				ImGui_ImplWin32_NewFrame( );
				ImGui::NewFrame( );

				RECT client_rect;
				GetClientRect( hwnd_, &client_rect );
				const float window_width  = static_cast<float>( client_rect.right - client_rect.left );
				const float window_height = static_cast<float>( client_rect.bottom - client_rect.top );

				ImGui::SetNextWindowPos( { 0, 0 } );
				ImGui::SetNextWindowSize( { window_width, window_height } );

				constexpr auto flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus |
				                       ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

				if ( ImGui::Begin( "Main", nullptr, flags ) )
				{
					on_render( );
				}
				ImGui::End( );

				ImGui::Render( );

				renderer_->clear( );

				ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData( ) );

				renderer_->present( );
			}
		}

		/// @brief Signals the loop to terminate.
		void close( );

	private:
		/// @brief Forwards resize events to the renderer.
		void handle_resize( UINT width, UINT height ) const;

		/// @brief Sets up ImGui context and backends.
		void initialize_imgui( ) const;

		/// @brief Cleans up ImGui backends.
		static void shutdown_imgui( );

		/// @brief Win32 Window Procedure.
		static LRESULT CALLBACK wnd_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam );

		WNDCLASSEXW wc_;
		HWND hwnd_;
		bool running_ = true;

		std::unique_ptr<Renderer> renderer_;

		static constexpr int WIDTH  = 1280;
		static constexpr int HEIGHT = 800;
	};
}
