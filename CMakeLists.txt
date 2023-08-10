cmake_minimum_required(VERSION 3.22.3) 

enable_language(C)

set(CMAKE_CXX_FLAGS_DEBUG_INIT "-Wall")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-Wall")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # don't build the supplied examples
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE) # don't build the supplied example games
FetchContent_Declare(raylib GIT_REPOSITORY https://github.com/raysan5/raylib.git GIT_TAG master)
FetchContent_MakeAvailable(raylib)

FetchContent_Declare(flecs GIT_REPOSITORY https://github.com/SanderMertens/flecs.git GIT_TAG master)
FetchContent_MakeAvailable(flecs)

project(gaming_game_gamers)

find_package(Threads REQUIRED)

add_subdirectory(src)
add_executable(${PROJECT_NAME} src/main.c)

target_link_libraries(${PROJECT_NAME} 
  raylib
  flecs
  ${CMAKE_THREAD_LIBS_INIT}
  -fsanitize=address
)

# Setting ASSETS_PATH
target_compile_definitions(${PROJECT_NAME} PUBLIC ASSET="${CMAKE_CURRENT_SOURCE_DIR}/assets/")
# Set the asset path macro to the absolute path on the dev machine
# target_compile_definitions(${PROJECT_NAME} PUBLIC ASSET="./assets") 

# Set the asset path macro in release mode to a relative path that assumes the assets folder is in the same directory as the game executable
