#include <boost/program_options.hpp>
#include <fstream>
#include "cli_parser.h"
#include <iostream>
#include <optional>
#include <vector>

namespace po = boost::program_options;
using namespace std::literals;


[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    
    po::options_description desc{ "All options"s };
    Args args;
    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value<int>()->value_name("milliseconds"), "set tick period")
        ("config-file,c", po::value(&args.config_file)->value_name("file"), "set config file path")
        ("www-root,w", po::value(&args.www_root)->value_name("dir"), "set static files root")
        ("randomize-spawn-points", "spawn dogs at random positions");

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
    return args;
}


