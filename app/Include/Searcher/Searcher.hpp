#pragma once

/// @brief Orchestrates raw disk I/O to parse the NTFS Master File Table (MFT).
class Searcher
{
public:
	using SortColumn = FileDatabase::SortColumn;
	using SortDirection = FileDatabase::SortDirection;

	Searcher( );
	~Searcher( ) = default;

	/// @brief Performs a full raw MFT scan of the specified volume.
	/// @param drive_letter The drive letter (e.g., 'C').
	/// @return True if scan completed successfully, false if access denied or failed.
	bool scan( char drive_letter );

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
	[[nodiscard]] std::wstring_view get_name_by_index( size_t index ) const noexcept
	{
		return m_db.get_name_by_index( index );
	}

	/// @brief Resolves path for UI virtualization.
	[[nodiscard]] std::wstring get_path_by_index( size_t index ) const
	{
		return m_db.get_path_by_index( index );
	}

	/// @brief Checks directory status for UI virtualization.
	[[nodiscard]] bool is_directory_by_index( size_t index ) const noexcept
	{
		return m_db.is_directory_by_index( index );
	}

	/// @brief Gets file size for UI virtualization.
	[[nodiscard]] uint64_t get_size_by_index( size_t index ) const noexcept
	{
		return m_db.get_size_by_index( index );
	}

	/// @brief Gets timestamp for UI virtualization.
	[[nodiscard]] uint64_t get_timestamp_by_index( size_t index ) const noexcept
	{
		return m_db.get_timestamp_by_index( index );
	}

	/// @brief Gets estimated memory usage of the underlying database.
	[[nodiscard]] size_t get_memory_usage( ) const noexcept
	{
		return m_db.get_memory_usage( );
	}

private:
	struct DataRun
	{
		uint64_t logical_cluster;
		uint64_t cluster_count;
	};

	/// @brief Parses the VBR to initialize NTFS geometry.
	[[nodiscard]] bool parse_boot_sector( HANDLE volume );

	/// @brief Reads the $MFT record and decodes its data runs.
	[[nodiscard]] bool extract_mft_runs( HANDLE volume );

	/// @brief Decodes a variable-length integer from a runlist.
	[[nodiscard]] static int64_t decode_run_offset( const uint8_t* runlist, uint8_t offset_size ) noexcept;
	[[nodiscard]] static uint64_t decode_run_length( const uint8_t* runlist, uint8_t length_size ) noexcept;

	/// @brief Parses a raw memory buffer containing multiple MFT records.
	void process_mft_buffer( const uint8_t* buffer, size_t size );

	UniqueHandle m_volume_handle;
	FileDatabase m_db;

	uint32_t m_bytes_per_cluster = 0;
	uint32_t m_bytes_per_record  = 0;
	uint64_t m_mft_start_cluster = 0;
	std::vector<DataRun> m_mft_runs;
	};
