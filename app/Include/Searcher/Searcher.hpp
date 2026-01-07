#pragma once
#include "FileDatabase.hpp"

class Searcher
{
public:
	Searcher( );
	~Searcher( ) = default;

	bool scan( char drive_letter );

	void for_each_file( const std::function<void( const std::wstring& )>& callback )
	{
		m_db.visit_all_paths( callback );
	}

	[[nodiscard]] size_t get_file_count( ) const
	{
		return m_db.get_file_count( );
	}

	[[nodiscard]] size_t get_directory_count( ) const
	{
		return m_db.get_directory_count( );
	}

	[[nodiscard]] size_t get_total_entries( ) const
	{
		return m_db.get_total_entries( );
	}

	std::wstring get_path_by_index( size_t index ) const
	{
		return m_db.get_path_by_index( index );
	}

	bool is_directory_by_index( size_t index ) const
	{
		return m_db.is_directory_by_index( index );
	}

private:

	static bool query_journal_info( HANDLE volume, USN_JOURNAL_DATA_V0& out_data );
	void process_buffer( const std::vector<uint8_t>& buffer, DWORD size );

	UniqueHandle m_volume_handle;
	FileDatabase m_db;
};
