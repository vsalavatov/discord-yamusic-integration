cmake_minimum_required(VERSION 3.14)
project(discord_yamusic_integration)

set(CMAKE_CXX_STANDARD 17)

find_package(Boost 1.65.0 REQUIRED system coroutine thread)
IF(Boost_FOUND)
    INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
    LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})
ENDIF(Boost_FOUND)

file(GLOB discord_cpps "discord-game-sdk/cpp/*.cpp")

#add_library(discord-sdk SHARED IMPORTED)
#set_property(TARGET discord-sdk PROPERTY IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/discord-game-sdk/lib/x86_64/discord_game_sdk.so")
add_executable(discord_yamusic_integration discord_app.cpp ${discord_cpps})

target_include_directories(discord_yamusic_integration PUBLIC "${CMAKE_SOURCE_DIR}/discord-game-sdk/cpp/")
target_link_libraries(discord_yamusic_integration discord_game_sdk ${Boost_LIBRARIES}) # discord-sdk
