#pragma once
#include "FileDatabase.hpp"

class Searcher
{
public:
	Searcher( );
	~Searcher( ) = default;

	bool scan( char drive_letter );

private:
	static bool query_journal_info( HANDLE volume, USN_JOURNAL_DATA_V0& out_data );
	void process_buffer( const std::vector<uint8_t>& buffer, DWORD size );

	UniqueHandle m_volume_handle;
	FileDatabase m_db;
};
