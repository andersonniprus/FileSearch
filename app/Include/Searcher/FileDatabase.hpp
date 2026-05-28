#pragma once

/// @brief Represents a single file or directory entry in the database.
struct FileEntry
{
	DWORD64 id;
	DWORD64 parent_id;
	std::wstring_view name;
	bool is_directory;
};

/// @brief High-performance flat database for storing and resolving file system entries.
class FileDatabase
{
public:
	enum class SortColumn { None, Name, Path, Type };
	enum class SortDirection { Ascending, Descending };

	/// @brief Pre-allocates memory for the internal entries vector.
	/// @param count Expected number of entries.
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
	/// @param id Unique file reference number.
	/// @param parent_id ID of the parent directory.
	/// @param name Filename.
	/// @param is_directory True if entry is a directory.
	void insert( DWORD64 id, const DWORD64 parent_id, const std::wstring_view name, const bool is_directory )
	{
		const std::wstring_view stored_name = m_pool.add( name );

		m_entries.push_back( FileEntry { id, parent_id, stored_name, is_directory } );

		if ( is_directory )
		{
			m_directory_count++;
		}
		else
		{
			m_file_count++;
		}
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

	/// @brief Applies a case-insensitive filter and sorts the view.
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

			for ( size_t i = 0; i < m_entries.size( ); ++i )
			{
				const auto& name = m_entries[ i ].name;
				auto it          = std::search( name.begin( ), name.end( ), uquery.begin( ), uquery.end( ),
					[&]( wchar_t c1, wchar_t c2 ) { return to_upper( c1 ) == c2; } );

				if ( it != name.end( ) )
				{
					m_view_indices.push_back( i );
				}
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
				if ( col == SortColumn::Name )
				{
					cmp_res = a.name.compare( b.name );
				}
				else if ( col == SortColumn::Type )
				{
					if ( a.is_directory != b.is_directory ) cmp_res = a.is_directory ? -1 : 1;
					else cmp_res = a.name.compare( b.name );
				}
				else if ( col == SortColumn::Path )
				{
					cmp_res = resolve_path_unlocked( a.id ).compare( resolve_path_unlocked( b.id ) );
				}
				return dir == SortDirection::Ascending ? ( cmp_res < 0 ) : ( cmp_res > 0 );
			};
			std::sort( std::execution::par_unseq, m_view_indices.begin( ), m_view_indices.end( ), cmp );
		}
	}

	/// @brief Sets the current drive letter being scanned.
	void set_drive_letter( char drive )
	{
		std::unique_lock lock( m_mutex );
		m_drive_letter = drive;
	}

	/// @brief Iterates over all resolved paths in the database.
	/// @param callback Invocable receiving the full path string.
	template<std::invocable<const std::wstring&> F>
	void visit_all_paths( F&& callback )
	{
		std::shared_lock lock( m_mutex );

		const size_t count = m_view_active ? m_view_indices.size( ) : m_entries.size( );
		for ( size_t i = 0; i < count; ++i )
		{
			size_t real_index = m_view_active ? m_view_indices[ i ] : i;
			std::wstring full_path = resolve_path_unlocked( m_entries[ real_index ].id );
			callback( full_path );
		}
	}

	/// @brief Gets the total number of files in the database.
	[[nodiscard]] size_t get_file_count( ) const noexcept
	{
		std::shared_lock lock( m_mutex );
		return m_file_count;
	}

	/// @brief Gets the total number of directories in the database.
	[[nodiscard]] size_t get_directory_count( ) const noexcept
	{
		std::shared_lock lock( m_mutex );
		return m_directory_count;
	}

	/// @brief Gets the total number of entries (files + directories).
	[[nodiscard]] size_t get_total_entries( ) const noexcept
	{
		std::shared_lock lock( m_mutex );
		return m_view_active ? m_view_indices.size( ) : m_entries.size( );
	}

	/// @brief Gets the name of the entry at a specific index without resolving full path.
	[[nodiscard]] std::wstring_view get_name_by_index( size_t index ) const
	{
		std::shared_lock lock( m_mutex );

		size_t real_index = m_view_active ? m_view_indices[ index ] : index;
		if ( real_index >= m_entries.size( ) )
		{
			return {};
		}

		return m_entries[ real_index ].name;
	}

	/// @brief Resolves the full path for an entry at a specific index.
	/// @param index Position in the entries vector.
	/// @return Resolved full path string.
	[[nodiscard]] std::wstring get_path_by_index( size_t index ) const
	{
		std::shared_lock lock( m_mutex );

		size_t real_index = m_view_active ? m_view_indices[ index ] : index;
		if ( real_index >= m_entries.size( ) )
		{
			return L"";
		}

		return resolve_path_unlocked( m_entries[ real_index ].id );
	}

	/// @brief Checks if the entry at a specific index is a directory.
	[[nodiscard]] bool is_directory_by_index( size_t index ) const
	{
		std::shared_lock lock( m_mutex );

		size_t real_index = m_view_active ? m_view_indices[ index ] : index;
		if ( real_index >= m_entries.size( ) )
		{
			return false;
		}

		return m_entries[ real_index ].is_directory;
	}

	private:
	/// @brief Internal path resolution using binary search on sorted entries.
	[[nodiscard]] std::wstring resolve_path_unlocked( const DWORD64 id ) const
	{
		alignas(64) thread_local std::byte buffer[ 4096 ];
		std::pmr::monotonic_buffer_resource mbr( buffer, sizeof( buffer ) );
		std::pmr::vector<std::wstring_view> path_parts( &mbr );
		path_parts.reserve( 32 );

		DWORD64 current_id = id;
		int depth          = 0;

		while ( depth++ < 256 )
		{
			constexpr DWORD64 INVALID_ID   = 0xFFFFFFFFFFFFFFFFULL;
			constexpr DWORD64 MAX_VALID_ID = 0xFFFFFFFFFFFFFFFEULL;

			if ( current_id == INVALID_ID || current_id > MAX_VALID_ID )
			{
				break;
			}

			auto it = std::ranges::lower_bound( m_entries, current_id, {}, &FileEntry::id );
			if ( it == m_entries.end( ) || it->id != current_id ) break;

			path_parts.push_back( it->name );

			if ( it->parent_id == current_id ||
			     it->parent_id == 0 ||
			     it->parent_id == INVALID_ID ||
			     it->parent_id > MAX_VALID_ID )
			{
				break;
			}

			current_id = it->parent_id;
		}

		if ( path_parts.empty( ) ) return L"";

		size_t len = 3;
		for ( const auto& p : path_parts )
		{
			len += p.length( ) + 1;
		}
		std::wstring full_path;
		full_path.reserve( len );

		full_path += static_cast<wchar_t>( m_drive_letter );
		full_path += L":\\";

		bool first = true;
		for ( auto it = path_parts.rbegin( ); it != path_parts.rend( ); ++it )
		{
			if ( !first ) full_path += L"\\";
			full_path += *it;
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
