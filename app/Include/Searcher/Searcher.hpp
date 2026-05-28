#pragma once

/// @brief Orchestrates NTFS USN Journal scanning and database population.
class Searcher
{
public:
	using SortColumn = FileDatabase::SortColumn;
	using SortDirection = FileDatabase::SortDirection;

	Searcher( );
	~Searcher( ) = default;

	/// @brief Performs a full scan of the specified volume.
	/// @param drive_letter The drive letter (e.g., 'C').
	/// @return True if scan completed successfully.
	bool scan( char drive_letter );

	/// @brief High-level iteration over all files in the database.
	template<std::invocable<const std::wstring&> F>
	void for_each_file( F&& callback )
	{
		m_db.visit_all_paths( std::forward<F>( callback ) );
	}

	/// @brief Applies a text filter and sort to the database results.
	void apply_filter_and_sort( const std::wstring_view query, SortColumn col, SortDirection dir )
	{
		m_db.apply_filter_and_sort( query, col, dir );
	}

	/// @brief Direct access to file count.
	[[nodiscard]] size_t get_file_count( ) const noexcept
	{
		return m_db.get_file_count( );
	}

	/// @brief Direct access to directory count.
	[[nodiscard]] size_t get_directory_count( ) const noexcept
	{
		return m_db.get_directory_count( );
	}

	/// @brief Direct access to total entries.
	[[nodiscard]] size_t get_total_entries( ) const noexcept
	{
		return m_db.get_total_entries( );
	}

	/// @brief Resolves name for UI virtualization without computing full path.
	[[nodiscard]] std::wstring_view get_name_by_index( size_t index ) const
	{
		return m_db.get_name_by_index( index );
	}

	/// @brief Resolves path for UI virtualization.
	[[nodiscard]] std::wstring get_path_by_index( size_t index ) const
	{
		return m_db.get_path_by_index( index );
	}

	/// @brief Checks directory status for UI virtualization.
	[[nodiscard]] bool is_directory_by_index( size_t index ) const
	{
		return m_db.is_directory_by_index( index );
	}

private:
	/// @brief Queries metadata about the USN Journal.
	static bool query_journal_info( HANDLE volume, USN_JOURNAL_DATA_V0& out_data );

	/// @brief Parses a raw USN data buffer and inserts records into the database.
	void process_buffer( const uint8_t* buffer, DWORD size );

	UniqueHandle m_volume_handle;
	FileDatabase m_db;
};
