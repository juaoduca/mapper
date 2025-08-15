cmake_minimum_required(VERSION 3.10)
project(ecm CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(nlohmann_json 3.2.0 REQUIRED)
include_directories(include)

# --- Detect PostgreSQL ---
find_package(PostgreSQL)
if (PostgreSQL_FOUND)
    message(STATUS "PostgreSQL found: ${PostgreSQL_VERSION}")
    set(HAVE_POSTGRESQL TRUE)
else()
    message(STATUS "PostgreSQL not found, building without PGSQL support")
    set(HAVE_POSTGRESQL FALSE)
endif()

# --- Detect SQLite ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

# --- Sources ---
file(GLOB SOURCES "src/*.cpp")

# Remove Postgres connection if libpq not available
if (NOT HAVE_POSTGRESQL)
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/connection_postgres.cpp")
endif()

# --- Main executable ---
add_executable(orm main.cpp ${SOURCES})
target_include_directories(orm PRIVATE include)
target_compile_definitions(orm PRIVATE HAVE_POSTGRESQL=$<BOOL:${HAVE_POSTGRESQL}>)
target_link_libraries(orm PRIVATE nlohmann_json::nlohmann_json fcgi sqlite3)

if (HAVE_POSTGRESQL)
    target_include_directories(orm PRIVATE ${PostgreSQL_INCLUDE_DIRS})
    target_link_libraries(orm PRIVATE ${PostgreSQL_LIBRARIES} pq)
endif()

# --- Unit tests ---
enable_testing()
add_executable(map_tests tests/test_schemamanager.cpp tests/test_ddl.cpp ${SOURCES})

target_include_directories(map_tests PRIVATE include tests/external)
target_compile_definitions(map_tests PRIVATE HAVE_POSTGRESQL=$<BOOL:${HAVE_POSTGRESQL}>)
target_link_libraries(map_tests PRIVATE nlohmann_json::nlohmann_json sqlite3)

# in root CMakeLists.txt, after the map_tests target is defined
target_sources(map_tests PRIVATE tests/test_dbpool.cpp)


if (HAVE_POSTGRESQL)
    target_include_directories(map_tests PRIVATE ${PostgreSQL_INCLUDE_DIRS})
    target_link_libraries(map_tests PRIVATE ${PostgreSQL_LIBRARIES} pq)
endif()

add_test(NAME OrmSchemaManagerTests COMMAND map_tests)


