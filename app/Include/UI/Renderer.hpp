#pragma once

namespace UI
{
	/// @brief D3D11 Abstraction layer for rendering ImGui and clearing frames.
	class Renderer
	{
	public:
		Renderer( HWND hwnd );
		~Renderer( );

		/// @brief Recreates swapchain buffers on window resize.
		void resize( int width, int height );

		/// @brief Clears the render target view.
		void clear( );

		/// @brief Swaps the back buffer to the screen.
		void present( ) const;

		/// @brief Accesses the internal D3D11 device.
		[[nodiscard]] ID3D11Device* get_device( ) const noexcept;

		/// @brief Accesses the internal D3D11 context.
		[[nodiscard]] ID3D11DeviceContext* get_device_context( ) const noexcept;

	private:
		void create_device_and_swapchain( HWND hwnd );
		void create_render_target( );
		void cleanup_render_target( );

		Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d_device_context_;
		Microsoft::WRL::ComPtr<IDXGISwapChain> swap_chain_;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> main_render_target_view_;
	};
}
