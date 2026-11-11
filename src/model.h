#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <random>
#include <algorithm>
#include "tagged.h"
#include "loot_generator.h"
#include "collision_detector.h"
#include <functional>
#include <iostream>
#include <chrono>
namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};
struct DogPoint {
    double x;
    double y;
};
enum class Direction {
    NORTH,
    SOUTH,
    WEST,
    EAST
};
class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }
    DogPoint GetRandomPoint() const {
        return GetRandomPointImpl();
    }
private:
    Point start_;
    Point end_;
    DogPoint GetRandomPointImpl() const{
        static std::random_device rd;
        static std::mt19937 gen(rd());
        if (IsHorizontal()) {
            std::uniform_real_distribution<double> dist_x(
                std::min(start_.x, end_.x),
                std::max(start_.x, end_.x)
            );
            return { dist_x(gen), static_cast<double>(start_.y) };
        }
        else {
            std::uniform_real_distribution<double> dist_y(
                std::min(start_.y, end_.y),
                std::max(start_.y, end_.y)
            );
            return { static_cast<double>(start_.x), dist_y(gen) };
        }
    }
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};
struct BagItem {
    int type;
    int id;
    int value;
};
class Dog {
public:
    Dog() = default;
    Dog(uint32_t id, std::string name, DogPoint position, int bag_capacity)
        : id_{ id }, name_{ std::move(name) }, position_{ position }, bag_capacity_ {bag_capacity} {
    }
    uint32_t GetId() const {
        return id_;
    }
    std::string GetName() const {
        return name_;
    }
    DogPoint GetPosition() const {
        return position_;
    }
    DogPoint GetSpeed() const {
        return speed_;
    }
    Direction GetDirection() const {
        return direction_;
    }
    void SetPosition(DogPoint position) {
        position_ = position;
    }
    void SetSpeed(DogPoint speed) {
        speed_ = speed;
    }
    void SetDirection(Direction direction) {
        direction_ = direction;
    }
    void SetMovement(const std::string& direction, double speed) {
        if (direction == "L") {
            speed_ = { -speed, 0 };
            direction_ = Direction::WEST;
        }
        else if (direction == "R") {
            speed_ = { speed, 0 };
            direction_ = Direction::EAST;
        }
        else if (direction == "U") {
            speed_ = { 0, -speed };
            direction_ = Direction::NORTH;
        }
        else if (direction == "D") {
            speed_ = { 0, speed };
            direction_ = Direction::SOUTH;
        }
        else if (direction == "") {
            speed_ = { 0, 0 };
        }
    }
    int GetBagCapacity() const{
        return bag_capacity_;
    }
    const std::vector<BagItem>& GetBag() const {
        return bag_item_;
    }
    std::vector<BagItem>& GetBag() {
        return bag_item_;
    }
    int GetScore() const {
        return score_;
    }
    void AddScore(int points) {
        score_ += points;
    }
    bool IsFullBag() const {
        return bag_item_.size() >= static_cast<size_t>(bag_capacity_);
    }
    void ClearBag() {
        bag_item_.clear();
    }
    bool TryAddToBag(const BagItem& item) {
        if (IsFullBag()) {
            return false;
        }
        bag_item_.push_back(item);
        return true;
    }
    void UpdateIdle(std::chrono::milliseconds delta) {
        auto speed = GetSpeed();
        if (speed.x == 0.0 && speed.y == 0.0) {
            idle_time_ += delta;
        }
        else {
            idle_time_ = std::chrono::milliseconds{ 0 };
        }
    }
    bool ShouldRetire(std::chrono::milliseconds threshold) const {
        return idle_time_ >= threshold;
    }
    std::chrono::milliseconds GetIdleTime() const{
        return idle_time_;
    }
    void SetJoinTime(std::chrono::milliseconds time) {
        join_time_ = time;
    }
    std::chrono::milliseconds GetJoinTime() const {
        return join_time_;
    }
    void SetIdleTime(std::chrono::milliseconds time) {
        idle_time_ = time;
    }
private:
    uint32_t id_;
    std::string name_;
    DogPoint position_;
    DogPoint speed_{ 0.0, 0.0 };
    std::chrono::milliseconds idle_time_{ 0 };//время бездействия
    std::chrono::milliseconds join_time_{ 0 };
    Direction direction_{ Direction::NORTH };
    std::vector<BagItem> bag_item_;
    int bag_capacity_;
    int score_ = 0;
};

enum class DirectionHint {
    ANY,
    HORIZONTAL,
    VERTICAL
};

struct LostObject {
    int id = 0;
    size_t type = 0;
    double x = 0.0;
    double y = 0.0;
    bool collected = false;
};
class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name, size_t loot_type_count = 0) noexcept
        : id_(std::move(id))
        , name_(std::move(name))
        , loot_type_count_(loot_type_count) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(Office office);
    DogPoint GetRandomStartPos() const {
        if (roads_.empty()) {
            return { 0.0, 0.0 };
        }
        const auto& first_road = roads_[0];
        auto start = first_road.GetStart();
        return { static_cast<double>(start.x), static_cast<double>(start.y) };
    }
    void SetDogSpeed(double speed) {
        dog_speed_ = speed;
    }
    double GetDogSpeed() const {
        return dog_speed_;
    }
    const Road* FindRoadContainingPoint(double x, double y, DirectionHint hint = DirectionHint::ANY) const {
        const Road* best_road = nullptr;

        for (const auto& road : roads_) {
            if (!IsPointOnRoad(x, y, road)) {
                continue;
            }

            // Если нет подсказки — возвращаем первую найденную
            if (hint == DirectionHint::ANY) {
                return &road;
            }

            // Если ищем вертикальную дорогу
            if (hint == DirectionHint::VERTICAL && road.IsVertical()) {
                return &road;
            }

            // Если ищем горизонтальную дорогу
            if (hint == DirectionHint::HORIZONTAL && road.IsHorizontal()) {
                return &road;
            }

            // Запоминаем первую найденную, если подходящей нет
            if (!best_road) {
                best_road = &road;
            }
        }

        return best_road;
    }
    size_t GetLootTypeCount() const noexcept { 
        return loot_type_count_; 
    }
    bool IsPointOnRoad(double x, double y, const Road& road) const;
    int GetBagCapacity() const {
        return bag_capacity_;
    }
    void SetBagCapacity(int capacity) {
        bag_capacity_ = capacity;
    }
    void SetLootValues(std::vector<int> values) {
        loot_values_ = values;
    }
    int GetLootValue(size_t type) const {
        if (type < loot_values_.size()) {
            return loot_values_[type];
        }
        return 0;
    }
private:
    int bag_capacity_ = 3;
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
    double dog_speed_ = 1.0;

    size_t loot_type_count_ = 0;
    std::vector<int> loot_values_;

};
struct LostPoint {
    double x;
    double y;
};
class SessionGathererProvider : public collision_detector::ItemGathererProvider {
public:
    SessionGathererProvider(const std::vector<Dog*>& dogs,
        const std::unordered_map<uint32_t, DogPoint>& old_positions,
        const std::vector<LostObject>& lost_objects, 
        const std::vector<Office>& offices)
        : dogs_(dogs)
        , old_positions_(old_positions)
        , lost_objects_(lost_objects)
        , offices_(offices) {}

    size_t ItemsCount() const override {
        return lost_objects_.size() + offices_.size();
    }
    collision_detector::Item GetItem(size_t idx) const override {
        if (idx < lost_objects_.size()) {
            const auto& obj = lost_objects_[idx];
            return { {obj.x, obj.y}, 0.0 };
        }
        const auto& office = offices_[idx - lost_objects_.size()];
        auto pos = office.GetPosition();
        return { {static_cast<double>(pos.x), static_cast<double>(pos.y)}, 0.5 };
    }

    size_t GatherersCount() const override {
        return dogs_.size();
    }

    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        const Dog* dog = dogs_[idx];
        auto old_pos = old_positions_.at(dog->GetId());
        auto new_pos = dog->GetPosition();
        return {
            {old_pos.x, old_pos.y},
            {new_pos.x, new_pos.y},
            0.6
        };
    }
private:
    const std::vector<Dog*>& dogs_;
    const std::unordered_map<uint32_t, DogPoint>& old_positions_;
    const std::vector<LostObject>& lost_objects_;
    const std::vector<Office>& offices_;
};
class GameSession {
public:
    using RandomGenerator = std::function<double()>;
    explicit GameSession(const Map& map, std::chrono::milliseconds loot_period,
        double loot_probability, RandomGenerator random_gen)
        : map_{ map }, loot_gen_{ loot_period , loot_probability, random_gen },
    rng_(std::move(random_gen)){}
    void AddDog(Dog dog) {
        dogs_[dog.GetId()] = std::move(dog);
    }

    Dog* FindDog(uint32_t id) {
        auto it = dogs_.find(id);
        if (it != dogs_.end()) {
            return &it->second;
        }
        return nullptr;
    }

    const Map& GetMap() const {
        return map_;
    }

    void UpdateState(double time_delta_seconds) {
        std::unordered_map<uint32_t, DogPoint> old_positions;
        old_positions.reserve(dogs_.size());
        for (auto& [id, dog] : dogs_) {
            old_positions[id] = dog.GetPosition();
        }

        for (auto& [id, dog] : dogs_) {
            MoveDog(dog, time_delta_seconds);
            dog.UpdateIdle(std::chrono::milliseconds{ static_cast<int64_t>(time_delta_seconds * 1000) });
        }
        ProcessGatherEvents(old_positions);
        SpawnLostObjects(time_delta_seconds);
    }
    const std::vector<LostObject>& GetLostObjects() const noexcept {
        return lost_objects_;
    }
    const std::unordered_map<uint32_t, Dog> GetDogs() const{
        return dogs_;
    }
    const int GetNextIdLostObj() const {
        return next_lost_object_id_;
    }
    void AddLostObj(LostObject obj) {
        lost_objects_.push_back(obj);
    }
    void SetNextIdLostObj(int id) {
        next_lost_object_id_ = id; 
    }
    void RemoveDog(uint32_t id) {
        auto it = dogs_.find(id);
        if (it != dogs_.end()) {
            dogs_.erase(it);
        }
    }
private:
    std::unordered_map<uint32_t, Dog> dogs_;
    void ProcessGatherEvents(
        const std::unordered_map<uint32_t, DogPoint>& old_positions);
    const Map& map_;

    void MoveDog(Dog& dog, double seconds);
    void HandleBoundaryCollision(Dog& dog, double time_delta_seconds);

    double FindBoundaryX(const Dog& dog, double time_delta) const {
        auto speed = dog.GetSpeed();
        auto pos = dog.GetPosition();

        double left = 0;
        double right = time_delta;
        const int COUNT_STEP = 100;
        for (int i = 0; i < COUNT_STEP; i++) {
            double mid = (left + right) / 2;
            double test_x = pos.x + speed.x * mid;
            if (map_.FindRoadContainingPoint(test_x, pos.y)) {
                left = mid;
            }
            else {
                right = mid;
            }
        }
        return pos.x + speed.x * left;
    }
    double FindBoundaryY(const Dog& dog, double time_delta) const {
        auto pos = dog.GetPosition();
        auto speed = dog.GetSpeed();

        double left = 0;
        double right = time_delta;
        const int COUNT_STEP = 100;
        for (int i = 0; i < COUNT_STEP; i++) {
            double mid = (left + right) / 2;
            double test_y = pos.y + speed.y * mid;

            if (map_.FindRoadContainingPoint(pos.x, test_y)) {
                left = mid;
            }
            else {
                right = mid;
            }
        }

        return pos.y + speed.y * left;
    }

    void SpawnLostObjects(double time_delta);
    void SpawnOneLostObject();
    size_t RandInt(size_t lo, size_t hi);
    LostPoint RandomPointOnRoad(const Road& road);

    loot_gen::LootGenerator loot_gen_;
    std::vector<LostObject> lost_objects_;//вектор всех предметов на карте
    RandomGenerator rng_;
    int next_lost_object_id_ = 0;
    
    
};

class Game {
public:
    using Maps = std::vector<Map>;
    using RandomGenerator = std::function<double()>;

    Game(std::chrono::milliseconds loot_period,
        double loot_probability)
        : loot_period_{ loot_period }
        , loot_probability_{ loot_probability }
        , rng_engine_{ std::make_shared<std::mt19937>(std::random_device{}()) }
        , rng_{ [engine = rng_engine_] {
              return std::uniform_real_distribution<>{0.0, 1.0}(*engine);
          } } {}
    void AddMap(Map map);

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }
    GameSession& CreateSessionForMap(const Map::Id& map_id);
    GameSession* FindSession(const Map::Id& map_id);
    double GetDefaultDogSpeed() const {
        return default_dog_speed_;
    }
    void SetDefaultDogSpeed(double speed) {
        default_dog_speed_ = speed;
    }
    auto& GetSession() {
        return sessions_;
    }
    const auto& GetSession() const {
        return sessions_;
    }
    const void SetRetirementTime(std::chrono::milliseconds time) {
        dog_retirement_time_ = time;
    }
    std::chrono::milliseconds GetRetirementTime() const{
        return dog_retirement_time_;
    }
private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;
    std::unordered_map<Map::Id, std::unique_ptr<GameSession>, MapIdHasher> sessions_;
    double default_dog_speed_ = 1.0;
    //храним время и вероятность
    std::chrono::milliseconds loot_period_;
    double loot_probability_;
    std::shared_ptr<std::mt19937> rng_engine_;
    RandomGenerator rng_;
    std::chrono::milliseconds dog_retirement_time_ = 60 000;
};


}  // namespace model


