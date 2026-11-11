cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(game_server
	src/main.cpp
	src/http_server.cpp
	src/http_server.h
	src/sdk.h
	src/model.h
	src/model.cpp
	src/tagged.h
	src/boost_json.cpp
	src/json_loader.h
	src/json_loader.cpp
	src/request_handler.cpp
	src/request_handler.h
	src/logger.h
	src/logger.cpp
	src/application.cpp
	src/application.h
	src/cli_parser.h
	src/extra_data.h
	src/loot_generator.h
	src/loot_generator.cpp
	src/collision_detector.h
    	src/collision_detector.cpp 
	src/serialization.h
	src/serialization_listener.h
)
set_target_properties(game_server PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
target_link_libraries(game_server PRIVATE CONAN_PKG::boost Threads::Threads)

option(BUILD_TESTS "Build tests" ON)

if(BUILD_TESTS)
    add_executable(game_server_tests
        src/model.h
        src/model.cpp
        src/loot_generator.h
        src/loot_generator.cpp
        src/tagged.h
        src/collision_detector.h
        src/collision_detector.cpp
        tests/loot_generator_tests.cpp
    )
    target_include_directories(game_server_tests PRIVATE CONAN_PKG::boost)
    target_link_libraries(game_server_tests PRIVATE CONAN_PKG::catch2 CONAN_PKG::boost Threads::Threads)
endif()

