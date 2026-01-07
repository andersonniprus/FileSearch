#include "Stdafx.hpp"
#include "UI/ResultsPage.hpp"

namespace UI
{
	ResultsPage::ResultsPage( )
	{
	}

	ResultsPage::~ResultsPage( )
	{
		if ( scan_thread_.joinable( ) )
		{
			scan_thread_.join( );
		}
	}

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

		if ( scan_thread_.joinable( ) )
		{
			scan_thread_.join( );
		}

		total_entries_    = 0;
		drive_letter_     = drive_letter;
		scan_in_progress_ = true;
		scan_complete_    = false;
		scan_start_time_  = std::chrono::steady_clock::now( );
		scan_duration_    = std::chrono::milliseconds( 0 );

		scan_thread_ = std::thread( [this, drive_letter]
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
		char drive_buf[ 2 ] = { drive_letter_, '\0' };
		if ( ImGui::InputText( "##drive", drive_buf, 2, ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank ) )
		{
			if ( drive_buf[ 0 ] >= 'A' && drive_buf[ 0 ] <= 'Z' )
			{
				drive_letter_ = drive_buf[ 0 ];
			}
		}

		ImGui::Separator( );

		ImGui::Text( "Total entries: %zu", total_entries_ );

		const float remaining_height = ImGui::GetContentRegionAvail( ).y;


		if ( !searcher_ || total_entries_ == 0 || scan_in_progress_.load( ) )
		{
			return;
		}

		if ( ImGui::BeginTable( "ResultsTable", 3, flags, { 0.0f, remaining_height } ) )
		{
			ImGui::TableSetupScrollFreeze( 0, 1 );
			ImGui::TableSetupColumn( "Name" );
			ImGui::TableSetupColumn( "Path" );
			ImGui::TableSetupColumn( "Type", ImGuiTableColumnFlags_WidthFixed, 50.0f );
			ImGui::TableHeadersRow( );

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

					std::wstring full_path = searcher_->get_path_by_index( static_cast<size_t>( i ) );
					bool is_directory      = searcher_->is_directory_by_index( static_cast<size_t>( i ) );

					if ( full_path.empty( ) )
					{
						continue;
					}

					auto pos          = full_path.find_last_of( L"\\" );
					std::wstring name = ( pos != std::wstring::npos ) ? full_path.substr( pos + 1 ) : full_path;

					ImGui::TableNextRow( );

					ImGui::TableSetColumnIndex( 0 );
					std::string name_utf8( name.begin( ), name.end( ) );
					ImGui::TextUnformatted( name_utf8.c_str( ) );

					ImGui::TableSetColumnIndex( 1 );
					std::string path_utf8( full_path.begin( ), full_path.end( ) );
					ImGui::TextUnformatted( path_utf8.c_str( ) );

					ImGui::TableSetColumnIndex( 2 );
					ImGui::Text( is_directory ? "DIR" : "FILE" );
				}
			}
			ImGui::EndTable( );
		}
	}
}
