cmake_minimum_required(VERSION 3.12)
project(ecm CXX)

set(CMAKE_CXX_STANDARD 17)


# ---- nlohmann_json fallback (vendored single-header) ----
# Try QUIET find first; if missing, look for third_party/nlohmann/json.hpp
set(_NLOHMANN_MIN_VER 3.2.0)
find_package(nlohmann_json ${_NLOHMANN_MIN_VER} QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found via find_package; trying vendored header.")
    set(NLOHMANN_JSON_VENDORED_DIR "${CMAKE_SOURCE_DIR}/third_party")
    if(EXISTS "${NLOHMANN_JSON_VENDORED_DIR}/nlohmann/json.hpp")
        add_library(nlohmann_json INTERFACE)
        target_include_directories(nlohmann_json INTERFACE ${NLOHMANN_JSON_VENDORED_DIR})
        add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
        message(STATUS "Using vendored nlohmann_json header at ${NLOHMANN_JSON_VENDORED_DIR}")
    else()
        message(FATAL_ERROR "nlohmann_json not found. Please provide third_party/nlohmann/json.hpp or install the package.")
    endif()
endif()
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
# Re-scan src/ when files are added/removed
file(GLOB SOURCES CONFIGURE_DEPENDS "src/*.cpp")

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

# Collect all test sources matching tests/test_*.cpp (auto-refresh)
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/test_*.cpp)

foreach(test_src ${TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)
    #add_executable(${test_name} ${test_src} ${SOURCES})
    add_executable(${test_name} ${test_src} tests/main.cpp ${SOURCES})
    target_include_directories(${test_name} PRIVATE include tests/external)
    target_compile_definitions(${test_name} PRIVATE HAVE_POSTGRESQL=$<BOOL:${HAVE_POSTGRESQL}>)
    target_link_libraries(${test_name} PRIVATE nlohmann_json::nlohmann_json sqlite3)

    if (HAVE_POSTGRESQL)
        target_include_directories(${test_name} PRIVATE ${PostgreSQL_INCLUDE_DIRS})
        target_link_libraries(${test_name} PRIVATE ${PostgreSQL_LIBRARIES} pq)
    endif()

    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()


