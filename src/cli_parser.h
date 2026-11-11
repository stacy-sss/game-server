#pragma once
#include <boost/program_options.hpp>
#include <optional>
#include <string>
#include <chrono>
#include <iostream>

namespace po = boost::program_options;
using namespace std::literals;

struct Args {
    std::optional<std::chrono::milliseconds> tick_period;
    std::string config_file;
    std::string www_root;
    bool randomize_spawn_points = false;
    bool help = false;
    std::string state_file;
    std::optional<int> save_state_period;
};

// Объявление функции парсинга

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {

    po::options_description desc{ "All options"s };
    Args args;
    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value<int>()->value_name("milliseconds"), "set tick period")
        ("config-file,c", po::value(&args.config_file)->value_name("file"), "set config file path")
        ("www-root,w", po::value(&args.www_root)->value_name("dir"), "set static files root")
        ("randomize-spawn-points", "spawn dogs at random positions")
        ("state-file", po::value(&args.state_file)->value_name("file"), "state file path")
        ("save-state-period", po::value<int>()->value_name("milliseconds"), "save period in ms");


    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        args.help = true;
        return args;
    }

    if (!vm.count("config-file")) {
        throw std::runtime_error("Config file path is required (--config-file or -c)");
    }

    if (!vm.count("www-root")) {
        throw std::runtime_error("WWW root directory is required (--www-root or -w)");
    }

    if (vm.count("tick-period")) {
        args.tick_period = std::chrono::milliseconds(vm["tick-period"].as<int>());
    }

    if (vm.count("randomize-spawn-points")) {
        args.randomize_spawn_points = true;
    }

    if (vm.count("state-file")) {
        args.state_file = vm["state-file"].as<std::string>();
    }
    if (vm.count("save-state-period")) {
        args.save_state_period = vm["save-state-period"].as<int>();
    }
    return args;
}
