#include "Stdafx.hpp"

namespace UI
{
	ResultsPage::ResultsPage( )
	{
		DWORD drives_bitmask = GetLogicalDrives( );
		for ( int i = 0; i < 26; ++i )
		{
			if ( drives_bitmask & ( 1 << i ) )
			{
				char letter = static_cast<char>( 'A' + i );
				available_drives_.push_back( std::string( 1, letter ) + ":" );
				
				if ( letter == 'C' )
				{
					selected_drive_idx_ = static_cast<int>( available_drives_.size( ) - 1 );
					drive_letter_       = 'C';
				}
			}
		}

		if ( available_drives_.empty( ) )
		{
			available_drives_.push_back( "C:" );
		}
	}

	ResultsPage::~ResultsPage( ) = default;

	void ResultsPage::set_searcher( const std::shared_ptr<Searcher>& searcher )
	{
		searcher_ = searcher;
	}

	void ResultsPage::start_scan( char drive_letter )
	{
		if ( scan_in_progress_.load( ) )
		{
			return;
		}

		total_entries_    = 0;
		drive_letter_     = drive_letter;
		scan_in_progress_ = true;
		scan_start_time_  = std::chrono::steady_clock::now( );
		scan_duration_    = std::chrono::milliseconds( 0 );
		search_query_[ 0 ] = '\0';
		current_sort_col_  = Searcher::SortColumn::Date;
		current_sort_dir_  = Searcher::SortDirection::Descending;

		if ( searcher_ )
		{
			searcher_->apply_filter_and_sort( L"", current_sort_col_, current_sort_dir_ );
		}

		scan_thread_ = std::jthread( [this, drive_letter]
		{
			try
			{
				if ( searcher_ && searcher_->scan( drive_letter ) )
				{
				}
			}
			catch ( const std::exception& e )
			{
				LOG( "Exception in scan thread: {}", e.what( ) );
			}

			scan_in_progress_ = false;
			auto end_time     = std::chrono::steady_clock::now( );
			scan_duration_    = std::chrono::duration_cast<std::chrono::milliseconds>( end_time - scan_start_time_ );

			update_data( );
		} );
	}

	void ResultsPage::update_data( )
	{
		if ( !searcher_ ) return;

		total_entries_ = searcher_->get_total_entries( );
	}

	void ResultsPage::render( )
	{
		ImGui::Separator( );

		if ( scan_in_progress_.load( ) )
		{
			ImGui::BeginDisabled( );
			ImGui::Button( "Scanning..." );
			ImGui::EndDisabled( );
		}
		else
		{
			if ( ImGui::Button( "Start Scan" ) )
			{
				start_scan( drive_letter_ );
			}
		}

		ImGui::SameLine( );
		ImGui::Text( "Drive:" );
		ImGui::SameLine( );
		ImGui::SetNextItemWidth( 60.0f );
		
		if ( ImGui::BeginCombo( "##drive", available_drives_[ selected_drive_idx_ ].c_str( ) ) )
		{
			for ( int i = 0; i < static_cast<int>( available_drives_.size( ) ); ++i )
			{
				const bool is_selected = ( selected_drive_idx_ == i );
				if ( ImGui::Selectable( available_drives_[ i ].c_str( ), is_selected ) )
				{
					selected_drive_idx_ = i;
					drive_letter_       = available_drives_[ i ][ 0 ];
				}

				if ( is_selected )
				{
					ImGui::SetItemDefaultFocus( );
				}
			}
			ImGui::EndCombo( );
		}

		ImGui::Separator( );

		ImGui::Text( "Search:" );
		ImGui::SameLine( );
		ImGui::SetNextItemWidth( -FLT_MIN );

		bool is_scanning = scan_in_progress_.load( );
		if ( is_scanning ) ImGui::BeginDisabled( );
		if ( ImGui::InputTextWithHint( "##search", "Type file name...", search_query_, sizeof( search_query_ ) ) )
		{
			if ( searcher_ && !is_scanning )
			{
				std::string sq( search_query_ );
				searcher_->apply_filter_and_sort( std::wstring( sq.begin( ), sq.end( ) ), current_sort_col_, current_sort_dir_ );
				update_data( );
			}
		}
		if ( is_scanning ) ImGui::EndDisabled( );

		const float footer_height = ImGui::GetFrameHeightWithSpacing( ) + ImGui::GetStyle( ).ItemSpacing.y;
		const float table_height  = ImGui::GetContentRegionAvail( ).y - footer_height;

		if ( !searcher_ || ( total_entries_ == 0 && !is_scanning ) )
		{
			ImGui::Dummy( { 0, table_height } );
		}
		else if ( is_scanning )
		{
			ImGui::SetCursorPosY( ImGui::GetCursorPosY( ) + table_height / 2.0f );
			ImGui::Text( "Scanning volume... please wait." );
			ImGui::SetCursorPosY( ImGui::GetCursorPosY( ) + table_height / 2.0f );
		}
		else
		{
			if ( ImGui::BeginTable( "ResultsTable", 4, flags, { 0.0f, table_height } ) )
			{
				ImGui::TableSetupScrollFreeze( 0, 1 );
				ImGui::TableSetupColumn( "Name" );
				ImGui::TableSetupColumn( "Path" );
				ImGui::TableSetupColumn( "Size", ImGuiTableColumnFlags_WidthFixed, 80.0f );
				ImGui::TableSetupColumn( "Date", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_WidthFixed, 120.0f );
				ImGui::TableHeadersRow( );

				if ( ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs( ) )
				{
					if ( sorts_specs->SpecsDirty )
					{
						Searcher::SortColumn col        = Searcher::SortColumn::None;
						Searcher::SortDirection dir     = Searcher::SortDirection::Ascending;

						if ( sorts_specs->SpecsCount > 0 )
						{
							const auto& spec = sorts_specs->Specs[ 0 ];
							if ( spec.ColumnIndex == 0 ) col = Searcher::SortColumn::Name;
							else if ( spec.ColumnIndex == 1 ) col = Searcher::SortColumn::Path;
							else if ( spec.ColumnIndex == 2 ) col = Searcher::SortColumn::Size;
							else if ( spec.ColumnIndex == 3 ) col = Searcher::SortColumn::Date;

							dir = ( spec.SortDirection == ImGuiSortDirection_Ascending ) ? Searcher::SortDirection::Ascending : Searcher::SortDirection::Descending;
						}

						if ( col != current_sort_col_ || dir != current_sort_dir_ )
						{
							current_sort_col_ = col;
							current_sort_dir_ = dir;

							if ( searcher_ && !is_scanning )
							{
								std::string sq( search_query_ );
								searcher_->apply_filter_and_sort( std::wstring( sq.begin( ), sq.end( ) ), current_sort_col_, current_sort_dir_ );
								update_data( );
							}
						}
						sorts_specs->SpecsDirty = false;
					}
				}

				ImGuiListClipper clipper;
				clipper.Begin( static_cast<int>( total_entries_ ) );

				while ( clipper.Step( ) )
				{
					for ( int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++ )
					{
						if ( scan_in_progress_.load( ) )
						{
							break;
						}

						std::wstring_view name_wide = searcher_->get_name_by_index( static_cast<size_t>( i ) );
						std::wstring full_path      = searcher_->get_path_by_index( static_cast<size_t>( i ) );
						uint64_t size               = searcher_->get_size_by_index( static_cast<size_t>( i ) );
						uint64_t timestamp          = searcher_->get_timestamp_by_index( static_cast<size_t>( i ) );
						bool is_directory           = searcher_->is_directory_by_index( static_cast<size_t>( i ) );

						if ( full_path.empty( ) )
						{
							continue;
						}

						char size_buf[ 32 ] = "";
						if ( !is_directory )
						{
							const char* units[] = { "B", "KB", "MB", "GB", "TB" };
							int unit_idx = 0;
							double display_size = static_cast<double>( size );
							while ( display_size >= 1024.0 && unit_idx < 4 )
							{
								display_size /= 1024.0;
								unit_idx++;
							}
							snprintf( size_buf, sizeof( size_buf ), "%.2f %s", display_size, units[ unit_idx ] );
						}

						char date_buf[ 64 ] = "";
						if ( timestamp > 0 )
						{
							FILETIME ft;
							ft.dwLowDateTime  = static_cast<DWORD>( timestamp & 0xFFFFFFFF );
							ft.dwHighDateTime = static_cast<DWORD>( timestamp >> 32 );
							SYSTEMTIME st_utc, st_local;
							if ( FileTimeToSystemTime( &ft, &st_utc ) && SystemTimeToTzSpecificLocalTime( nullptr, &st_utc, &st_local ) )
							{
								snprintf( date_buf, sizeof( date_buf ), "%04d-%02d-%02d %02d:%02d",
										  st_local.wYear, st_local.wMonth, st_local.wDay,
										  st_local.wHour, st_local.wMinute );
							}
						}

						ImGui::TableNextRow( );

						ImGui::TableSetColumnIndex( 0 );
						std::string name_utf8( name_wide.begin( ), name_wide.end( ) );
						std::string path_utf8( full_path.begin( ), full_path.end( ) );

						ImGui::PushID( i );
						if ( ImGui::Selectable( name_utf8.c_str( ), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap ) )
						{
							ShellExecuteW( nullptr, L"open", full_path.c_str( ), nullptr, nullptr, SW_SHOWDEFAULT );
						}

						if ( ImGui::BeginPopupContextItem( "ContextMenu" ) )
						{
							if ( ImGui::MenuItem( "Open File Location" ) )
							{
								auto pos = full_path.find_last_of( L"\\" );
								std::wstring parent_dir = ( pos != std::wstring::npos ) ? full_path.substr( 0, pos ) : full_path;
								ShellExecuteW( nullptr, L"open", parent_dir.c_str( ), nullptr, nullptr, SW_SHOWDEFAULT );
							}

							if ( ImGui::MenuItem( "Copy Full Path" ) )
							{
								ImGui::SetClipboardText( path_utf8.c_str( ) );
							}

							if ( ImGui::MenuItem( "Execute" ) )
							{
								ShellExecuteW( nullptr, L"open", full_path.c_str( ), nullptr, nullptr, SW_SHOWDEFAULT );
							}

							ImGui::EndPopup( );
						}
						ImGui::PopID( );

						ImGui::TableSetColumnIndex( 1 );
						ImGui::TextUnformatted( path_utf8.c_str( ) );

						ImGui::TableSetColumnIndex( 2 );
						ImGui::TextUnformatted( size_buf );

						ImGui::TableSetColumnIndex( 3 );
						ImGui::TextUnformatted( date_buf );
					}
				}
				ImGui::EndTable( );
			}
		}

		// --- Footer / Status Bar ---
		ImGui::Separator( );
		
		if ( searcher_ )
		{
			size_t mem_bytes = searcher_->get_memory_usage( );
			double mem_mb = mem_bytes / ( 1024.0 * 1024.0 );

			std::chrono::milliseconds duration = scan_duration_;
			if ( scan_in_progress_.load( ) )
			{
				duration = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::steady_clock::now( ) - scan_start_time_ );
			}

			const size_t file_count = searcher_->get_file_count( );
			const size_t dir_count  = searcher_->get_directory_count( );
			const size_t total = searcher_->get_total_entries( );

			ImGui::Text( "Files: %zu | Directories: %zu | Total: %zu | Scan: %.3fs | RAM: %.1f MB", 
				file_count, dir_count, total, duration.count() / 1000.0, mem_mb );
		}
		else
		{
			ImGui::Text( "Engine not initialized." );
		}
	}
}
