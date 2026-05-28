#include "Stdafx.hpp"
#include "Searcher/Searcher.hpp"

Searcher::Searcher( )
{
	LOG( "Searcher initialized" );
}

bool Searcher::scan( char drive_letter )
{
	try
	{
		m_db.clear( );

		drive_letter = static_cast<char>( std::toupper( drive_letter ) );
		if ( drive_letter < 'A' || drive_letter > 'Z' )
		{
			return false;
		}

		m_db.set_drive_letter( drive_letter );

		std::wstring volume_path = L"\\\\.\\";
		volume_path              += static_cast<wchar_t>( drive_letter );
		volume_path              += L":";

		LOG( "Opening volume handle: {}", std::string(volume_path.begin(), volume_path.end()) );

		m_volume_handle.reset( CreateFileW(
			volume_path.c_str( ),
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr,
			OPEN_EXISTING,
			FILE_FLAG_OVERLAPPED,
			nullptr
		) );

		if ( m_volume_handle.get( ) == INVALID_HANDLE_VALUE )
		{
			LOG( "Failed to open volume. Error: {}", GetLastError() );
			return false;
		}

		USN_JOURNAL_DATA_V0 journal_data = {};
		if ( !query_journal_info( m_volume_handle.get( ), journal_data ) )
		{
			LOG( "Failed to query USN Journal info" );
			return false;
		}

		LOG( "USN Journal ID: {}, Next USN: {}", journal_data.UsnJournalID, journal_data.NextUsn );

		MFT_ENUM_DATA_V1 enum_data;
		enum_data.StartFileReferenceNumber = 0;
		enum_data.LowUsn                   = 0;
		enum_data.HighUsn                  = journal_data.NextUsn;
		enum_data.MinMajorVersion          = 2;
		enum_data.MaxMajorVersion          = 2;

		constexpr size_t buffer_size = 1024 * 1024 * 8;
		DWORD bytes_returned = 0;

		LOG( "Starting USN enumeration (Async)..." );

		m_db.reserve( 2000000 );

		auto alloc_buffer = []( )
		{
			return static_cast<uint8_t*>( VirtualAlloc( nullptr, buffer_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE ) );
		};
		auto free_buffer = []( uint8_t* p )
		{
			if ( p ) VirtualFree( p, 0, MEM_RELEASE );
		};

		uint8_t* buffers[ 2 ] = { alloc_buffer( ), alloc_buffer( ) };
		if ( !buffers[ 0 ] || !buffers[ 1 ] )
		{
			free_buffer( buffers[ 0 ] );
			free_buffer( buffers[ 1 ] );
			return false;
		}

		OVERLAPPED ol[ 2 ] = {};
		ol[ 0 ].hEvent     = CreateEventW( nullptr, TRUE, FALSE, nullptr );
		ol[ 1 ].hEvent     = CreateEventW( nullptr, TRUE, FALSE, nullptr );

		int current_idx = 0;

		auto queue_read = [&]( int idx ) -> bool
		{
			ResetEvent( ol[ idx ].hEvent );
			BOOL success = DeviceIoControl(
				m_volume_handle.get( ), FSCTL_ENUM_USN_DATA,
				&enum_data, sizeof( enum_data ),
				buffers[ idx ], buffer_size,
				nullptr, &ol[ idx ]
			);

			if ( !success )
			{
				if ( GetLastError( ) != ERROR_IO_PENDING )
				{
					return false;
				}
			}
			return true;
		};

		bool reading = queue_read( current_idx );

		while ( reading )
		{
			if ( !GetOverlappedResult( m_volume_handle.get( ), &ol[ current_idx ], &bytes_returned, TRUE ) )
			{
				break;
			}

			if ( bytes_returned <= sizeof( USN ) )
			{
				break;
			}

			enum_data.StartFileReferenceNumber = *reinterpret_cast<DWORD64*>( buffers[ current_idx ] );

			int next_idx = 1 - current_idx;
			reading      = queue_read( next_idx );

			process_buffer( buffers[ current_idx ], bytes_returned );

			current_idx = next_idx;
		}

		CloseHandle( ol[ 0 ].hEvent );
		CloseHandle( ol[ 1 ].hEvent );
		free_buffer( buffers[ 0 ] );
		free_buffer( buffers[ 1 ] );

		m_db.finalize( );
		return true;
	}
	catch ( const std::exception& e )
	{
		LOG( "Exception during scan: {}", e.what() );
		return false;
	}
}

bool Searcher::query_journal_info( HANDLE volume, USN_JOURNAL_DATA_V0& out_data )
{
	DWORD bytes_returned = 0;
	return DeviceIoControl(
		volume,
		FSCTL_QUERY_USN_JOURNAL,
		nullptr,
		0,
		&out_data,
		sizeof( out_data ),
		&bytes_returned,
		nullptr
	);
}

void Searcher::process_buffer( const uint8_t* buffer, DWORD size )
{
	DWORD offset = sizeof( USN );

	while ( offset < size )
	{
		auto* record = reinterpret_cast<const USN_RECORD_V2*>( buffer + offset );

		if ( record->RecordLength == 0 )
			break;

		std::wstring_view filename_view(
			reinterpret_cast<const wchar_t*>( reinterpret_cast<const char*>( record ) + record->FileNameOffset ),
			record->FileNameLength / sizeof( wchar_t )
		);

		bool is_directory = ( record->FileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0;

		m_db.insert(
			record->FileReferenceNumber,
			record->ParentFileReferenceNumber,
			filename_view,
			is_directory
		);

		offset += record->RecordLength;
	}
}
