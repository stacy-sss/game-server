#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include "serialization.h"
#include "application.h"
#include "logger.h"
#include <boost/json.hpp>

namespace json = boost::json;
class SerializationListener {
public:
	SerializationListener(application::Application& app,
		model::Game& game, std::filesystem::path state_file,
		std::chrono::milliseconds save_period) :
		app_(app), game_(game), state_file_(state_file),
		save_period_(save_period) {}

	void OnTick(std::chrono::milliseconds delta) {
BOOST_LOG_TRIVIAL(info) << "OnTick: delta=" << delta.count() 
                            << " elapsed=" << elapsed_.count()
                            << " save_period=" << save_period_.count();		
elapsed_ += delta;
		try {
			if (save_period_.count() > 0 && elapsed_ >= save_period_) {
				Save();
				elapsed_ = std::chrono::milliseconds{ 0 };   // сбрасываем
			}
		}
		catch (const std::exception& e) {
			json::value data_error = {
			{"code", EXIT_FAILURE},
			{"exception", e.what()}
			};
			BOOST_LOG_TRIVIAL(error) << logging::add_value(additional_data, data_error) << "save was failed";

		}
		
	}

	void Save() const {
		serialization::ApplicationRepr app_repr(app_);
		serialization::GameRepr game_repr(game_);
		auto tmp = state_file_;
		tmp += ".tmp";
		{
			std::ofstream file(tmp, std::ios::binary);
			boost::archive::text_oarchive ar(file);
			ar << app_repr << game_repr;
		}
		std::filesystem::rename(tmp, state_file_);
		BOOST_LOG_TRIVIAL(info) << "state saved to " << state_file_.string();
	}
private:
	application::Application& app_;
	model::Game& game_;
	std::filesystem::path state_file_;
	std::chrono::milliseconds save_period_;
	std::chrono::milliseconds elapsed_{ 0 };
};
