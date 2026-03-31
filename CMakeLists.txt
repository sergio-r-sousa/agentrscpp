cmake_minimum_required(VERSION 3.16)

# Allow older cmake_minimum_required() in fetched dependencies.
if(POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif()

project(agentrscpp VERSION 0.1.0 LANGUAGES CXX)

# ── C++ standard ────────────────────────────────────────────────────────────

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Compiler flags (GCC) ───────────────────────────────────────────────────

add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

# ── Options ─────────────────────────────────────────────────────────────────

option(AGENTRS_BUILD_EXAMPLES "Build example executables" ON)
option(AGENTRS_BUILD_TESTS    "Build test executables"    OFF)
option(AGENTRS_ENABLE_REDIS   "Enable Redis memory backend (requires hiredis)" OFF)

# ── Dependencies ───────────────────────────────────────────────────────────

# nlohmann/json — prefer system package, fallback to FetchContent
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# yaml-cpp — prefer system package, fallback to FetchContent
find_package(yaml-cpp QUIET)
if(NOT yaml-cpp_FOUND)
    include(FetchContent)
    set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG        0.8.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(yaml-cpp)
endif()

# cpp-httplib — use local third_party copy or FetchContent
find_package(OpenSSL REQUIRED)

# Check for local copy first
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cpp-httplib/CMakeLists.txt")
    set(HTTPLIB_REQUIRE_OPENSSL ON CACHE BOOL "" FORCE)
    add_subdirectory(third_party/cpp-httplib EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    set(HTTPLIB_REQUIRE_OPENSSL ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG        v0.18.3
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(httplib)
endif()

# ── Optional: hiredis for Redis backend ────────────────────────────────────

if(AGENTRS_ENABLE_REDIS)
    find_package(hiredis REQUIRED)
    add_compile_definitions(AGENTRS_HAS_REDIS)
endif()

# ── Collect library source files ───────────────────────────────────────────

file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

# ── Static library target ─────────────────────────────────────────────────

add_library(agentrscpp STATIC ${LIB_SOURCES})

target_include_directories(agentrscpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(agentrscpp
    PUBLIC
        nlohmann_json::nlohmann_json
        yaml-cpp::yaml-cpp
        httplib::httplib
)

# Platform-specific link libraries
if(WIN32)
    target_link_libraries(agentrscpp PUBLIC ws2_32)
endif()

if(AGENTRS_ENABLE_REDIS)
    target_link_libraries(agentrscpp PUBLIC hiredis::hiredis)
endif()

# Alias for consistent usage in downstream projects
add_library(agentrs::agentrscpp ALIAS agentrscpp)

# ── Examples ───────────────────────────────────────────────────────────────

if(AGENTRS_BUILD_EXAMPLES)
    file(GLOB EXAMPLE_SOURCES CONFIGURE_DEPENDS "examples/*.cpp")

    foreach(EXAMPLE_SRC ${EXAMPLE_SOURCES})
        get_filename_component(EXAMPLE_NAME ${EXAMPLE_SRC} NAME_WE)
        add_executable(example_${EXAMPLE_NAME} ${EXAMPLE_SRC})
        target_link_libraries(example_${EXAMPLE_NAME} PRIVATE agentrscpp)
        set_target_properties(example_${EXAMPLE_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples"
        )
    endforeach()
endif()

# ── Tests ──────────────────────────────────────────────────────────────────

if(AGENTRS_BUILD_TESTS)
    enable_testing()
    file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "tests/*.cpp")

    foreach(TEST_SRC ${TEST_SOURCES})
        get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
        add_executable(test_${TEST_NAME} ${TEST_SRC})
        target_link_libraries(test_${TEST_NAME} PRIVATE agentrscpp)
        add_test(NAME ${TEST_NAME} COMMAND test_${TEST_NAME})
    endforeach()
endif()

# ── Install ────────────────────────────────────────────────────────────────

install(TARGETS agentrscpp
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/agentrs DESTINATION include)

# ── Summary ────────────────────────────────────────────────────────────────

message(STATUS "")
message(STATUS "agentrscpp v${PROJECT_VERSION}")
message(STATUS "  C++ standard:    ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build examples:  ${AGENTRS_BUILD_EXAMPLES}")
message(STATUS "  Build tests:     ${AGENTRS_BUILD_TESTS}")
message(STATUS "  Redis support:   ${AGENTRS_ENABLE_REDIS}")
message(STATUS "")
