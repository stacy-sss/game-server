#pragma once
#include "http_server.h"
#include "model.h"
#include "logger.h"
#include "extra_data.h"
#include "application.h"
#include "postgres.h"
#include <boost/log/utility/manipulators/add_value.hpp>
#include <filesystem>
#include <boost/json.hpp> 

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace json = boost::json;

namespace json_keys {
    using boost::json::string_view;

    inline constexpr auto ID = "id"sv;
    inline constexpr auto NAME = "name"sv;
    inline constexpr auto X = "x"sv;
    inline constexpr auto Y = "y"sv;
    inline constexpr auto W = "w"sv;
    inline constexpr auto H = "h"sv;
    inline constexpr auto X0 = "x0"sv;
    inline constexpr auto Y0 = "y0"sv;
    inline constexpr auto X1 = "x1"sv;
    inline constexpr auto Y1 = "y1"sv;
    inline constexpr auto OFFSET_X = "offsetX"sv;
    inline constexpr auto OFFSET_Y = "offsetY"sv;
    inline constexpr auto ROADS = "roads"sv;
    inline constexpr auto BUILDINGS = "buildings"sv;
    inline constexpr auto OFFICES = "offices"sv;
    inline constexpr auto LOOT = "lootTypes"sv;
    // Для ошибок
    inline constexpr auto CODE = "code"sv;
    inline constexpr auto MESSAGE = "message"sv;
}
namespace Endpoints {
    using namespace std::string_view_literals;
    // Эндпоинты
    inline constexpr auto MAPS_ROOT = "/api/v1/maps"sv;
    inline constexpr auto MAPS_WITH_ID = "/api/v1/maps/"sv;
    inline constexpr auto API_ROOT = "/api/"sv;
    inline constexpr auto API_JOIN = "/api/v1/game/join"sv;
    inline constexpr auto API_PLAYERS = "/api/v1/game/players"sv;
    inline constexpr auto API_STATE = "/api/v1/game/state"sv;
    inline constexpr auto API_ACTION = "/api/v1/game/player/action"sv;
    inline constexpr auto API_TICK = "/api/v1/game/tick"sv;
    inline constexpr auto API_RECORDS = "/api/v1/game/records"sv;
}
namespace http_handler {
    //для обработки ошибок
    http::response<http::string_body> MakeErrorResponse(http::status status,
        const std::string& code, const std::string& message);

class ApiHandler {
public:
    ApiHandler(application::Application& app, postgres::Database& db) : app_{app}, db_{ db } {}
    template <typename Body, typename Allocator, typename Send>
    void HandleApiRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string target = std::string(req.target());

        if (target == Endpoints::API_JOIN) {
            if (req.method() != http::verb::post) {
                auto response = MakeErrorResponse(http::status::method_not_allowed,
                    "invalidMethod", "Only POST method is expected");
                response.set(http::field::allow, "POST");
                send(std::move(response));
                return;
            }
            auto content_type = req[http::field::content_type];
            if (content_type != "application/json") {
                send(MakeErrorResponse(http::status::bad_request,
                    "invalidArgument", "Content-Type must be application/json"));
                return;
            }
            send(HandleJoinReq(req));
            return;
        }
        if (target == Endpoints::API_PLAYERS) {
            if (req.method() != http::verb::get && req.method() != http::verb::head) {
                auto response = MakeErrorResponse(http::status::method_not_allowed,
                    "invalidMethod", "Invalid method");
                response.set(http::field::allow, "GET, HEAD");
                send(std::move(response));
                return;
            }
            send(HandlePlayersReq(req));
            return;
        }
        if (target == Endpoints::API_STATE) {
            if (req.method() != http::verb::get && req.method() != http::verb::head) {
                auto response = MakeErrorResponse(http::status::method_not_allowed,
                    "invalidMethod", "Invalid method");
                response.set(http::field::allow, "GET, HEAD");
                send(std::move(response));
                return;
            }
            send(HandleStateReq(req));
            return;
        }
        if (target == Endpoints::API_ACTION) {
            if (req.method() != http::verb::post) {
                auto response = MakeErrorResponse(http::status::method_not_allowed,
                    "invalidMethod", "Only POST method is expected");
                response.set(http::field::allow, "POST");
                send(std::move(response));
                return;
            }
            send(HandleActionReq(req));
            return;
        }
        if (target == Endpoints::API_TICK) {
            if (req.method() != http::verb::post) {
                auto response = MakeErrorResponse(http::status::method_not_allowed,
                    "invalidMethod", "Only POST method is expected");
                response.set(http::field::allow, "POST");
                send(std::move(response));
                return;
            }
            if (app_.IsTickManual()) {
                send(HandleTickReq(req));
            }
            else {
                send(MakeErrorResponse(http::status::bad_request,
                    "badRequest", "Invalid endpoint"));
            }
            
            return;
        }
        if (target.starts_with(Endpoints::API_RECORDS)) {
            if (req.method() != http::verb::get && req.method() != http::verb::head) {
                auto response = MakeErrorResponse(http::status::method_not_allowed,
                    "invalidMethod", "Invalid method");
                response.set(http::field::allow, "GET, HEAD");
                send(std::move(response));
                return;
            }
            send(HandleRecordkReq(req));
            return;
        }
    }
private:
    http::response<http::string_body> HandleJoinReq(const http::request<http::string_body>& req);
    http::response<http::string_body> HandlePlayersReq(const http::request<http::string_body>& req);
    http::response<http::string_body> HandleStateReq(const http::request<http::string_body>& req);
    http::response<http::string_body> HandleActionReq(const http::request<http::string_body>& req);
    http::response<http::string_body> HandleTickReq(const http::request<http::string_body>& req);
    http::response<http::string_body> HandleRecordkReq(const http::request<http::string_body>& req);
    application::Application& app_;
    postgres::Database& db_;
    template <typename Fn>
    http::response<http::string_body> ExecuteAuthorized(const http::request<http::string_body>& req,
        application::Application& app, Fn&& action);
};
class RequestHandler {
public:
    explicit RequestHandler(model::Game& game, std::filesystem::path static_dir, ApiHandler& api_handler, extra_data::ExtraData& ex)
        : game_{ game }, static_dir_(std::move(static_dir)), api_handler_{ api_handler }, ex_{ex} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string target = std::string(req.target());
        
        try {
            if (target.find(Endpoints::API_ROOT) == 0) {
                if (target == Endpoints::API_JOIN) {
                    api_handler_.HandleApiRequest(std::move(req), send);
                    return;
                }
                if (target == Endpoints::API_PLAYERS) {
                    api_handler_.HandleApiRequest(std::move(req), send);
                    return;
                }
                if (target == Endpoints::API_STATE) {
                    api_handler_.HandleApiRequest(std::move(req), send);
                    return;
                }
                if (target == Endpoints::API_ACTION) {
                    api_handler_.HandleApiRequest(std::move(req), send);
                    return;
                }
                if (target == Endpoints::API_TICK) {
                    api_handler_.HandleApiRequest(std::move(req), send);
                    return;
                }
                if (target.starts_with(Endpoints::API_RECORDS)) {
                    api_handler_.HandleApiRequest(std::move(req), send);
                    return;
                }
                if (req.method() != http::verb::get && req.method() != http::verb::head) {
                    auto response = MakeErrorResponse(http::status::method_not_allowed,
                        "invalidMethod", "Only GET and HEAD allowed");
                    response.set(http::field::allow, "GET, HEAD");
                    send(std::move(response));
                    return;
                }
                if (target == Endpoints::MAPS_ROOT) {
                    send(HandleMapsReq());
                    return;
                }
                if (target.find(Endpoints::MAPS_WITH_ID) == 0) {
                    std::string id = target.substr(Endpoints::MAPS_WITH_ID.size());
                    send(HandleMapReq(id));
                    return;
                }
                send(MakeErrorResponse(http::status::bad_request,
                    "badRequest", "Bad request"));
                return;
            }
            send(HandleStaticRequest(target));
        }
        catch (...) {
            send(MakeErrorResponse(http::status::bad_request, "badRequest", "Bad request"));
            return;
        }
    }

private:
    http::response<http::string_body> HandleMapsReq();
    http::response<http::string_body> HandleMapReq(const std::string& id);
    //вспомогательные методы для статических файлов
    std::string UrlDecode(const std::string& target);
    std::string GetMimeType(const std::string& ext);
    bool IsPathUnderRoot(const std::filesystem::path& path, const std::filesystem::path& root);
    http::response<http::string_body> MakeTextErrorResponse(http::status status,
        const std::string& message) const;
    http::response<http::string_body> HandleStaticRequest(const std::string& target);

    //вспомогательные методы для обработки json
    json::array SerializeRoads(const model::Map& map) const;
    json::array SerializeBuildings(const model::Map& map) const;
    json::array SerializeOffices(const model::Map& map) const;
    json::object SerializeMapInfo(const model::Map& map) const;

    model::Game& game_;
    extra_data::ExtraData& ex_;
    std::filesystem::path static_dir_;
    ApiHandler& api_handler_;
};

}  // namespace http_handler

template<typename BaseHandler>
class LoggerReqHandler {
public:
    explicit LoggerReqHandler(BaseHandler& handler, std::string client_ip) : decorated_(handler), 
    client_ip_(std::move(client_ip)){}

    template<typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        auto start_time = std::chrono::steady_clock::now();

        std::string ip = client_ip_;
        std::string uri = std::string(req.target());
        std::string method = std::string(req.method_string());

        json::value req_data = {
            {"ip", ip},
            {"URI", uri},
            {"method", method}
        };

        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, req_data) << "request received";

        decorated_(std::move(req), [&](auto&& resp) {
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            int response_time = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
            int code = resp.result_int();
            std::string content_type;
            if (auto it = resp.find(http::field::content_type); it != resp.end()) {
                content_type = std::string(it->value());
            }
            json::value resp_data{
                {"response_time", response_time},
                {"code", code},
                {"content_type", content_type.empty() ? nullptr : json::value(content_type)}
            };
            BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, resp_data) << "response sent";
            send(std::move(resp));
        });
    }

private:
    BaseHandler& decorated_;
    std::string client_ip_;
};



