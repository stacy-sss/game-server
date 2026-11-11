#pragma once
#include <unordered_map>
#include <string>
#include <boost/json.hpp> 
namespace json = boost::json;
namespace extra_data {

	class ExtraData {
	public:
		void AddLootType(const std::string& map_id, json::array types) {
			loot_array_[map_id] = std::move(types);
		}
		const json::array& GetLootArray(const std::string& map_id) const {
			return loot_array_.at(map_id);
		}
	private:
		std::unordered_map<std::string, json::array> loot_array_;
	};
}
