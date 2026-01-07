#pragma once

struct FileNode
{
	DWORD64 parent_id;
	std::wstring_view name;
	bool is_directory;
};

class FileDatabase
{
public:
	void reserve( const size_t count )
	{
		std::lock_guard<std::mutex> lock( m_mutex );
		m_files.reserve( count );
		m_index.reserve( count );
	}

	void clear( )
	{
		std::lock_guard<std::mutex> lock( m_mutex );
		m_files.clear( );
		m_index.clear( );
		m_file_count      = 0;
		m_directory_count = 0;
	}

	void insert( DWORD64 id, const DWORD64 parent_id, const std::wstring_view name, const bool is_directory )
	{
		std::lock_guard<std::mutex> lock( m_mutex );

		const std::wstring_view stored_name = m_pool.add( name );

		m_files.emplace( id, FileNode { parent_id, stored_name, is_directory } );
		m_index.push_back( id );

		if ( is_directory )
		{
			m_directory_count++;
		}
		else
		{
			m_file_count++;
		}
	}

	void visit_all_paths( const std::function<void( const std::wstring& )>& callback )
	{
		std::lock_guard<std::mutex> lock( m_mutex );

		for ( const auto& id : m_files | std::views::keys )
		{
			std::wstring full_path = resolve_path_unlocked( id );
			callback( full_path );
		}
	}

	size_t get_file_count( ) const
	{
		std::lock_guard<std::mutex> lock( m_mutex );
		return m_file_count;
	}

	size_t get_directory_count( ) const
	{
		std::lock_guard<std::mutex> lock( m_mutex );
		return m_directory_count;
	}

	size_t get_total_entries( ) const
	{
		std::lock_guard<std::mutex> lock( m_mutex );
		return m_files.size( );
	}

	std::wstring get_path_by_index( size_t index ) const
	{
		std::lock_guard<std::mutex> lock( m_mutex );

		if ( index >= m_index.size( ) )
		{
			return L"";
		}

		DWORD64 id = m_index[ index ];
		auto it    = m_files.find( id );
		if ( it == m_files.end( ) )
		{
			return L"";
		}

		return resolve_path_unlocked( id );
	}

	bool is_directory_by_index( size_t index ) const
	{
		std::lock_guard<std::mutex> lock( m_mutex );

		if ( index >= m_index.size( ) )
		{
			return false;
		}

		DWORD64 id = m_index[ index ];
		auto it    = m_files.find( id );
		if ( it == m_files.end( ) )
		{
			return false;
		}

		return it->second.is_directory;
	}

private:

	std::wstring resolve_path_unlocked( const DWORD64 id ) const
	{
		thread_local std::vector<std::wstring_view> path_parts;
		path_parts.clear();
		path_parts.reserve(16);

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

			auto it = m_files.find( current_id );
			if ( it == m_files.end( ) ) break;

			path_parts.push_back( it->second.name );

			if ( it->second.parent_id == current_id ||
			     it->second.parent_id == 0 ||
			     it->second.parent_id == INVALID_ID ||
			     it->second.parent_id > MAX_VALID_ID )
			{
				break;
			}

			current_id = it->second.parent_id;
		}

		if ( path_parts.empty( ) ) return L"";

		size_t len = 0;
		for ( const auto& p : path_parts )
		{
			len += p.length( ) + 1;
		}
		std::wstring full_path;
		full_path.reserve( len );

		for ( auto it = path_parts.rbegin( ); it != path_parts.rend( ); ++it )
		{
			if ( !full_path.empty( ) ) full_path += L"\\";
			full_path += *it;
		}
		return full_path;
	}

	std::unordered_map<DWORD64, FileNode> m_files;
	std::vector<DWORD64> m_index;
	mutable std::mutex m_mutex;
	StringPool<wchar_t> m_pool { 1024 * 1024 * 4 };

	size_t m_file_count      = 0;
	size_t m_directory_count = 0;
};
