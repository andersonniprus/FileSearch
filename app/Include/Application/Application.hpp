#pragma once

/// @brief Core application controller that manages the main execution loop and services.
class Application
{
public:
	/// @brief Initializes the application and its core services.
	Application( );

	/// @brief Starts the application execution.
	/// @return Exit code of the application.
	[[nodiscard]] int run( ) const;

private:
	std::shared_ptr<class Searcher> searcher_;
};
