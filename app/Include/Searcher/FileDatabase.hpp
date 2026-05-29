#pragma once

/// @brief Represents a single file or directory entry in the database.
struct FileEntry
{
	DWORD64 id;
	DWORD64 parent_id;
	std::wstring_view name;
	uint64_t file_size;
	uint64_t timestamp;
	bool is_directory;
};

/// @brief High-performance flat database for storing and resolving file system entries.
class FileDatabase
{
public:
	enum class SortColumn { None, Name, Path, Size, Date, Type };
	enum class SortDirection { Ascending, Descending };

	/// @brief Pre-allocates memory for the internal entries vector.
	void reserve( const size_t count )
	{
		m_entries.reserve( count );
	}

	/// @brief Clears all entries and resets counters.
	void clear( )
	{
		std::unique_lock lock( m_mutex );
		m_entries.clear( );
		m_file_count      = 0;
		m_directory_count = 0;
	}

	/// @brief Inserts a new entry into the database.
	void insert( DWORD64 id, const DWORD64 parent_id, const std::wstring_view name, uint64_t file_size, uint64_t timestamp, const bool is_directory )
	{
		const std::wstring_view stored_name = m_pool.add( name );

		m_entries.push_back( FileEntry { id, parent_id, stored_name, file_size, timestamp, is_directory } );

		if ( is_directory ) m_directory_count++;
		else m_file_count++;
	}

	/// @brief Sorts entries by ID to enable fast binary search resolution.
	void finalize( )
	{
		std::unique_lock lock( m_mutex );
		std::sort( std::execution::par_unseq, m_entries.begin(), m_entries.end(), []( const FileEntry& a, const FileEntry& b ) {
			return a.id < b.id;
		} );
		m_view_active = false;
		m_view_indices.clear( );
	}

	/// @brief Applies a parallel case-insensitive filter and sorts the view.
	void apply_filter_and_sort( const std::wstring_view query, SortColumn col, SortDirection dir )
	{
		std::unique_lock lock( m_mutex );
		if ( query.empty( ) && col == SortColumn::None )
		{
			m_view_active = false;
			m_view_indices.clear( );
			return;
		}

		m_view_active = true;
		m_view_indices.clear( );

		if ( !query.empty( ) )
		{
			auto to_upper = []( wchar_t c ) -> wchar_t
			{
				return ( c >= L'a' && c <= L'z' ) ? c - ( L'a' - L'A' ) : c;
			};

			std::wstring uquery;
			uquery.reserve( query.length( ) );
			for ( wchar_t c : query ) uquery.push_back( to_upper( c ) );

			const size_t n = m_entries.size( );
			const size_t num_threads = std::thread::hardware_concurrency( );
			const size_t chunk_size = ( n + num_threads - 1 ) / num_threads;
			
			std::vector<std::vector<size_t>> per_thread_results( num_threads );
			std::vector<size_t> chunk_indices( num_threads );
			std::iota( chunk_indices.begin( ), chunk_indices.end( ), 0 );

			std::for_each( std::execution::par_unseq, chunk_indices.begin( ), chunk_indices.end( ), [&]( size_t thread_idx ) {
				size_t start = thread_idx * chunk_size;
				size_t end = (std::min)( start + chunk_size, n );
				auto& thread_vec = per_thread_results[ thread_idx ];
				thread_vec.reserve( chunk_size / 4 );

				for ( size_t i = start; i < end; ++i )
				{
					const auto& name = m_entries[ i ].name;
					auto it          = std::search( name.begin( ), name.end( ), uquery.begin( ), uquery.end( ),
						[&]( wchar_t c1, wchar_t c2 ) { return to_upper( c1 ) == c2; } );

					if ( it != name.end( ) )
					{
						thread_vec.push_back( i );
					}
				}
			} );

			for ( auto& thread_vec : per_thread_results )
			{
				m_view_indices.insert( m_view_indices.end( ), thread_vec.begin( ), thread_vec.end( ) );
			}
		}
		else
		{
			m_view_indices.resize( m_entries.size( ) );
			std::iota( m_view_indices.begin( ), m_view_indices.end( ), 0 );
		}

		if ( col != SortColumn::None && !m_view_indices.empty( ) )
		{
			auto cmp = [this, col, dir]( size_t a_idx, size_t b_idx ) {
				const auto& a = m_entries[ a_idx ];
				const auto& b = m_entries[ b_idx ];
				int cmp_res = 0;
				if ( col == SortColumn::Name ) cmp_res = a.name.compare( b.name );
				else if ( col == SortColumn::Size ) cmp_res = ( a.file_size < b.file_size ) ? -1 : ( a.file_size > b.file_size );
				else if ( col == SortColumn::Date ) cmp_res = ( a.timestamp < b.timestamp ) ? -1 : ( a.timestamp > b.timestamp );
				else if ( col == SortColumn::Type ) cmp_res = ( a.is_directory != b.is_directory ) ? ( a.is_directory ? -1 : 1 ) : a.name.compare( b.name );
				else if ( col == SortColumn::Path ) cmp_res = resolve_path_unlocked( a.id ).compare( resolve_path_unlocked( b.id ) );
				
				return dir == SortDirection::Ascending ? ( cmp_res < 0 ) : ( cmp_res > 0 );
			};
			std::sort( std::execution::par_unseq, m_view_indices.begin( ), m_view_indices.end( ), cmp );
		}
	}

	/// @brief Estimates current memory usage in bytes.
	[[nodiscard]] size_t get_memory_usage( ) const noexcept
	{
		std::shared_lock lock( m_mutex );
		size_t usage = sizeof( *this );
		usage += m_entries.capacity( ) * sizeof( FileEntry );
		usage += m_view_indices.capacity( ) * sizeof( size_t );
		usage += m_pool.getBlockCount() * m_pool.getStandardBlockCapacity() * sizeof(wchar_t);
		return usage;
	}

	void set_drive_letter( char drive ) noexcept { std::unique_lock lock( m_mutex ); m_drive_letter = drive; }
	[[nodiscard]] size_t get_file_count( ) const noexcept { std::shared_lock lock( m_mutex ); return m_file_count; }
	[[nodiscard]] size_t get_directory_count( ) const noexcept { std::shared_lock lock( m_mutex ); return m_directory_count; }
	[[nodiscard]] size_t get_total_entries( ) const noexcept { std::shared_lock lock( m_mutex ); return m_view_active ? m_view_indices.size( ) : m_entries.size( ); }

	[[nodiscard]] std::wstring_view get_name_by_index( size_t index ) const noexcept
	{
		std::shared_lock lock( m_mutex );
		size_t real_index = m_view_active ? m_view_indices[ index ] : index;
		return m_entries[ real_index ].name;
	}

	[[nodiscard]] std::wstring get_path_by_index( size_t index ) const
	{
		std::shared_lock lock( m_mutex );
		size_t real_index = m_view_active ? m_view_indices[ index ] : index;
		return resolve_path_unlocked( m_entries[ real_index ].id );
	}

	[[nodiscard]] bool is_directory_by_index( size_t index ) const noexcept
	{
		std::shared_lock lock( m_mutex );
		size_t real_index = m_view_active ? m_view_indices[ index ] : index;
		return m_entries[ real_index ].is_directory;
	}

	[[nodiscard]] uint64_t get_size_by_index( size_t index ) const noexcept
	{
		std::shared_lock lock( m_mutex );
		size_t real_index = m_view_active ? m_view_indices[ index ] : index;
		return m_entries[ real_index ].file_size;
	}

	[[nodiscard]] uint64_t get_timestamp_by_index( size_t index ) const noexcept
	{
		std::shared_lock lock( m_mutex );
		size_t real_index = m_view_active ? m_view_indices[ index ] : index;
		return m_entries[ real_index ].timestamp;
	}

private:
	[[nodiscard]] std::wstring resolve_path_unlocked( const DWORD64 id ) const
	{
		alignas(64) thread_local std::byte buffer[ 4096 ];
		std::pmr::monotonic_buffer_resource mbr( buffer, sizeof( buffer ) );
		std::pmr::vector<std::wstring_view> path_parts( &mbr );
		path_parts.reserve( 32 );

		auto it = std::ranges::lower_bound( m_entries, id, {}, &FileEntry::id );
		if ( it == m_entries.end( ) || it->id != id ) return L"";

		DWORD64 current_id = id;
		int depth = 0;
		while ( depth++ < 256 )
		{
			auto entry_it = std::ranges::lower_bound( m_entries, current_id, {}, &FileEntry::id );
			if ( entry_it == m_entries.end( ) || entry_it->id != current_id ) break;
			
			path_parts.push_back( entry_it->name );
			if ( entry_it->parent_id == 0 || entry_it->parent_id == current_id ) break;
			current_id = entry_it->parent_id;
		}

		if ( path_parts.empty( ) ) return L"";

		std::wstring full_path;
		full_path.reserve( 256 );
		full_path += static_cast<wchar_t>( m_drive_letter );
		full_path += L":\\";

		bool first = true;
		for ( auto rit = path_parts.rbegin( ); rit != path_parts.rend( ); ++rit )
		{
			if ( !first ) full_path += L"\\";
			full_path += *rit;
			first     = false;
		}
		return full_path;
	}

	std::vector<FileEntry> m_entries;
	std::vector<size_t> m_view_indices;
	bool m_view_active = false;
	mutable std::shared_mutex m_mutex;
	StringPool<wchar_t> m_pool { 1024 * 1024 * 16 };

	size_t m_file_count      = 0;
	size_t m_directory_count = 0;
	char m_drive_letter      = 'C';
};