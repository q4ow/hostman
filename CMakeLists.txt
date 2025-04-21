cmake_minimum_required(VERSION 3.12)
project(hostman VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(HOSTMAN_USE_TUI "Build with TUI support (requires ncurses)" OFF)

find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Check if cJSON is available
if(EXISTS "/lib/libcjson.so")
    message(STATUS "Found cJSON: /lib/libcjson.so")
    set(JSON_LIBRARY "cjson")
    set(JSON_INCLUDE_DIR "/usr/include")
    add_definitions(-DUSE_CJSON)
else()
    find_package(jansson REQUIRED)
    set(JSON_LIBRARY jansson)
    set(JSON_INCLUDE_DIR ${JANSSON_INCLUDE_DIRS})
    add_definitions(-DUSE_JANSSON)
endif()

if(HOSTMAN_USE_TUI)
    find_package(Curses REQUIRED)
    add_definitions(-DUSE_TUI)
endif()

set(HOSTMAN_SOURCES
    src/main.c
    src/cli.c
    src/config.c
    src/database.c
    src/network.c
    src/encryption.c
    src/hosts.c
    src/logging.c
    src/utils.c
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CURL_INCLUDE_DIRS}
    ${SQLITE3_INCLUDE_DIRS}
    ${JSON_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
)

add_executable(hostman ${HOSTMAN_SOURCES})

target_link_libraries(hostman
    ${CURL_LIBRARIES}
    SQLite::SQLite3  # Use the proper target instead of SQLITE3_LIBRARIES
    ${JSON_LIBRARY}
    ${OPENSSL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    m
)

if(HOSTMAN_USE_TUI)
    target_link_libraries(hostman ${CURSES_LIBRARIES})
endif()

# Installation rules
install(TARGETS hostman DESTINATION bin)