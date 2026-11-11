#pragma once
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include "model.h"

namespace geom {

    template <typename Archive>
    void serialize(Archive& ar, Point2D& point, [[maybe_unused]] const unsigned version) {
        ar& point.x;
        ar& point.y;
    }

    template <typename Archive>
    void serialize(Archive& ar, Vec2D& vec, [[maybe_unused]] const unsigned version) {
        ar& vec.x;
        ar& vec.y;
    }

}  // namespace geom

namespace model {
    template <typename Archive>
    void serialize(Archive& ar, model::Direction& dir, const unsigned) {
        int val = static_cast<int>(dir);
        ar& val;
        if (Archive::is_loading::value) {
            dir = static_cast<model::Direction>(val);
        }
    }

    template <typename Archive>
    void serialize(Archive& ar, model::BagItem& itm, const unsigned) {
        ar& itm.type;
        ar& itm.id;
        ar& itm.value;
    }

    template <typename Archive>
    void serialize(Archive& ar, model::DogPoint& p, const unsigned) {
        ar& p.x;
        ar& p.y;
    }

    template <typename Archive>
    void serialize(Archive& ar, model::LostObject& obj, const unsigned) {
        ar& obj.id;
        ar& obj.type;
        ar& obj.x;
        ar& obj.y;
        ar& obj.collected;
    }

}//namespace model
namespace serialization {



class DogRepr {
public:
    DogRepr() = default;
    explicit DogRepr(const model::Dog& dog) :
    id_(dog.GetId()), name_(dog.GetName()),
    position_(dog.GetPosition()),
    direction_(dog.GetDirection()),
    score_(dog.GetScore()),
    bag_item_(dog.GetBag()),
    bag_capacity_(dog.GetBagCapacity()),
    speed_(dog.GetSpeed()) {}

    [[nodiscard]] model::Dog Restore() const {
        model::Dog dog{ id_, name_, position_, bag_capacity_ };
        dog.SetDirection(direction_);
        dog.AddScore(score_);
        dog.SetSpeed(speed_);
        dog.SetJoinTime(join_time_);
        dog.SetIdleTime(idle_time_);
        for (const auto& item : bag_item_) {
            if (!dog.TryAddToBag(item)) {
                throw std::runtime_error("Failed to put bag content");
            }
        }
        return dog;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& name_;
        ar& position_;
        ar& direction_;
        ar& score_;
        ar& bag_item_;
        ar& bag_capacity_;
        ar& speed_;
        ar& join_time_;
        ar& idle_time_;
    }
private:
    uint32_t id_ = 0;
    std::string name_;
    model::DogPoint position_;
    model::Direction direction_ = model::Direction::NORTH;
    int score_ = 0;
    std::vector<model::BagItem> bag_item_;
    int bag_capacity_ = 0;
    model::DogPoint speed_;
    std::chrono::milliseconds join_time_;
    std::chrono::milliseconds idle_time_;
};

class PlayerRepr {
public:
    PlayerRepr() = default;
    explicit PlayerRepr(const application::Player& player) :
        id_(player.GetId()), name_(player.GetName()),
        token_(player.GetToken()), dog_id_(player.GetDogId()),
        map_id_(player.GetMapId()) {}

    [[nodiscard]] application::Player Restore() const {
        application::Player player{ id_, name_, token_, dog_id_, map_id_ };
        return player;
    }
    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& name_;
        ar& token_;
        ar& dog_id_;
        ar& map_id_;
    }
private:
    uint32_t id_ = 0;
    std::string name_;
    std::string token_;
    uint32_t dog_id_ = 0;
    std::string map_id_;
};


class GameSessionRepr {
public:
    GameSessionRepr() = default;
    GameSessionRepr(const model::GameSession& session) :
        next_id_(session.GetNextIdLostObj()), map_id_(*session.GetMap().GetId()) {
        for (const auto& [id, dog] : session.GetDogs()) {
            dogs_.emplace_back(dog);
        }
        for (const auto& obj : session.GetLostObjects()) {
            lost_objects_.emplace_back(obj);
        }
    }

    template <typename Archive>
    void serialize(Archive& ar, const unsigned) {
        ar& next_id_;
        ar& map_id_;
        ar& dogs_;
        ar& lost_objects_;
    }
    int GetNextId() const { 
        return next_id_; 
    }
    const std::string& GetMapId() const { 
        return map_id_; 
    }
    const std::vector<DogRepr>& GetDogs() const {
        return dogs_; 
    }
    const std::vector<model::LostObject>& GetLostObjects() const { 
        return lost_objects_; 
    }

private:
    int next_id_ = 0;
    std::string map_id_;
    std::vector<DogRepr> dogs_;
    std::vector<model::LostObject> lost_objects_;
};

class GameRepr {
public:
    GameRepr() = default;
    GameRepr(const model::Game& game) {
        for (const auto& [map_id, session] : game.GetSession()) {
            sessions_.emplace_back(*map_id, *session);
        }
    }
    template <typename Archive>
    void serialize(Archive& ar, const unsigned) {
        ar& sessions_;
    }

    const std::vector<std::pair<std::string, GameSessionRepr>>& GetSessions() const {
        return sessions_;
    }

private:
    std::vector<std::pair<std::string, GameSessionRepr>> sessions_;
};


class ApplicationRepr {
public:
    ApplicationRepr() = default;
    ApplicationRepr(const application::Application& app) {
        for (const auto& [token, player] : app.GetPlayers().GetAll()) {
            players_.emplace_back(player);
        }
    }

    template <typename Archive>
    void serialize(Archive& ar, const unsigned) {
        ar& players_;
    }

    const std::vector<PlayerRepr>& GetPlayers() const {
        return players_; 
    }
private:
    std::vector<PlayerRepr> players_;
};
}//namespace serialization
