#include "postgres.h"
#include <pqxx/zview.hxx>

namespace postgres {
	using namespace std::literals;
	using pqxx::operator"" _zv;
	Database::Database(const std::string& db_url, size_t pool_size) :
		pool_{ pool_size , [db_url]
			{return std::make_shared<pqxx::connection>(db_url)} } {
		InitSchema();
	}
	void Database::InitSchema() {
		auto conn = pool_.GetConnection();
		pqxx::work work{ *conn };
		work.exec(R"(CREATE TABLE IF NOT EXISTS retired_players (
        id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
        name varchar(100) NOT NULL,
		score integer NOT NULL,
		play_time_ms integer NOT NULL
    );
	)"_zv);
		work.exec(R"(CREATE INDEX IF NOT EXISTS sort_for_score ON retired_players(score DESC, play_time_ms, name))"_zv);
		work.commit();
	}
	void Database::AddRetiredPlayer(const application::RetiredPlayer& player) {
		auto conn = pool_.GetConnection();
		pqxx::work work{ *conn };
		work.exec_params(R"(
		INSERT INTO retired_players (name, score, play_time_ms) VALUES ($1, $2, $3))"_zv,
			player.name, player.score, player.play_time.count());
		work.commit();
	}
	std::vector<application::RetiredPlayer> Database::GetRecords(int start, int max_items) {
		std::vector<application::RetiredPlayer> result;
		auto conn = pool_.GetConnection();
		pqxx::work work{ *conn };
		auto rows = work.exec("SELECT name, score, play_time_ms FROM retired_players ORDER BY score DESC, play_time_ms, name "
			"LIMIT $1 OFFSET $2", max_items, start);
		for (const auto& row : rows) {
			result.emplace_back(row[0].as<std::string>(),
				row[1].as<int>(), std::chrono::milliseconds{ row[2].as<int>() });
		}
		return result;
	}
}