#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>
#include "model.h"
#include "extra_data.h"
#include <boost/json.hpp> 
namespace json = boost::json;
namespace json_loader {

	std::string ReadFileJSON(const std::filesystem::path& json_path);
	json::value ParseJSON(const std::string& json_str);
	model::Road LoadRoad(const json::object& road_obj);
	model::Building LoadBuilding(const json::object& building_obj);
	model::Office LoadOffice(const json::object& office_obj);
	model::Map LoadMap(const json::object& map_obj, double default_speed, extra_data::ExtraData& ex,
		int default_bag_capacity);
	model::Game LoadGame(const std::filesystem::path& json_path, extra_data::ExtraData& ex);
	
}  // namespace json_loader




