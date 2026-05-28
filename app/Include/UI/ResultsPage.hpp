#pragma once

namespace UI
{
	/// @brief Container for entry data displayed in the UI table.
	struct DisplayEntry
	{
		std::wstring name;
		std::wstring path;
		std::wstring size_str;
		bool is_directory;
	};

	/// @brief The main UI view for triggering scans and displaying results.
	class ResultsPage
	{
	public:
		ResultsPage( );
		~ResultsPage( );

		/// @brief Injects the searcher service into the page.
		void set_searcher( const std::shared_ptr<Searcher>& searcher );

		/// @brief Renders the page contents using ImGui.
		void render( );

	private:
		/// @brief Spawns the background scan thread.
		void start_scan( char drive_letter );

		/// @brief Updates entry counters from the searcher.
		void update_data( );

		ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
		                        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
		                        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
		                        ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable;

		std::shared_ptr<Searcher> searcher_;
		size_t total_entries_ = 0;

		std::atomic<bool> scan_in_progress_ = false;
		std::atomic<bool> scan_complete_    = false;
		std::jthread scan_thread_;
		std::chrono::steady_clock::time_point scan_start_time_;
		std::chrono::milliseconds scan_duration_ { 0 };
		char drive_letter_ = 'C';
		std::vector<std::string> available_drives_;
		int selected_drive_idx_ = 0;
		char search_query_[ 256 ] = "";
		Searcher::SortColumn current_sort_col_ = Searcher::SortColumn::Name;
		Searcher::SortDirection current_sort_dir_ = Searcher::SortDirection::Ascending;
	};
}
