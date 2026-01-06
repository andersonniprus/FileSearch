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
		drive_letter = static_cast<char>( std::toupper( drive_letter ) );
		if ( drive_letter < 'A' || drive_letter > 'Z' )
		{
			return false;
		}

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
			0,
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

		constexpr size_t buffer_size = 1024 * 1024 * 2; // 2MB
		std::vector<uint8_t> buffer( buffer_size );
		DWORD bytes_returned = 0;

		LOG( "Starting USN enumeration..." );

		while ( true )
		{
			BOOL success = DeviceIoControl(
				m_volume_handle.get( ),
				FSCTL_ENUM_USN_DATA,
				&enum_data,
				sizeof( enum_data ),
				buffer.data( ),
				static_cast<DWORD>( buffer.size( ) ),
				&bytes_returned,
				nullptr
			);

			if ( !success )
			{
				DWORD error = GetLastError( );
				if ( error == ERROR_HANDLE_EOF )
				{
					LOG( "Scan finished (EOF)." );
					break;
				}
				LOG( "Enumeration stopped. Error: {}", error );
				break;
			}

			if ( bytes_returned <= sizeof( USN ) )
			{
				break;
			}

			process_buffer( buffer, bytes_returned );

			enum_data.StartFileReferenceNumber = *reinterpret_cast<DWORD64*>( buffer.data( ) );
		}

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

void Searcher::process_buffer( const std::vector<uint8_t>& buffer, DWORD size )
{
	DWORD offset = sizeof( USN );

	while ( offset < size )
	{
		auto* record = reinterpret_cast<const USN_RECORD_V2*>( buffer.data( ) + offset );

		if ( record->RecordLength == 0 )
			break;

		std::wstring filename(
			reinterpret_cast<const wchar_t*>( reinterpret_cast<const char*>( record ) + record->FileNameOffset ),
			record->FileNameLength / sizeof( wchar_t )
		);

		bool is_directory = ( record->FileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0;

		m_db.insert(
			record->FileReferenceNumber,
			record->ParentFileReferenceNumber,
			std::move( filename ),
			is_directory
		);

		offset += record->RecordLength;
	}
}
