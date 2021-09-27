cmake_minimum_required(VERSION 3.10)

project(dppmusic LANGUAGES C CXX VERSION 0.1.0)

# Compiler Config
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)  

option(YTDL_BUILD_MUX "" OFF)
option(YTDL_BUILD_CLI "" OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps/ytdl)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps/dpp)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps/webm_parser)

add_executable(dppmusic
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bot.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/player.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/partial_buffer_reader.cc
)

target_include_directories(dppmusic PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/dpp/include
)

target_link_libraries(dppmusic PUBLIC
    dpp
    ytdlhttp 
    ytdlcore
    webm
)
