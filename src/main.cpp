#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>
#include <filesystem>
#include "json_loader.h"
#include "request_handler.h"
#include "logger.h"
#include "application.h"
#include "cli_parser.h"
#include "ticker.h"
#include "extra_data.h"
#include "serialization.h"
#include "serialization_listener.h"
#include <fstream>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/archive/text_iarchive.hpp>

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

int main(int argc, const char* argv[]) {
    try {
        InitBoostLog();
        auto args = ParseCommandLine(argc, argv);
        if (!args.has_value()) {
            return EXIT_FAILURE;
        }
        if (args->help) {
            return EXIT_SUCCESS;
        }
        extra_data::ExtraData ex;
        model::Game game = json_loader::LoadGame(args->config_file, ex);
        std::filesystem::path static_dir = args->www_root;
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });
        application::Application app(game);

        std::string state_file;
std::chrono::milliseconds save_period{0};
if (!args->state_file.empty()) {
    state_file = args->state_file;
    if (args->save_state_period.has_value() && args->save_state_period.value() > 0) {
        save_period = std::chrono::milliseconds{args->save_state_period.value()};
    }
}
        if (!state_file.empty() && std::filesystem::exists(state_file)) {
            try {
                std::ifstream file(state_file, std::ios::binary);
                boost::archive::text_iarchive ar(file);
                serialization::ApplicationRepr app_repr;
                serialization::GameRepr game_repr;
                ar >> app_repr >> game_repr;
                for (const auto& [map_id_str, session_repr] : game_repr.GetSessions()) {
                    model::Map::Id map_id{ map_id_str };
                    if (!game.FindMap(map_id)) {
                        throw std::runtime_error("Map not found: " + map_id_str);
                    }
                    auto& session = game.CreateSessionForMap(map_id);
                    for (const auto& dog_repr : session_repr.GetDogs()) {
                        session.AddDog(dog_repr.Restore());
                    }
                    for (const auto& obj : session_repr.GetLostObjects()) {
                        session.AddLostObj(obj);
                    }
                    session.SetNextIdLostObj(session_repr.GetNextId());
                }
                BOOST_LOG_TRIVIAL(info) << "Restoring " << app_repr.GetPlayers().size() << " players";
                for (const auto& player_repr : app_repr.GetPlayers()) {
                    auto player = player_repr.Restore();
                 BOOST_LOG_TRIVIAL(info) << "Restore player: token=" << player.GetToken() 
                            << " name=" << player.GetName();
                app.GetPlayers().AddPlayer(std::move(player));
                }
                BOOST_LOG_TRIVIAL(info) << "After restore, players size: " 
                        << app.GetPlayers().GetAll().size();
            }
            catch (const std::exception& e) {
                json::value data_error = {
                    {"code", EXIT_FAILURE},
                    {"exception", e.what()}
                };
                BOOST_LOG_TRIVIAL(error) << logging::add_value(additional_data, data_error)
                    << "failed to load state";
                return EXIT_FAILURE;
            }
        }

        if (args->tick_period.has_value()) {
            BOOST_LOG_TRIVIAL(info) << "Starting ticker with period: " 
                            << args->tick_period->count() << "ms";
            app.StartTicker(ioc, args->tick_period.value());
        } else {
            BOOST_LOG_TRIVIAL(info) << "No ticker, manual ticks only";
        }
        const char* db_url = std::getenv("GAME_DB_URL");
        if (!db_url) {
            db_url = std::getenv("DB_URL");
        }
        if (!db_url) {
            throw std::runtime_error("DB URL is not set");
        }
        const size_t pool_size = std::max(1u, std::thread::hardware_concurrency());
        postgres::Database db{ db_url, pool_size };
        app.DoOnRetire([&db](const application::RetiredPlayer& player) {
            db.AddRetiredPlayer(player);
            });
        http_handler::ApiHandler api_handler(app, db);
        http_handler::RequestHandler handler{ game, static_dir, api_handler, ex };

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        tcp::endpoint endpoint{ address, port };

        LoggerReqHandler logger_handler{ handler, endpoint.address().to_string()};

        http_server::ServeHttp(ioc, endpoint, logger_handler);
        json::value data_start = {
            {"port", port},
            {"address", address.to_string()}
        };
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data_start) << "server started";
        BOOST_LOG_TRIVIAL(info) << "Creating listener with save_period=" << save_period.count();  
        SerializationListener listener(app, game, state_file, save_period);
        app.DoOnTick([&listener](std::chrono::milliseconds delta) {
            listener.OnTick(delta);
         });
        
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
        if (!state_file.empty()) {
            listener.Save();
        }
        json::value data_stop = {
            {"code", 0},
            {"exception", nullptr}
        };
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data_stop) << "server exited";

    } catch (const std::exception& ex) {
        json::value data_error = {
            {"code", EXIT_FAILURE},
            {"exception", ex.what()}
        };
        BOOST_LOG_TRIVIAL(error) << logging::add_value(additional_data, data_error) << "server exited";

        return EXIT_FAILURE;
    }
}


