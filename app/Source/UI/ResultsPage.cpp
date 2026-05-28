#include "Stdafx.hpp"
#include "UI/ResultsPage.hpp"

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
		scan_complete_    = false;
		scan_start_time_  = std::chrono::steady_clock::now( );
		scan_duration_    = std::chrono::milliseconds( 0 );
		search_query_[ 0 ] = '\0';
		current_sort_col_  = Searcher::SortColumn::Name;
		current_sort_dir_  = Searcher::SortDirection::Ascending;

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
					scan_complete_ = true;
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
		if ( searcher_ )
		{
			const size_t file_count = searcher_->get_file_count( );
			const size_t dir_count  = searcher_->get_directory_count( );

			std::chrono::milliseconds current_duration = scan_duration_;
			if ( scan_in_progress_.load( ) )
			{
				auto now         = std::chrono::steady_clock::now( );
				current_duration = std::chrono::duration_cast<std::chrono::milliseconds>( now - scan_start_time_ );
			}

			const double seconds = current_duration.count( ) / 1000.0;

			ImGui::Text( "Files: %zu  |  Directories: %zu  |  Time: %.2f s",
			             file_count, dir_count, seconds );
		}

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

		ImGui::Text( "Total entries: %zu", total_entries_ );

		ImGui::SameLine( );
		ImGui::SetNextItemWidth( -FLT_MIN );

		bool is_scanning = scan_in_progress_.load( );
		if ( is_scanning ) ImGui::BeginDisabled( );
		if ( ImGui::InputTextWithHint( "##search", "Search files...", search_query_, sizeof( search_query_ ) ) )
		{
			if ( searcher_ && !is_scanning )
			{
				std::string sq( search_query_ );
				searcher_->apply_filter_and_sort( std::wstring( sq.begin( ), sq.end( ) ), current_sort_col_, current_sort_dir_ );
				update_data( );
			}
		}
		if ( is_scanning ) ImGui::EndDisabled( );

		const float remaining_height = ImGui::GetContentRegionAvail( ).y;

		if ( !searcher_ || total_entries_ == 0 || is_scanning )
		{
			return;
		}

		if ( ImGui::BeginTable( "ResultsTable", 3, flags, { 0.0f, remaining_height } ) )
		{
			ImGui::TableSetupScrollFreeze( 0, 1 );
			ImGui::TableSetupColumn( "Name", ImGuiTableColumnFlags_DefaultSort );
			ImGui::TableSetupColumn( "Path" );
			ImGui::TableSetupColumn( "Type", ImGuiTableColumnFlags_WidthFixed, 50.0f );
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
						else if ( spec.ColumnIndex == 2 ) col = Searcher::SortColumn::Type;

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

					std::wstring_view name = searcher_->get_name_by_index( static_cast<size_t>( i ) );
					std::wstring full_path = searcher_->get_path_by_index( static_cast<size_t>( i ) );
					bool is_directory      = searcher_->is_directory_by_index( static_cast<size_t>( i ) );

					if ( full_path.empty( ) )
					{
						continue;
					}

					ImGui::TableNextRow( );

					ImGui::TableSetColumnIndex( 0 );
					std::string name_utf8( name.begin( ), name.end( ) );
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
					ImGui::Text( is_directory ? "DIR" : "FILE" );
				}
			}
			ImGui::EndTable( );
		}
	}
}
