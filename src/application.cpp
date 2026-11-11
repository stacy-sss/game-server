#include "application.h"
#include <iomanip>  
#include <sstream>    
#include <random> 

namespace application {
    //-------Application---------
    JoinGameResult Application::JoinGame(const std::string& player_name,
        const std::string& map_id) {
        if (player_name.empty()) {
            return{ "", 0, false, "invalidArgument", "Invalid name" };
        }
        model::Map::Id map_id_tagged{ map_id };
        const model::Map* map = game_.FindMap(map_id_tagged);

        if (!map) {
            return{ "", 0, false, "mapNotFound", "Map not found" };
        }

        model::GameSession* session = game_.FindSession(map_id_tagged);
        if (!session) {
            session = &game_.CreateSessionForMap(map_id_tagged);
        }

        uint32_t dog_id = next_dog_id_;
        model::DogPoint start_pos = map->GetRandomStartPos();
        model::Dog dog(dog_id, player_name, start_pos, map->GetBagCapacity());
        dog.SetJoinTime(game_time_);
        session->AddDog(std::move(dog));

        uint32_t player_id = next_player_id_;
        std::string token = GenerateToken();
        Player player(player_id, player_name, token, dog_id, map_id);
        players_.AddPlayer(std::move(player));

        next_dog_id_++;
        next_player_id_++;

        return { token, player_id, true, "", "" };
    }
    std::string Application::GenerateToken() {
        uint64_t part1 = generator1_();
        uint64_t part2 = generator2_();
        std::ostringstream oss;
        oss << std::hex << std::setw(16) << std::setfill('0') << part1
            << std::setw(16) << std::setfill('0') << part2;
        return oss.str();
    }
    
    std::vector<PlayerInfo> Application::GetGameState(const std::string& token) const {
        auto* player = players_.FindByToken(token);
        if (!player) {
            return {};
        }

        return players_.GetPlayersOnMap(player->GetMapId());
    }
    double Application::GetMapSpeed(const std::string& map_id) const {
        auto* map = game_.FindMap(model::Map::Id{ map_id });
        if (!map) {
            return game_.GetDefaultDogSpeed();
        }
        return map->GetDogSpeed();
    }
    void Application::UpdateGameState(int64_t time_delta_ms) {
        double time_delta_seconds = static_cast<double>(time_delta_ms) / 1000.0;
        auto& sessions = game_.GetSession();
        for (auto& [map_id, session] : sessions) {
            if (session) {
                session->UpdateState(time_delta_seconds);
            }
        }
    }
    void Application::Tick(std::chrono::milliseconds delta) {
        UpdateGameState(delta.count());
        game_time_ += delta;
        for (auto& [map_id, session] : game_.GetSession()) {
            if (!session) continue;
            std::vector<uint32_t> to_retire;
            for (const auto& [dog_id, dog] : session->GetDogs()) {
                if (dog.GetIdleTime() >= game_.GetRetirementTime()) {
                    to_retire.push_back(dog_id);
                }
            }
            for (auto dog_id : to_retire) {
                auto* dog = session->FindDog(dog_id);
                if (!dog) continue;
                auto play_time = game_time_ - dog->GetJoinTime();
                if (rtr_handler_) {
                    RetiredPlayer rp{
                        dog->GetName(), dog->GetScore(), play_time
                    };
                    rtr_handler_(rp);
                }
                players_.RemoveByDogId(dog_id);
                session->RemoveDog(dog_id);
            }
        }

        
        if (tick_signal_) {
            tick_signal_(delta);
        }
    }

    void Application::StartTicker(net::io_context& ioc, std::chrono::milliseconds period) {
        auto strand = net::make_strand(ioc);
        ticker_ = std::make_shared<Ticker>(
            strand,
            period,
            [this](std::chrono::milliseconds delta) { this->Tick(delta); }
        );
        ticker_->Start();
    }
    //-------Players---------
    Player& Players::AddPlayer(Player player) {
        std::string token = player.GetToken();
        uint32_t dog_id = player.GetDogId();
        auto [it, inserted] = players_.emplace(token, std::move(player));
        if (inserted) {
            token_by_dog_id_[dog_id] = token;
        }
        return it->second;
    }
    Player* Players::FindByDogId(uint32_t id) {
        for (auto& [token, player] : players_) {
            if (player.GetDogId() == id) {
                return &player;
            }
        }
        return nullptr;
    }
    Player* Players::FindByToken(const std::string& token) {
        auto it = players_.find(token);
        return (it != players_.end()) ? &it->second : nullptr;

    }
    const Player* Players::FindByToken(const std::string& token) const {
        auto it = players_.find(token);
        return (it != players_.end()) ? &it->second : nullptr;

    }
    std::vector<PlayerInfo> Players::GetAllPlayers() const {
        std::vector<PlayerInfo> result;
        result.reserve(players_.size());
        for (const auto& [token, player] : players_) {
            result.push_back(player.ToInfo());
        }
        return result;
    }

    std::vector<PlayerInfo> Players::GetPlayersOnMap(const std::string& map_id) const {
        std::vector<PlayerInfo> result;
        for (const auto& [token, player] : players_) {
            if (player.GetMapId() == map_id) {
                result.push_back(player.ToInfo());
            }
        }
        return result;
    }
    Player* Players::FindById(uint32_t id) {
        for (const auto& [token, player] : players_) {
            if (player.GetId() == id) {
                return const_cast<Player*>(&player);
            }
        }
        return nullptr;
    }

    const Player* Players::FindById(uint32_t id) const {
        for (const auto& [token, player] : players_) {
            if (player.GetId() == id) {
                return const_cast<Player*>(&player);
            }
        }
        return nullptr;
    }
}

