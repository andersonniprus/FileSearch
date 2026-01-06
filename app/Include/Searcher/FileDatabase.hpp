#pragma once

struct FileNode
{
	DWORD64 parent_id;
	std::wstring name;
	bool is_directory;
};

class FileDatabase
{
public:
	void insert( const DWORD64 id, const DWORD64 parent_id, std::wstring name, const bool is_directory )
	{
		std::lock_guard<std::mutex> lock( m_mutex );
		m_files[ id ] = { parent_id, std::move( name ), is_directory };
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

	size_t count( ) const
	{
		std::lock_guard<std::mutex> lock( m_mutex );
		return m_files.size( );
	}

private:
	std::wstring resolve_path_unlocked( const DWORD64 id )
	{
		std::wstring full_path;
		DWORD64 current_id = id;
		int depth_safety   = 0;

		while ( depth_safety++ < 256 )
		{
			auto it = m_files.find( current_id );
			if ( it == m_files.end( ) ) break;

			const auto& node = it->second;

			if ( !full_path.empty( ) ) full_path = L"\\" + full_path;
			full_path = node.name + full_path;

			if ( node.parent_id == current_id || node.parent_id == 0 ) break;

			current_id = node.parent_id;
		}
		return full_path;
	}

	std::unordered_map<DWORD64, FileNode> m_files;
	mutable std::mutex m_mutex;
};
