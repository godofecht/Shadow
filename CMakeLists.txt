cmake_minimum_required(VERSION 3.5)
project(sdl_app)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to switch between system SDL2 library and vendored SDL2 library
option(MYGAME_VENDORED "Use vendored libraries" OFF)

# Set the directories for SDL2 and SDL2_image
set(SDL2_image_DIR "${CMAKE_SOURCE_DIR}/sdl-image/build")
set(SDL2_IMAGE_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/sdl-image/include")
set(SDL2_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/sdl/include")

if(MYGAME_VENDORED)
    add_subdirectory(vendored/sdl EXCLUDE_FROM_ALL)
else()
    # Find SDL2 and SDL2_image
    find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2)
    find_package(SDL2_image CONFIG REQUIRED) # Locate SDL2_image
endif()

# Set the directory for Box2D
add_subdirectory(box2d)

# Add source files
set(SOURCE_FILES
    main.cpp
    SDLApp.cpp
    Sprite.cpp
    Scene.cpp
)

# Create the executable
add_executable(sdl_app ${SOURCE_FILES})

# SDL2::SDL2main may or may not be available
if(TARGET SDL2::SDL2main)
    target_link_libraries(sdl_app PRIVATE SDL2::SDL2main)
endif()

# Link SDL2 and SDL2_image
target_link_libraries(sdl_app PRIVATE SDL2::SDL2)

# Link SDL2_image if available
if(TARGET SDL2::SDL2_image)
    target_link_libraries(sdl_app PRIVATE 
        SDL2::SDL2_image
        d2d1
        dwrite 
    )
else()
    # If SDL2::SDL2_image isn't available, manually specify the SDL2_image library
    target_link_libraries(sdl_app PRIVATE 
        "${SDL2_image_DIR}/Release/SDL2_image.lib"
        d2d1
        dwrite 
    )
endif()

# Add include directories for Box2D, SDL2, and SDL2_image
target_include_directories(sdl_app PRIVATE 
    "${CMAKE_SOURCE_DIR}/box2d/include" 
    ${SDL2_INCLUDE_DIRS} 
    ${SDL2_IMAGE_INCLUDE_DIRS}
)

# Define NOMINMAX to prevent Windows min/max macros from interfering
target_compile_definitions(sdl_app PRIVATE NOMINMAX)


# Set the correct paths for SDL2.dll and SDL2_image.dll based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(SDL2_DLL_PATH "${CMAKE_SOURCE_DIR}/sdl/build/Debug/SDL2d.dll")
    set(SDL2_IMAGE_DLL_PATH "${CMAKE_SOURCE_DIR}/sdl-image/build/Debug/SDL2_imaged.dll")
else()
    set(SDL2_DLL_PATH "${CMAKE_SOURCE_DIR}/sdl/build/Release/SDL2.dll")
    set(SDL2_IMAGE_DLL_PATH "${CMAKE_SOURCE_DIR}/sdl-image/build/Release/SDL2_image.dll")
endif()

# Define output directory for DLLs
set(OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}")

# Print paths to verify
message("SDL2 DLL Path: ${SDL2_DLL_PATH}")
message("SDL2 Image DLL Path: ${SDL2_IMAGE_DLL_PATH}")
message("Output Directory: ${OUTPUT_DIRECTORY}")

# Copy SDL2.dll and SDL2_image.dll to the output directory after build
if (NOT MYGAME_VENDORED)
    add_custom_command(TARGET sdl_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL2_DLL_PATH}"
            "${OUTPUT_DIRECTORY}")
    add_custom_command(TARGET sdl_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL2_IMAGE_DLL_PATH}"
            "${OUTPUT_DIRECTORY}")
endif()