cmake_minimum_required(VERSION 3.12)
project(hostman VERSION 1.1.3 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(HOSTMAN_USE_TUI "Build with TUI support (requires ncurses)" OFF)

find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

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

set(HOSTMAN_CORE_SOURCES
    src/core/config.c
    src/core/logging.c
    src/core/utils.c)

set(HOSTMAN_CLI_SOURCES
    src/cli/cli.c)

set(HOSTMAN_NETWORK_SOURCES
    src/network/network.c
    src/network/hosts.c)

set(HOSTMAN_CRYPTO_SOURCES
    src/crypto/encryption.c)

set(HOSTMAN_STORAGE_SOURCES
    src/storage/database.c)

set(HOSTMAN_SOURCES
    src/main.c
    ${HOSTMAN_CORE_SOURCES}
    ${HOSTMAN_CLI_SOURCES}
    ${HOSTMAN_NETWORK_SOURCES}
    ${HOSTMAN_CRYPTO_SOURCES}
    ${HOSTMAN_STORAGE_SOURCES})

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CURL_INCLUDE_DIRS}
    ${SQLITE3_INCLUDE_DIRS}
    ${JSON_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
)

add_executable(hostman ${HOSTMAN_SOURCES})

target_link_libraries(hostman
    ${CURL_LIBRARIES}
    SQLite::SQLite3
    ${JSON_LIBRARY}
    ${OPENSSL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    m)

if(HOSTMAN_USE_TUI)
    target_link_libraries(hostman ${CURSES_LIBRARIES})
endif()

install(TARGETS hostman DESTINATION bin)
install(DIRECTORY include/hostman DESTINATION include)

option(BUILD_DOCS "Build documentation" OFF)
if(BUILD_DOCS)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
        set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
        configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
        add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM)
    endif()
endif()

find_program(CLANG_FORMAT "clang-format")
if(CLANG_FORMAT)
    message(STATUS "Found clang-format: ${CLANG_FORMAT}")
    
    file(GLOB_RECURSE ALL_SOURCE_FILES
        "${CMAKE_SOURCE_DIR}/src/*.c"
        "${CMAKE_SOURCE_DIR}/src/*.h"
        "${CMAKE_SOURCE_DIR}/include/hostman/*.h"
    )
    
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i -style=file ${ALL_SOURCE_FILES}
        COMMENT "Formatting source code with clang-format"
        VERBATIM)
    
    add_custom_target(check-format
        COMMAND ${CLANG_FORMAT} -style=file -output-replacements-xml ${ALL_SOURCE_FILES} | grep -c "<replacement " > /dev/null && echo "Code formatting issues found" || echo "Code formatting check passed"
        COMMENT "Checking code formatting with clang-format"
        VERBATIM)
else()
    message(STATUS "clang-format not found, format target will not be available")
    add_custom_target(format
        COMMAND echo "clang-format not found, please install it to enable automatic formatting"
        VERBATIM)
endif()