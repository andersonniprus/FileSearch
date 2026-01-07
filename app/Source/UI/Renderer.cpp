#include "Stdafx.hpp"
#include "UI/Renderer.hpp"

namespace UI
{
	Renderer::Renderer( HWND hwnd )
	{
		create_device_and_swapchain( hwnd );
		create_render_target( );
	}

	Renderer::~Renderer( )
	{
		cleanup_render_target( );
	}

	void Renderer::resize( int width, int height )
	{
		if ( d3d_device_ && width > 0 && height > 0 )
		{
			cleanup_render_target( );
			swap_chain_->ResizeBuffers( 0, static_cast<UINT>( width ), static_cast<UINT>( height ), DXGI_FORMAT_UNKNOWN, 0 );
			create_render_target( );
		}
	}

	void Renderer::clear( )
	{
		constexpr float clear_color_with_alpha[ 4 ] = { 0.f, 0.f, 0.f, 1.f };
		d3d_device_context_->OMSetRenderTargets( 1, main_render_target_view_.GetAddressOf( ), nullptr );
		d3d_device_context_->ClearRenderTargetView( main_render_target_view_.Get( ), clear_color_with_alpha );
	}

	void Renderer::present( ) const
	{
		swap_chain_->Present( 1, 0 );
	}

	ID3D11Device* Renderer::get_device( ) const
	{
		return d3d_device_.Get( );
	}

	ID3D11DeviceContext* Renderer::get_device_context( ) const
	{
		return d3d_device_context_.Get( );
	}

	void Renderer::create_device_and_swapchain( HWND hwnd )
	{
		DXGI_SWAP_CHAIN_DESC sd               = {};
		sd.BufferCount                        = 2;
		sd.BufferDesc.Width                   = 0;
		sd.BufferDesc.Height                  = 0;
		sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator   = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow                       = hwnd;
		sd.SampleDesc.Count                   = 1;
		sd.SampleDesc.Quality                 = 0;
		sd.Windowed                           = TRUE;
		sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

		constexpr D3D_FEATURE_LEVEL feature_level_array[ ] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
		D3D_FEATURE_LEVEL feature_level;

		const HRESULT res = D3D11CreateDeviceAndSwapChain(
			nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
			feature_level_array, 2, D3D11_SDK_VERSION, &sd,
			&swap_chain_, &d3d_device_, &feature_level, &d3d_device_context_
		);

		if ( res != S_OK )
		{
			throw std::runtime_error( "Failed to create D3D11 device" );
		}
	}

	void Renderer::create_render_target( )
	{
		Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
		swap_chain_->GetBuffer( 0, IID_PPV_ARGS( back_buffer.GetAddressOf( ) ) );
		d3d_device_->CreateRenderTargetView( back_buffer.Get( ), nullptr, &main_render_target_view_ );
	}

	void Renderer::cleanup_render_target( )
	{
		main_render_target_view_.Reset( );
	}
}
