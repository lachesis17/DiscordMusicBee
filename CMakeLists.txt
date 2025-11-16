cmake_minimum_required(VERSION 3.16)
project(DiscordMusicBee)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_PREFIX_PATH "C:/Qt/qtbase-6.8/build/lib/cmake")

# Bundle mingw/gcc
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/lib
    ${CMAKE_SOURCE_DIR}/lib/discord-rpc/include
    ${CMAKE_SOURCE_DIR}/lib/discord-rpc/src
)

set(DISCORD_RPC_SRC
    lib/discord-rpc/src/discord_rpc.cpp
    lib/discord-rpc/src/discord_register_win.cpp
    lib/discord-rpc/src/rpc_connection.cpp
    lib/discord-rpc/src/connection_win.cpp
    lib/discord-rpc/src/serialization.cpp
)

set(PROJECT_SRC
    src/music_bee.cpp
)

set(RESOURCES assets/resources.qrc)
set(WIN_RESOURCES assets/resources.rc)

find_package(Qt6 COMPONENTS Widgets Core Gui REQUIRED)

add_executable(DiscordMusicBee
    ${PROJECT_SRC}
    ${DISCORD_RPC_SRC}
    ${RESOURCES}
    ${WIN_RESOURCES}
)

target_link_libraries(DiscordMusicBee
    PRIVATE Qt6::Widgets
    PRIVATE Qt6::Core
    PRIVATE Qt6::Gui
)

if(WIN32)
    set_target_properties(DiscordMusicBee PROPERTIES WIN32_EXECUTABLE YES)
endif()
