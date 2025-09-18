cmake_minimum_required(VERSION 3.12)
project(ecm LANGUAGES CXX)

# This will place all executables (main and tests) into the /build folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/build")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE INCLUDE_SUBDIRS "${CMAKE_CURRENT_SOURCE_DIR}/include/*/")

include_directories(include include/query )


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

# --- Sources (auto-refresh on new/removed files) ---
file(GLOB SOURCES CONFIGURE_DEPENDS "src/*.cpp")

# If libpq absent, drop PG-specific source
if (NOT HAVE_POSTGRESQL)
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/connection_postgres.cpp")
endif()

# ---- Core library: single source of truth for src/*.cpp ----
add_library(orm_core STATIC ${SOURCES})
target_include_directories(orm_core PUBLIC include rapidjson)
target_compile_definitions(orm_core PRIVATE HAVE_POSTGRESQL=$<BOOL:${HAVE_POSTGRESQL}>)
target_link_libraries(orm_core
    # PRIVATE rapidjson
    # PRIVATE nlohmann_json::nlohmann_json
    PUBLIC  sqlite3
)
if (HAVE_POSTGRESQL)
    target_include_directories(orm_core PRIVATE ${PostgreSQL_INCLUDE_DIRS})
    target_link_libraries(orm_core PUBLIC ${PostgreSQL_LIBRARIES} pq)
endif()

# ---- Main executable ----
add_executable(orm main.cpp)
target_link_libraries(orm PRIVATE orm_core fcgi)

# ---- Unit tests ----
enable_testing()
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/test_*.cpp tests/query/test_*.h)

foreach(test_src ${TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src} tests/main.cpp)
    target_include_directories(${test_name} PRIVATE include tests/external)
    target_compile_definitions(${test_name} PRIVATE HAVE_POSTGRESQL=$<BOOL:${HAVE_POSTGRESQL}>)
    target_link_libraries(${test_name} PRIVATE orm_core)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
