#include "model.h"

#include <stdexcept>
#include <iostream>
namespace model {
using namespace std::literals;

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}
bool Map::IsPointOnRoad(double x, double y, const Road& road) const {
    const double ROAD_WIDTH = 0.8;
    const double HALF_WIDTH = ROAD_WIDTH / 2.0; // 0.4

    auto start = road.GetStart();
    auto end = road.GetEnd();

    if (road.IsHorizontal()) {
        double min_x = static_cast<double>(std::min(start.x, end.x));
        double max_x = static_cast<double>(std::max(start.x, end.x));
        double road_y = static_cast<double>(start.y);

        if (x < min_x - HALF_WIDTH || x > max_x + HALF_WIDTH) {
            return false;
        }
        if (y < road_y - HALF_WIDTH || y > road_y + HALF_WIDTH) {
            return false;
        }
    }
    else {
        double road_x = static_cast<double>(start.x);
        double min_y = static_cast<double>(std::min(start.y, end.y));
        double max_y = static_cast<double>(std::max(start.y, end.y));

       
        if (x < road_x - HALF_WIDTH || x > road_x + HALF_WIDTH) {
            return false;
        }
        if (y < min_y - HALF_WIDTH || y > max_y + HALF_WIDTH) {
            return false;
        }
    }
    return true;
}

void GameSession::MoveDog(Dog& dog, double seconds) {
    auto speed = dog.GetSpeed();
    if (speed.x == 0 && speed.y == 0) {
        return;
    }

    const double STEP = 0.1;
    double remaining_time = seconds;

    while (remaining_time > 0) {
        double step = std::min(STEP, remaining_time);

        auto pos = dog.GetPosition();
        double new_x = pos.x + speed.x * step;
        double new_y = pos.y + speed.y * step;

        const Road* road = map_.FindRoadContainingPoint(new_x, new_y);
        if (road) {
            dog.SetPosition({ new_x, new_y });
            remaining_time -= step;
        }
        else {
            HandleBoundaryCollision(dog, step);
            speed = dog.GetSpeed();
            if (speed.x == 0 && speed.y == 0) {
                break;  // Выходим из цикла
            }
            remaining_time -= step;
        }
    }
}
void GameSession::HandleBoundaryCollision(Dog& dog, double time_delta_seconds) {
    auto speed = dog.GetSpeed();
    auto pos = dog.GetPosition();
    const double HALF_WIDTH = 0.4;
    // Находим дорогу, на которой собака была ДО шага
    const Road* current_road = map_.FindRoadContainingPoint(pos.x, pos.y);

    if (current_road) {
        if (current_road->IsHorizontal()) {
            // Горизонтальная дорога
            double min_x = static_cast<double>(std::min(current_road->GetStart().x, current_road->GetEnd().x));
            double max_x = static_cast<double>(std::max(current_road->GetStart().x, current_road->GetEnd().x));
            double road_y = static_cast<double>(current_road->GetStart().y);

            if (speed.x < 0) {
                // Движение влево — левая граница
                dog.SetPosition({ min_x - HALF_WIDTH, pos.y });
            }
            else if (speed.x > 0) {
                // Движение вправо — правая граница
                dog.SetPosition({ max_x + HALF_WIDTH, pos.y });
            }
            else if (speed.y < 0) {
                // Движение вверх — верхняя граница
                dog.SetPosition({ pos.x, road_y - HALF_WIDTH });
            }
            else if (speed.y > 0) {
                // Движение вниз — нижняя граница
                dog.SetPosition({ pos.x, road_y + HALF_WIDTH });
            }
        }
        else {
            // Вертикальная дорога
            double min_y = static_cast<double>(std::min(current_road->GetStart().y, current_road->GetEnd().y));
            double max_y = static_cast<double>(std::max(current_road->GetStart().y, current_road->GetEnd().y));
            double road_x = static_cast<double>(current_road->GetStart().x);

            if (speed.y < 0) {
                // Движение вверх — верхняя граница
                dog.SetPosition({ pos.x, min_y - HALF_WIDTH });
            }
            else if (speed.y > 0) {
                // Движение вниз — нижняя граница
                dog.SetPosition({ pos.x, max_y + HALF_WIDTH });
            }
            else if (speed.x < 0) {
                // Движение влево — левая граница
                dog.SetPosition({ road_x - HALF_WIDTH, pos.y });
            }
            else if (speed.x > 0) {
                // Движение вправо — правая граница
                dog.SetPosition({ road_x + HALF_WIDTH, pos.y });
            }
        }
    }

    // ВСЕГДА обнуляем скорость после столкновения
    dog.SetSpeed({ 0, 0 });
}

void GameSession::SpawnLostObjects(double time_delta) {
    using namespace std::chrono;
    auto delta = duration_cast<milliseconds>(duration<double>{time_delta});
    unsigned to_spawn = loot_gen_.Generate(
        delta,
        static_cast<unsigned>(lost_objects_.size()),
        static_cast<unsigned>(dogs_.size()));
    for (unsigned i = 0; i < to_spawn; i++) {
        SpawnOneLostObject();
    }
}
void GameSession::SpawnOneLostObject() {
    const size_t loot_count = map_.GetLootTypeCount();
    if (loot_count == 0 || map_.GetRoads().empty()) {
        return;
    }
    size_t type = RandInt(0, loot_count - 1);
    const auto& roads = map_.GetRoads();
    size_t random_idx = RandInt(0, roads.size() - 1);
    const Road& road = roads[random_idx];
    int id = next_lost_object_id_;
    LostPoint p = RandomPointOnRoad(road);
    bool collected = false;
    lost_objects_.push_back({ id, type, p.x, p.y, collected});
    next_lost_object_id_++;
}

void GameSession::ProcessGatherEvents(
    const std::unordered_map<uint32_t, DogPoint>& old_positions) {
    std::vector<Dog*> dog_ptrs;
    dog_ptrs.reserve(dogs_.size());
    for (auto& [id, dog] : dogs_) {
        dog_ptrs.push_back(&dog);
    }
    SessionGathererProvider provider(
        dog_ptrs,
        old_positions,
        lost_objects_,
        map_.GetOffices()
    );
    auto events = collision_detector::FindGatherEvents(provider);
    for (const auto& event : events) {
        Dog* dog = dog_ptrs[event.gatherer_id];
        if (event.item_id < lost_objects_.size()) {
            auto& obj = lost_objects_[event.item_id];
            if (obj.collected) {
                continue;
            }
            if (!dog->IsFullBag()) {
                dog->TryAddToBag({ static_cast<int>(obj.type), obj.id, map_.GetLootValue(obj.type)});
                obj.collected = true;
            }
        }
        else {
            int total = 0;
            for (const auto& item : dog->GetBag()) {
                total += item.value;
            }
            dog->AddScore(total);
            dog->ClearBag();
        }
    }
    lost_objects_.erase(
        std::remove_if(lost_objects_.begin(), lost_objects_.end(),
            [](const LostObject& obj) { return obj.collected; }),
        lost_objects_.end());
}
size_t GameSession::RandInt(size_t lo, size_t hi) {
    double r = rng_();
    return lo + static_cast<size_t>(r * (hi - lo + 1));
}
LostPoint GameSession::RandomPointOnRoad(const Road& road) {
    double t = rng_();
    auto s = road.GetStart();
    auto e = road.GetEnd();
    return { s.x + t * (e.x - s.x), s.y + t * (e.y - s.y) };
}
void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

GameSession& Game::CreateSessionForMap(const Map::Id& map_id) {
    auto* map = FindMap(map_id);
    if (!map) {
        throw std::runtime_error("Map not found");
    }
    auto session = std::make_unique<GameSession>(*map, loot_period_, loot_probability_, rng_);
    sessions_[map_id] = std::move(session);
    return *sessions_[map_id];
}

GameSession* Game::FindSession(const Map::Id& map_id) {
    auto it = sessions_.find(map_id);
    if (it != sessions_.end()) {
        return it->second.get();
    }
    return nullptr;
}

}  // namespace model


