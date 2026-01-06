#pragma once

class Application
{
public:
	Application( );

	[[nodiscard]] int run( ) const;

	template<typename T>
	void register_service( std::shared_ptr<T> service )
	{
		services_[ std::type_index( typeid( T ) ) ] = std::move( service );
	}

	template<typename T>
	[[nodiscard]] std::shared_ptr<T> get_service( ) const
	{
		if ( const auto it = services_.find( std::type_index( typeid( T ) ) ); it != services_.end( ) )
		{
			return std::any_cast<std::shared_ptr<T>>( it->second );
		}

		throw std::runtime_error( "Service requested but not found in Application context." );
	}

private:
	std::unordered_map<std::type_index, std::any> services_;
};
