#include "json_loader.h"

namespace json_loader {

std::string ReadFileJSON(const std::filesystem::path& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file " + json_path.string());
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

json::value ParseJSON(const std::string& json_str) {
    try {
        return json::parse(json_str);
    } catch (const std::exception& e) {
        throw std::runtime_error("Invalid JSON: " + std::string(e.what()));
    }
}

model::Road LoadRoad(const json::object& road_obj) {
    auto x0_ptr = road_obj.if_contains("x0");
    auto y0_ptr = road_obj.if_contains("y0");
    if (!x0_ptr || !y0_ptr) {
        throw std::runtime_error("Invalid road format: missing x0 or y0");
    }
    model::Point start{
        x0_ptr->as_int64(),
        y0_ptr->as_int64()
    };
    auto x1_ptr = road_obj.if_contains("x1");
    auto y1_ptr = road_obj.if_contains("y1");
    if (x1_ptr && !y1_ptr) {
        model::Coord end_x = x1_ptr->as_int64();
        return model::Road(model::Road::HORIZONTAL, start, end_x);
    }
    if (y1_ptr && !x1_ptr) {
        model::Coord end_y = y1_ptr->as_int64();
        return model::Road(model::Road::VERTICAL, start, end_y);
    }
    throw std::runtime_error("Invalid road format");
}

model::Building LoadBuilding(const json::object& building_obj) {
    auto x_ptr = building_obj.if_contains("x");
    auto y_ptr = building_obj.if_contains("y");
    auto w_ptr = building_obj.if_contains("w");
    auto h_ptr = building_obj.if_contains("h");
    if (!x_ptr || !y_ptr || !w_ptr || !h_ptr) {
        throw std::runtime_error("Invalid building format: missing x, y, w or h");
    }
    model::Point pos{
        x_ptr->as_int64(),
        y_ptr->as_int64()
    };
    model::Size size{
        w_ptr->as_int64(),
        h_ptr->as_int64()
    };
    model::Rectangle rectangle{ pos, size };
    return model::Building{ rectangle };
}

model::Office LoadOffice(const json::object& office_obj) {
    auto id_ptr = office_obj.if_contains("id");
    auto x_ptr = office_obj.if_contains("x");
    auto y_ptr = office_obj.if_contains("y");
    auto offset_x_ptr = office_obj.if_contains("offsetX");
    auto offset_y_ptr = office_obj.if_contains("offsetY");

    if (!id_ptr || !x_ptr || !y_ptr || !offset_x_ptr || !offset_y_ptr) {
        throw std::runtime_error("Invalid office format: missing required fields");
    }

    std::string id_str = std::string(id_ptr->as_string().c_str());
    model::Office::Id id{id_str};
    model::Point pos{
        x_ptr->as_int64(),
        y_ptr->as_int64()
    };
    model::Offset offset{
        offset_x_ptr->as_int64(),
        offset_y_ptr->as_int64()
    };
    return model::Office(id, pos, offset);
}

model::Map LoadMap(const json::object& map_obj, double default_speed, extra_data::ExtraData& ex,
    int default_bag_capacity) {
    auto id_ptr = map_obj.if_contains("id");
    auto name_ptr = map_obj.if_contains("name");

    std::string id_str = std::string(id_ptr->as_string().c_str());
    std::string name = std::string(name_ptr->as_string().c_str());

    auto loot_types_ptr = map_obj.if_contains("lootTypes");
    if (!loot_types_ptr || !loot_types_ptr->is_array()) {
        throw std::runtime_error("Missing 'lootTypes' array in config");
    }
    const auto& loot_types = loot_types_ptr->as_array();
    std::vector<int> values;
    for (const auto& lt : loot_types) {
        auto& obj = lt.as_object();
        if (auto value_ptr = obj.if_contains("value")) {
            values.push_back(value_ptr->as_int64());
        }
        else {
            values.push_back(0);
        }
    }
    size_t k = loot_types.size();

    model::Map::Id id{ id_str };
    model::Map map(id, name, k);
    map.SetLootValues(values);
    double dog_speed = default_speed;
    if (auto speed_ptr = map_obj.if_contains("dogSpeed")) {
        dog_speed = speed_ptr->as_double();
    }
    map.SetDogSpeed(dog_speed);

    int map_bag_capacity = default_bag_capacity;
    if (auto capacity_ptr = map_obj.if_contains("bagCapacity")) {
        map_bag_capacity = capacity_ptr->as_int64();
    }
    
    map.SetBagCapacity(map_bag_capacity);
    if (auto roads_ptr = map_obj.if_contains("roads")) {
        if (roads_ptr->is_array()) {
            for (const auto& road_value : roads_ptr->as_array()) {
                map.AddRoad(LoadRoad(road_value.as_object()));
            }
        }
    }
    if (auto buildings_ptr = map_obj.if_contains("buildings")) {
        if (buildings_ptr->is_array()) {
            for (const auto& build_value : buildings_ptr->as_array()) {
                map.AddBuilding(LoadBuilding(build_value.as_object()));
            }
        }
    }
    if (auto offices_ptr = map_obj.if_contains("offices")) {
        if (offices_ptr->is_array()) {
            for (const auto& office_value : offices_ptr->as_array()) {
                map.AddOffice(LoadOffice(office_value.as_object()));
            }
        }
    }
    ex.AddLootType(id_str, loot_types);
    return map;
}

model::Game LoadGame(const std::filesystem::path& json_path, extra_data::ExtraData& ex) {
    std::string json_str = ReadFileJSON(json_path);
    auto value = ParseJSON(json_str);
    auto& root = value.as_object();

    auto cfg_ptr = root.if_contains("lootGeneratorConfig");
    if (!cfg_ptr || !cfg_ptr->is_object()) {
        throw std::runtime_error("Missing 'lootGeneratorConfig' object in config");
    }
    const auto& cfg = cfg_ptr->as_object();
    double period_sec = cfg.at("period").as_double();
    double probability = cfg.at("probability").as_double();
    auto period_ms = std::chrono::milliseconds{
    static_cast<int>(period_sec * 1000.0) };

    model::Game game{ period_ms, probability };
    double default_speed = 1.0;
    if (auto speed_ptr = root.if_contains("defaultDogSpeed")) {
        default_speed = speed_ptr->as_double();
    }
    game.SetDefaultDogSpeed(default_speed);
    int default_bag_capacity = 3;
    if (auto bag_ptr = root.if_contains("defaultBagCapacity")) {
        default_bag_capacity = bag_ptr->as_int64();
    }
    //время выкида
    auto default_rtr = std::chrono::milliseconds{
    static_cast<int>(60 * 1000.0) };
    if (auto rtr_ptr = root.if_contains("dogRetirementTime")) {
        default_rtr = std::chrono::milliseconds{
    static_cast<int>(rtr_ptr->as_double * 1000.0) };
    }
    game.SetRetirementTime(default_rtr);

    auto maps_ptr = root.if_contains("maps");
    if (!maps_ptr || !maps_ptr->is_array()) {
        throw std::runtime_error("Missing 'maps' array in config");
    }
    for (const auto& map_value : maps_ptr->as_array()) {
        game.AddMap(std::move(LoadMap(map_value.as_object(), default_speed, ex, default_bag_capacity)));
    }

    return game;
}

}  // namespace json_loader


