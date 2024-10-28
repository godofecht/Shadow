cmake_minimum_required(VERSION 3.10)
project(SDLApp)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find SDL2
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)

# Include directories for SDL2
include_directories(${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS})

# Add the source files
set(SOURCE_FILES
    main.cpp
    SDLApp.cpp
    AssetManager.cpp
    Sprite.cpp
    CharacterControllerScript.cpp
)

# Create the executable
add_executable(sdl_app ${SOURCE_FILES})

# Link SDL2 and SDL2_image libraries
target_link_libraries(sdl_app ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES})
