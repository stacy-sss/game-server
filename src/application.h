#pragma once
#include "model.h"
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
#include "ticker.h"
#include <boost/signals2.hpp>
namespace application {

struct JoinGameResult {
    std::string token;
    uint32_t player_id;
    bool success;
    std::string error_code;
    std::string error_message;
};
struct PlayerInfo {
    uint32_t id;
    std::string name;
};

class Player {
public:
    Player(uint32_t id, std::string name, std::string token, uint32_t dog_id, std::string map_id) :
        id_{ id }, name_{ std::move(name) }, token_{ std::move(token) }, dog_id_{ dog_id }, map_id_{ std::move(map_id) } {}

    uint32_t GetId() const {
        return id_;
    }
    std::string GetName() const {
        return name_;
    }
    std::string GetToken() const {
        return token_;
    }
    uint32_t GetDogId() const {
        return dog_id_;
    }
    PlayerInfo ToInfo() const {
        return { id_, name_ };
    }
    const std::string& GetMapId() const { 
        return map_id_; 
    }
    
private:
    uint32_t id_;
    std::string name_;
    std::string token_;
    uint32_t dog_id_;
    std::string map_id_;
   
};
class Players {
public:
    Player& AddPlayer(Player player);
    Player* FindByDogId(uint32_t id);
    Player* FindByToken(const std::string& token);
    const Player* FindByToken(const std::string& token) const;
    std::vector<PlayerInfo> GetAllPlayers() const;
    std::vector<PlayerInfo> GetPlayersOnMap(const std::string& map_id) const;
    Player* FindById(uint32_t id);
    const Player* FindById(uint32_t id) const;

    const std::unordered_map<std::string, Player>& GetAll() const {
        return players_;
    }
    void RemoveByDogId(uint32_t id) {
        auto it = token_by_dog_id_.find(id);
        if (it != token_by_dog_id_.end()) {
            players_.erase(it->second);
            token_by_dog_id_.erase(it);
        }
    }
private:
    std::unordered_map<std::string, Player> players_;
    std::unordered_map<uint32_t, std::string> token_by_dog_id_;
};

struct RetiredPlayer {
    std::string name;
    int score;
    std::chrono::milliseconds play_time;
};
class Application {
public:
    using TickHandler = std::function<void(std::chrono::milliseconds)>;
    using RetiredHandler = std::function<void(const RetiredPlayer&)>;
    Application(model::Game& game) : game_(game),
        generator1_{ std::random_device{}() }
        , generator2_{ std::random_device{}() }, 
    next_dog_id_(0), next_player_id_(0) {}

    JoinGameResult JoinGame(const std::string& player_name,
        const std::string& map_id);
    Players& GetPlayers() {
        return players_;
    }
    const Players& GetPlayers() const {
        return players_;
    }
    model::Game& GetGame() { 
        return game_; 
    }
    const model::Game& GetGame() const { 
        return game_; 
    }
    std::vector<PlayerInfo> GetGameState(const std::string& token) const;
    double GetMapSpeed(const std::string& map_id) const;
    void UpdateGameState(int64_t time_delta_ms);
    void Tick(std::chrono::milliseconds delta);
    void StartTicker(net::io_context& ioc, std::chrono::milliseconds period);
    bool IsTickManual() const { return !ticker_; }
    void DoOnTick(TickHandler handler) {
        tick_signal_ = std::move(handler);
    }
    void DoOnRetire(RetiredHandler handler) {
        rtr_handler = std::move(handler);
    }
    //реализовать слушателя для бд
private:
    std::string GenerateToken();

    Players players_;
    model::Game& game_;
    uint32_t next_dog_id_;
    uint32_t next_player_id_;
    std::mt19937_64 generator1_;
    std::mt19937_64 generator2_;

    TickHandler tick_signal_;
    RetiredHandler rtr_handler_;
    std::chrono::milliseconds game_time_{ 0 };
    std::shared_ptr<Ticker> ticker_;
};


}
