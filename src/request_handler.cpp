#include "request_handler.h"

namespace http_handler {

//---------------------RequestHandler----------------------
http::response<http::string_body> RequestHandler::HandleMapsReq() {
    http::response<http::string_body> response(http::status::ok, 11);
    response.set(http::field::content_type, "application/json");
    response.set(http::field::cache_control, "no-cache");

    json::array arr;
    for (const auto& map : game_.GetMaps()) {
        json::object obj;
        obj[json_keys::ID] = *map.GetId();
        obj[json_keys::NAME] = map.GetName();
        arr.push_back(obj);
    }

    response.body() = json::serialize(arr);
    response.content_length(response.body().size());
    response.keep_alive(true);
    return response;
}

json::array RequestHandler::SerializeRoads(const model::Map& map) const {
    json::array roads;
    for (const auto& road : map.GetRoads()) {
        json::object road_obj;
        auto start = road.GetStart();
        auto end = road.GetEnd();
        if (road.IsHorizontal()) {
            road_obj[json_keys::X0] = start.x;
            road_obj[json_keys::Y0] = start.y;
            road_obj[json_keys::X1] = end.x;
        } else {
            road_obj[json_keys::X0] = start.x;
            road_obj[json_keys::Y0] = start.y;
            road_obj[json_keys::Y1] = end.y;
        }
        roads.push_back(road_obj);
    }
    return roads;
}

json::array RequestHandler::SerializeBuildings(const model::Map& map) const {
    json::array buildings;
    for (const auto& building : map.GetBuildings()) {
        json::object building_obj;
        auto bounds = building.GetBounds();
        building_obj[json_keys::X] = bounds.position.x;
        building_obj[json_keys::Y] = bounds.position.y;
        building_obj[json_keys::W] = bounds.size.width;
        building_obj[json_keys::H] = bounds.size.height;
        buildings.push_back(building_obj);
    }
    return buildings;
}

json::array RequestHandler::SerializeOffices(const model::Map& map) const {
    json::array offices;
    for (const auto& office : map.GetOffices()) {
        json::object office_obj;
        office_obj[json_keys::ID] = *office.GetId();
        office_obj[json_keys::X] = office.GetPosition().x;
        office_obj[json_keys::Y] = office.GetPosition().y;
        office_obj[json_keys::OFFSET_X] = office.GetOffset().dx;
        office_obj[json_keys::OFFSET_Y] = office.GetOffset().dy;
        offices.push_back(office_obj);
    }
    return offices;
}

json::object RequestHandler::SerializeMapInfo(const model::Map& map) const {
    json::object obj;
    obj[json_keys::ID] = *map.GetId();
    obj[json_keys::NAME] = map.GetName();
    return obj;
}

http::response<http::string_body> RequestHandler::HandleMapReq(const std::string& id) {
    auto map = game_.FindMap(model::Map::Id{id});
    if (!map) {
        return MakeErrorResponse(http::status::not_found, "mapNotFound", "Map not found");
    }
    //все данные из extra_data
    const auto& ex_data = ex_.GetLootArray(id);
    json::object obj = SerializeMapInfo(*map);
    obj[json_keys::ROADS] = SerializeRoads(*map);
    obj[json_keys::BUILDINGS] = SerializeBuildings(*map);
    obj[json_keys::OFFICES] = SerializeOffices(*map);
    obj[json_keys::LOOT] = ex_data;

    http::response<http::string_body> response(http::status::ok, 11);
    response.set(http::field::content_type, "application/json");
    response.set(http::field::cache_control, "no-cache");
    response.body() = json::serialize(obj);
    response.content_length(response.body().size());
    response.keep_alive(true);
    return response;
}
std::string RequestHandler::UrlDecode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.size(); i++) {
        if (str[i] == '%' && i + 2 < str.size()) {
            int value = std::stoi(str.substr(i + 1, 2), nullptr, 16);
            result += static_cast<char>(value);
            i += 2;
        }
        else if (str[i] == '+') {
            result += ' ';
        }
        else {
            result += str[i];
        }
    }
    return result;
}

std::string RequestHandler::GetMimeType(const std::string& ext) {
    static const std::unordered_map<std::string, std::string> mime_types = {
        {".html", "text/html"}, {".css", "text/css"}, {".js", "text/javascript"},
        {".png", "image/png"}, {".jpg", "image/jpeg"}, {".jpe", "image/jpeg"},
        {".jpeg", "image/jpeg"}, {".gif", "image/gif"}, {".bmp", "image/bmp"},
        {".ico", "image/vnd.microsoft.icon"}, {".tiff", "image/tiff"},
        {".tif", "image/tiff"}, {".svg", "image/svg+xml"}, {".svgz", "image/svg+xml"},
        {".mp3", "audio/mpeg"}
    };
    std::string ext_lower = ext;
    for (char& c : ext_lower) {
        c = std::tolower(static_cast<unsigned char>(c));
    }

    auto it = mime_types.find(ext_lower);
    return it != mime_types.end() ? it->second : "application/octet-stream";
}
bool RequestHandler::IsPathUnderRoot(const std::filesystem::path& path, const std::filesystem::path& root) {
    auto relative = std::filesystem::relative(path, root);
    return !relative.empty() && relative.string().find("..") == std::string::npos;
}
http::response<http::string_body> RequestHandler::HandleStaticRequest(const std::string& target) {
    std::string decoded = UrlDecode(target);
    if (!decoded.empty() && decoded[0] == '/') {
        decoded = decoded.substr(1);
    }
    if (decoded.empty()) {
        decoded = "index.html";
    }
    std::filesystem::path full_path = static_dir_ / decoded;
    if (!IsPathUnderRoot(full_path, static_dir_)) {
        return MakeTextErrorResponse(http::status::bad_request, "Bad Request");

    }
    //файл существует
    if (!std::filesystem::exists(full_path)) {
        return MakeTextErrorResponse(http::status::not_found, "Not Found");
    }

    if (std::filesystem::is_directory(full_path)) {
        full_path = full_path / "index.html";
        if (!std::filesystem::exists(full_path)) {
            return MakeTextErrorResponse(http::status::not_found, "Not Found");;
        }
    }
    std::ifstream file(full_path, std::ios::binary);
    if (!file.is_open()) {
        return MakeTextErrorResponse(http::status::not_found, "Not Found");
    }
    std::string content((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    http::response<http::string_body> response(http::status::ok, 11);
    response.set(http::field::content_type, GetMimeType(full_path.extension().string()));
    response.body() = content;
    response.content_length(response.body().size());
    response.keep_alive(true);
    response.version(11);
    return response;
}

http::response<http::string_body> MakeErrorResponse(http::status status,
    const std::string& code, const std::string& message) {
    http::response<http::string_body> response(status, 11);
    response.set(http::field::content_type, "application/json");
    response.set(http::field::cache_control, "no-cache");
    json::object obj;
    obj[json_keys::CODE] = code;
    obj[json_keys::MESSAGE] = message;
    response.body() = json::serialize(obj);
    response.content_length(response.body().size());
    response.keep_alive(true);
    return response;
}
http::response<http::string_body> RequestHandler::MakeTextErrorResponse(http::status status,
    const std::string& message) const {
    http::response<http::string_body> response(status, 11);
    response.set(http::field::content_type, "text/plain");
    response.body() = message;
    response.content_length(response.body().size());
    response.keep_alive(true);
    return response;
}
//---------------------ApiHandler----------------------
template <typename Fn>
http::response<http::string_body> ApiHandler::ExecuteAuthorized(const http::request<http::string_body>& req,
    application::Application& app, Fn&& action) {
    auto auth_header = req[http::field::authorization];
    if (auth_header.empty()) {
        return MakeErrorResponse(http::status::unauthorized,
            "invalidToken", "Authorization header is missing");
    }
    std::string auth_value = std::string(auth_header);
    const std::string prefix = "Bearer ";
    if (auth_value.size() < prefix.size() ||
        auth_value.compare(0, prefix.size(), prefix) != 0) {
        return MakeErrorResponse(http::status::unauthorized,
            "invalidToken", "Invalid authorization header format");
    }
    std::string token = auth_value.substr(prefix.size());
    bool is_valid_format = (token.size() == 32 &&
        token.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos);
    const auto* current_player = app.GetPlayers().FindByToken(token);
    if (!current_player) {
        if (is_valid_format) {
            return MakeErrorResponse(http::status::unauthorized,
                "unknownToken", "Player token has not been found");
        }
        return MakeErrorResponse(http::status::unauthorized,
            "invalidToken", "Invalid token format");
    }
    return action(current_player, token);
}
http::response<http::string_body> ApiHandler::HandleJoinReq(const http::request<http::string_body>& req) {
    try {
        auto json_body = json::parse(req.body());
        auto obj = json_body.as_object();

        if (!obj.contains("userName") || !obj.contains("mapId")) {
            return MakeErrorResponse(http::status::bad_request,
                "invalidArgument", "Missing required fields: userName and mapId");
        }
        std::string player_name = json::value_to<std::string>(obj.at("userName"));
        std::string map_id = json::value_to<std::string>(obj.at("mapId"));

        if (player_name.empty()) {
            return MakeErrorResponse(http::status::bad_request,
                "invalidArgument", "Invalid name");
        }
        auto result = app_.JoinGame(player_name, map_id);
        if (!result.success) {
            if (result.error_code == "mapNotFound") {
                return MakeErrorResponse(http::status::not_found,
                    result.error_code, result.error_message);
            }
            return MakeErrorResponse(http::status::bad_request,
                result.error_code, result.error_message);
        }

        json::object join_obj;
        join_obj["authToken"] = result.token;
        join_obj["playerId"] = result.player_id;

        http::response<http::string_body> response(http::status::ok, 11);
        response.set(http::field::content_type, "application/json");
        response.set(http::field::cache_control, "no-cache");
        response.body() = json::serialize(join_obj);
        response.content_length(response.body().size());
        response.keep_alive(true);
        return response;

    }
    catch (const json::system_error& e) {
        return MakeErrorResponse(http::status::bad_request,
            "invalidArgument", "Join game request parse error");
    }
    catch (const std::exception& e) {
        return MakeErrorResponse(http::status::bad_request,
            "invalidArgument", "Join game request parse error");
    }
}
http::response<http::string_body> ApiHandler::HandlePlayersReq(const http::request<http::string_body>& req) {
    return ExecuteAuthorized(req, app_, [&](const auto* player, const std::string&) {
        auto players_info = app_.GetPlayers().GetPlayersOnMap(player->GetMapId());

        json::object response_obj;
        for (const auto& info : players_info) {
            json::object player_obj;
            player_obj["name"] = info.name;
            response_obj[std::to_string(info.id)] = player_obj;
        }

        http::response<http::string_body> response(http::status::ok, 11);
        response.set(http::field::content_type, "application/json");
        response.set(http::field::cache_control, "no-cache");
        response.body() = json::serialize(response_obj);
        response.content_length(response.body().size());
        response.keep_alive(true);
        return response;
    });   
}

http::response<http::string_body> ApiHandler::HandleStateReq(const http::request<http::string_body>& req) {
    
    return ExecuteAuthorized(req, app_, [&](const auto* current_player, const std::string&) {
        auto players_info = app_.GetPlayers().GetPlayersOnMap(current_player->GetMapId());

        auto* session = app_.GetGame().FindSession(model::Map::Id{ current_player->GetMapId() });
        if (!session) {
            return MakeErrorResponse(http::status::internal_server_error,
                "internalError", "Game session not found");
        }

        json::object response_obj;
        json::object players_obj;

        for (const auto& info : players_info) {
            const auto* player = app_.GetPlayers().FindById(info.id);
            if (!player) {
                continue;
            }

            auto* dog = session->FindDog(player->GetDogId());
            if (!dog) {
                continue;
            }

            json::object player_state;
            json::array pos = { dog->GetPosition().x, dog->GetPosition().y };
            json::array speed = { dog->GetSpeed().x, dog->GetSpeed().y };

            std::string dir;
            switch (dog->GetDirection()) {
            case model::Direction::NORTH: dir = "U"; break;
            case model::Direction::EAST:  dir = "R"; break;
            case model::Direction::SOUTH: dir = "D"; break;
            case model::Direction::WEST:  dir = "L"; break;
            }
            
            player_state["pos"] = pos;
            player_state["speed"] = speed;
            player_state["dir"] = dir;
            json::array bag;
            for (const auto& item : dog->GetBag()) {
                json::object bag_item;
                bag_item["id"] = item.id;
                bag_item["type"] = item.type;
                bag.push_back(std::move(bag_item));
            }
            player_state["bag"] = bag;
            player_state["score"] = dog->GetScore();
            players_obj[std::to_string(info.id)] = player_state;
        }

        response_obj["players"] = players_obj;
        
        json::object lost_objects;
        size_t idx = 0;
        for (const auto& obj : session->GetLostObjects()) {
            json::object item;
            item["type"] = obj.type;                         
            item["pos"] = json::array{ obj.x, obj.y };   
            lost_objects[std::to_string(idx)] = std::move(item);
            idx++;
        }
        response_obj["lostObjects"] = std::move(lost_objects);
        http::response<http::string_body> response(http::status::ok, 11);
        response.set(http::field::content_type, "application/json");
        response.set(http::field::cache_control, "no-cache");
        response.body() = json::serialize(response_obj);
        response.content_length(response.body().size());
        response.keep_alive(true);
        return response;
    });
}

http::response<http::string_body> ApiHandler::HandleActionReq(const http::request<http::string_body>& req) {
    auto content_type = req[http::field::content_type];
    if (content_type != "application/json") {
        return MakeErrorResponse(http::status::bad_request,
            "invalidArgument", "Invalid content type");
    }

    return ExecuteAuthorized(req, app_, [&](const auto* current_player, const std::string&) {
        try {
            auto json_body = json::parse(req.body());
            auto obj = json_body.as_object();
            if (!obj.contains("move")) {
                return MakeErrorResponse(http::status::bad_request,
                    "invalidArgument", "Failed to parse action");
            }
            std::string move = json::value_to<std::string>(obj.at("move"));
            if (move != "L" && move != "R" && move != "U" && move != "D" && move != "") {
                return MakeErrorResponse(http::status::bad_request,
                   "invalidArgument", "Failed to parse action");
            }

            auto* session = app_.GetGame().FindSession(model::Map::Id{ current_player->GetMapId() });
            auto* dog = session->FindDog(current_player->GetDogId());
            double speed = app_.GetMapSpeed(current_player->GetMapId());

            dog->SetMovement(move, speed);

            json::object empty_obj;
            http::response<http::string_body> response(http::status::ok, 11);
            response.set(http::field::content_type, "application/json");
            response.set(http::field::cache_control, "no-cache");
            response.body() = json::serialize(empty_obj);
            response.content_length(response.body().size());
            response.keep_alive(true);
            return response;
        }
        catch (const json::system_error& e) {
            return MakeErrorResponse(http::status::bad_request,
                "invalidArgument", "Failed to parse action");
        }
    });
}
http::response<http::string_body> ApiHandler::HandleTickReq(const http::request<http::string_body>& req) {
    auto content_type = req[http::field::content_type];
    if (content_type != "application/json") {
        return MakeErrorResponse(http::status::bad_request,
            "invalidArgument", "Invalid content type");
    }
    try {
        auto json_body = json::parse(req.body());
        auto obj = json_body.as_object();

        // 4. Проверяем наличие поля timeDelta
        if (!obj.contains("timeDelta")) {
            return MakeErrorResponse(http::status::bad_request,
                "invalidArgument", "Missing required field: timeDelta");
        }

        // 5. Получаем timeDelta
        const auto& time_delta_value = obj.at("timeDelta");
        if (!time_delta_value.is_int64()) {
            return MakeErrorResponse(http::status::bad_request,
                "invalidArgument", "timeDelta must be an integer");
        }

        int64_t time_delta = time_delta_value.as_int64();

        // 6. Проверяем, что timeDelta > 0
        if (time_delta <= 0) {
            return MakeErrorResponse(http::status::bad_request,
                "invalidArgument", "timeDelta must be positive");
        }

        // 7. Обновляем состояние игры
        app_.Tick(std::chrono::milliseconds{time_delta});

        // 8. Возвращаем успешный ответ с пустым JSON
        http::response<http::string_body> response(http::status::ok, 11);
        response.set(http::field::content_type, "application/json");
        response.set(http::field::cache_control, "no-cache");
        response.body() = "{}";
        response.content_length(response.body().size());
        response.keep_alive(true);
        return response;

    }
    catch (const json::system_error& e) {
        // Ошибка парсинга JSON
        return MakeErrorResponse(http::status::bad_request,
            "invalidArgument", "Failed to parse tick request JSON");
    }
    catch (const std::exception& e) {
        // Другие ошибки
        return MakeErrorResponse(http::status::bad_request,
            "invalidArgument", "Failed to process tick request");
    }
}

http::response<http::string_body> ApiHandler::HandleRecordkReq(const http::request<http::string_body>& req) {
    int start = 0;
    int max_items = 100;

    auto target = req.target();
    auto qpos = target.find('?');
    if (qpos != boost::string_view::npos) {
        auto query = target.substr(qpos + 1);
        while (!query.empty()) {
            auto amp = query.find('&');
            auto pair = query.substr(0, amp);
            auto eq = pair.find('=');
            if (eq != boost::string_view::npos) {
                auto key = pair.substr(0, eq);
                auto value = pair.substr(eq + 1);
                if (key == "start") {
                    start = std::stoi(std::string(value));
                }
                else if (key == "maxItems") {
                    max_items = std::stoi(std::string(value));
                }
            }
            if (amp == boost::string_view::npos) break;
            query = query.substr(amp + 1);
        }
    }
    if (start < 0 || max_items < 0) {
        return MakeErrorResponse(http::status::bad_request,
            "invalidArgument", "start and maxItems must be non-negative");
    }
    if (max_items > 100) {
        return MakeErrorResponse(http::status::bad_request,
            "invalidArgument", "maxItems must not exceed 100");
    }

    auto records = db_.GetRecords(start, max_items);

    json::array arr;
    for (const auto& r : records) {
        arr.push_back(json::object{
            {"name", r.name},
            {"score", r.score},
            {"playTime", r.play_time.count() / 1000.0}
            });
    }

    http::response<http::string_body> response(http::status::ok, req.version());
    response.set(http::field::content_type, "application/json");
    response.set(http::field::cache_control, "no-cache");
    response.body() = json::serialize(arr);
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    return response;
}
}  // namespace http_handler


