#include "Stdafx.hpp"

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
		if ( drive_letter < 'A' || drive_letter > 'Z' ) return false;

		m_db.set_drive_letter( drive_letter );

		std::wstring volume_path = L"\\\\.\\";
		volume_path              += static_cast<wchar_t>( drive_letter );
		volume_path              += L":";

		LOG( "Opening volume handle (MFT): {}", std::string( volume_path.begin( ), volume_path.end( ) ) );

		m_volume_handle.reset( CreateFileW(
			volume_path.c_str( ),
			GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr,
			OPEN_EXISTING,
			FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING,
			nullptr
		) );

		if ( m_volume_handle.get( ) == INVALID_HANDLE_VALUE )
		{
			DWORD error = GetLastError( );
			LOG( "Failed to open raw volume. Error: {}", error );
			if ( error == ERROR_ACCESS_DENIED )
			{
				LOG( "Access denied. MFT parsing requires Administrator privileges." );
			}
			return false;
		}

		if ( !parse_boot_sector( m_volume_handle.get( ) ) )
		{
			LOG( "Failed to parse Boot Sector." );
			return false;
		}

		if ( !extract_mft_runs( m_volume_handle.get( ) ) )
		{
			LOG( "Failed to extract MFT runs." );
			return false;
		}

		LOG( "Starting raw MFT enumeration (Async)..." );

		m_db.reserve( 2000000 );

		constexpr size_t buffer_size = 1024 * 1024 * 8;
		auto free_buffer = []( void* p ) { if ( p ) VirtualFree( p, 0, MEM_RELEASE ); };
		
		std::unique_ptr<uint8_t[], decltype(free_buffer)> buffer_a(
			static_cast<uint8_t*>( VirtualAlloc( nullptr, buffer_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE ) ), free_buffer );
		std::unique_ptr<uint8_t[], decltype(free_buffer)> buffer_b(
			static_cast<uint8_t*>( VirtualAlloc( nullptr, buffer_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE ) ), free_buffer );

		uint8_t* buffers[ 2 ] = { buffer_a.get(), buffer_b.get() };
		if ( !buffers[ 0 ] || !buffers[ 1 ] ) return false;

		OVERLAPPED ol[ 2 ] = {};
		ol[ 0 ].hEvent     = CreateEventW( nullptr, TRUE, FALSE, nullptr );
		ol[ 1 ].hEvent     = CreateEventW( nullptr, TRUE, FALSE, nullptr );

		int current_idx = 0;

		for ( const auto& run : m_mft_runs )
		{
			uint64_t physical_offset = run.logical_cluster * m_bytes_per_cluster;
			uint64_t bytes_to_read   = run.cluster_count * m_bytes_per_cluster;
			uint64_t bytes_read_so_far = 0;

			auto queue_read = [&]( int idx, uint64_t offset, uint32_t size ) -> bool
			{
				if ( size == 0 ) return false;
				ResetEvent( ol[ idx ].hEvent );
				ol[ idx ].Offset     = static_cast<DWORD>( offset & 0xFFFFFFFF );
				ol[ idx ].OffsetHigh = static_cast<DWORD>( offset >> 32 );

				BOOL success = ReadFile( m_volume_handle.get( ), buffers[ idx ], size, nullptr, &ol[ idx ] );
				if ( !success && GetLastError( ) != ERROR_IO_PENDING )
				{
					return false;
				}
				return true;
			};

			uint32_t first_read_size = static_cast<uint32_t>( std::min<uint64_t>( buffer_size, bytes_to_read ) );
			bool reading = queue_read( current_idx, physical_offset, first_read_size );

			while ( reading )
			{
				DWORD bytes_returned = 0;
				if ( !GetOverlappedResult( m_volume_handle.get( ), &ol[ current_idx ], &bytes_returned, TRUE ) )
				{
					break;
				}

				bytes_read_so_far += bytes_returned;
				uint64_t remaining_bytes = bytes_to_read - bytes_read_so_far;

				int next_idx = 1 - current_idx;
				uint32_t next_read_size = static_cast<uint32_t>( std::min<uint64_t>( buffer_size, remaining_bytes ) );
				
				reading = queue_read( next_idx, physical_offset + bytes_read_so_far, next_read_size );

				process_mft_buffer( buffers[ current_idx ], bytes_returned );

				current_idx = next_idx;
			}
		}

		CloseHandle( ol[ 0 ].hEvent );
		CloseHandle( ol[ 1 ].hEvent );

		m_db.finalize( );
		return true;
	}
	catch ( const std::exception& e )
	{
		LOG( "Exception during MFT scan: {}", e.what( ) );
		return false;
	}
}

[[nodiscard]] bool Searcher::parse_boot_sector( HANDLE volume )
{
	constexpr size_t sector_size = 4096;
	auto free_v = []( void* p ) { if ( p ) VirtualFree( p, 0, MEM_RELEASE ); };
	std::unique_ptr<uint8_t[], decltype(free_v)> buffer(
		static_cast<uint8_t*>( VirtualAlloc( nullptr, sector_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE ) ), free_v );
	
	if ( !buffer ) return false;

	OVERLAPPED ol = {};
	ol.hEvent     = CreateEventW( nullptr, TRUE, FALSE, nullptr );
	
	BOOL success = ReadFile( volume, buffer.get(), sector_size, nullptr, &ol );
	if ( !success && GetLastError( ) != ERROR_IO_PENDING )
	{
		CloseHandle( ol.hEvent );
		return false;
	}

	DWORD bytes_returned = 0;
	GetOverlappedResult( volume, &ol, &bytes_returned, TRUE );
	CloseHandle( ol.hEvent );

	auto* boot_sector = reinterpret_cast<const NTFS::BootSector*>( buffer.get() );

	if ( std::strncmp( boot_sector->oem_id, "NTFS    ", 8 ) != 0 )
	{
		return false;
	}

	m_bytes_per_cluster = boot_sector->bytes_per_sector * boot_sector->sectors_per_cluster;
	
	if ( boot_sector->clusters_per_mft_record > 0 )
	{
		m_bytes_per_record = boot_sector->clusters_per_mft_record * m_bytes_per_cluster;
	}
	else
	{
		m_bytes_per_record = 1 << std::abs( boot_sector->clusters_per_mft_record );
	}

	m_mft_start_cluster = boot_sector->mft_logical_cluster_number;

	return m_bytes_per_cluster > 0 && m_bytes_per_record > 0;
}

[[nodiscard]] bool Searcher::extract_mft_runs( HANDLE volume )
{
	size_t read_size = m_bytes_per_cluster;
	if ( read_size < m_bytes_per_record ) read_size = m_bytes_per_record;

	auto free_v = []( void* p ) { if ( p ) VirtualFree( p, 0, MEM_RELEASE ); };
	std::unique_ptr<uint8_t[], decltype(free_v)> buffer(
		static_cast<uint8_t*>( VirtualAlloc( nullptr, read_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE ) ), free_v );

	if ( !buffer ) return false;

	uint64_t mft_offset = m_mft_start_cluster * m_bytes_per_cluster;

	OVERLAPPED ol = {};
	ol.hEvent     = CreateEventW( nullptr, TRUE, FALSE, nullptr );
	ol.Offset     = static_cast<DWORD>( mft_offset & 0xFFFFFFFF );
	ol.OffsetHigh = static_cast<DWORD>( mft_offset >> 32 );

	BOOL success = ReadFile( volume, buffer.get(), static_cast<DWORD>( read_size ), nullptr, &ol );
	if ( !success && GetLastError( ) != ERROR_IO_PENDING )
	{
		CloseHandle( ol.hEvent );
		return false;
	}

	DWORD bytes_returned = 0;
	GetOverlappedResult( volume, &ol, &bytes_returned, TRUE );
	CloseHandle( ol.hEvent );

	auto* record = reinterpret_cast<const NTFS::MftRecordHeader*>( buffer.get() );
	if ( record->magic != 0x454C4946 )
	{
		return false;
	}

	uint32_t attr_offset = record->first_attribute_offset;
	while ( attr_offset < record->real_size )
	{
		auto* attr = reinterpret_cast<const NTFS::AttributeHeader*>( buffer.get() + attr_offset );
		if ( attr->type == 0xFFFFFFFF ) break;

		if ( attr->type == 0x80 )
		{
			if ( attr->non_resident )
			{
				auto* non_resident = reinterpret_cast<const NTFS::NonResidentAttributeHeader*>( attr );
				const uint8_t* runlist = buffer.get() + attr_offset + non_resident->data_runs_offset;
				
				uint64_t current_lcn = 0;
				while ( *runlist != 0 )
				{
					uint8_t header = *runlist;
					uint8_t length_size = header & 0x0F;
					uint8_t offset_size = ( header >> 4 ) & 0x0F;
					runlist++;

					uint64_t length = decode_run_length( runlist, length_size );
					runlist += length_size;

					int64_t offset = decode_run_offset( runlist, offset_size );
					runlist += offset_size;

					current_lcn += offset;
					
					m_mft_runs.push_back( { current_lcn, length } );
				}
			}
			break;
		}

		attr_offset += attr->length;
	}

	return !m_mft_runs.empty( );
}

int64_t Searcher::decode_run_offset( const uint8_t* runlist, uint8_t offset_size ) noexcept
{
	int64_t offset = 0;
	for ( uint8_t i = 0; i < offset_size; ++i )
	{
		offset |= static_cast<int64_t>( runlist[ i ] ) << ( i * 8 );
	}

	if ( offset_size > 0 && ( runlist[ offset_size - 1 ] & 0x80 ) )
	{
		for ( uint8_t i = offset_size; i < 8; ++i )
		{
			offset |= 0xFFLL << ( i * 8 );
		}
	}
	return offset;
}

uint64_t Searcher::decode_run_length( const uint8_t* runlist, uint8_t length_size ) noexcept
{
	uint64_t length = 0;
	for ( uint8_t i = 0; i < length_size; ++i )
	{
		length |= static_cast<uint64_t>( runlist[ i ] ) << ( i * 8 );
	}
	return length;
}

void Searcher::process_mft_buffer( const uint8_t* buffer, size_t size )
{
	size_t offset = 0;
	while ( offset + m_bytes_per_record <= size )
	{
		const uint8_t* record_buffer = buffer + offset;
		auto* record = reinterpret_cast<const NTFS::MftRecordHeader*>( record_buffer );

		if ( record->magic == 0x454C4946 && ( record->flags & 0x01 ) )
		{
			bool is_directory = ( record->flags & 0x02 ) != 0;
			uint32_t record_number = record->record_number;
			
			if ( record->base_reference == 0 )
			{
				uint32_t attr_offset = record->first_attribute_offset;
				while ( attr_offset < record->real_size && attr_offset + sizeof(NTFS::AttributeHeader) <= m_bytes_per_record )
				{
					auto* attr = reinterpret_cast<const NTFS::AttributeHeader*>( record_buffer + attr_offset );
					if ( attr->type == 0xFFFFFFFF ) break;
					if ( attr->length == 0 ) break;

					if ( attr->type == 0x30 )
					{
						if ( !attr->non_resident )
						{
							auto* resident = reinterpret_cast<const NTFS::ResidentAttributeHeader*>( attr );
							auto* fn_attr = reinterpret_cast<const NTFS::FileNameAttribute*>( record_buffer + attr_offset + resident->value_offset );

							if ( fn_attr->name_type == 1 || fn_attr->name_type == 3 || fn_attr->name_type == 0 )
							{
								DWORD64 parent_id = fn_attr->parent_directory & 0x0000FFFFFFFFFFFFULL;
								std::wstring_view name_view( fn_attr->name, fn_attr->name_length );
								m_db.insert( record_number, parent_id, name_view, fn_attr->real_size, fn_attr->file_accessed_time, is_directory );
								break;
							}
						}
					}
					attr_offset += attr->length;
				}
			}
		}

		offset += m_bytes_per_record;
	}
}
