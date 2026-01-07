#pragma once

namespace UI
{
	class Renderer
	{
	public:
		Renderer( HWND hwnd );
		~Renderer( );

		void resize( int width, int height );
		void clear( );
		void present( ) const;

		[[nodiscard]] ID3D11Device* get_device( ) const;
		[[nodiscard]] ID3D11DeviceContext* get_device_context( ) const;

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
