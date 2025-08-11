cmake_minimum_required(VERSION 3.10)
project(ecm CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(nlohmann_json 3.2.0 REQUIRED)
include_directories(include)

file(GLOB SOURCES "src/*.cpp")

add_executable(orm main.cpp ${SOURCES})
target_link_libraries(orm nlohmann_json::nlohmann_json fcgi pq sqlite3)

# Catch2 for unit testing (single header)
enable_testing()
add_executable(tests tests/test_schemamanager.cpp tests/test_ddl.cpp ${SOURCES})
target_include_directories(tests PRIVATE include tests/external)
target_link_libraries(tests nlohmann_json::nlohmann_json sqlite3)
add_test(NAME OrmSchemaManagerTests COMMAND tests)
