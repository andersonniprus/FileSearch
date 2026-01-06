#pragma once


struct HandleDeleter
{
	void operator()( HANDLE handle ) const
	{
		if ( handle && handle != INVALID_HANDLE_VALUE )
		{
			CloseHandle( handle );
		}
	}
};

using UniqueHandle = std::unique_ptr<void, HandleDeleter>;
