#pragma once

/// @brief Custom deleter for Win32 handles to be used with smart pointers.
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

/// @brief RAII wrapper for Win32 handles using std::unique_ptr.
using UniqueHandle = std::unique_ptr<void, HandleDeleter>;
