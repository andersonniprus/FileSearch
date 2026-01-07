#pragma once

#include "Searcher/Searcher.hpp"

namespace UI
{
	struct DisplayEntry
	{
		std::wstring name;
		std::wstring path;
		std::wstring size_str;
		bool is_directory;
	};

	class ResultsPage
	{
	public:
		ResultsPage( );
		~ResultsPage( );

		void set_searcher( const std::shared_ptr<Searcher>& searcher );
		void render( );

	private:
		void start_scan( char drive_letter );
		void update_data( );

		ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
		                        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
		                        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
		                        ImGuiTableFlags_Hideable;

		std::shared_ptr<Searcher> searcher_;
		size_t total_entries_ = 0;

		std::atomic<bool> scan_in_progress_ = false;
		std::atomic<bool> scan_complete_    = false;
		std::thread scan_thread_;
		std::chrono::steady_clock::time_point scan_start_time_;
		std::chrono::milliseconds scan_duration_ { 0 };
		char drive_letter_ = 'C';
	};
}
