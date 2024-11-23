cmake_minimum_required(VERSION 3.10)
#set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Project name
project(umbra)

add_definitions(-DRTAUDIO_EXPORTS)


set(CMAKE_CXX_STANDARD 17)

# Base path for dependencies
set(LIB_DIR ${CMAKE_CURRENT_LIST_DIR}/dependencies)

# Disable warnings about unsafe CRT functions
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

message(STATUS "LIB_DIR: ${LIB_DIR}")

# Define the build configuration (Debug or Release)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(BUILD_CONFIG ${CMAKE_BUILD_TYPE})

set(BUILD_DIR ${CMAKE_CURRENT_LIST_DIR}/build-win/dependencies)

# Paths to the DLLs
set(DLLS_TO_COPY
    ${BUILD_DIR}/SDL/${BUILD_CONFIG}/SDL2d.dll
    ${BUILD_DIR}/SDL_image/${BUILD_CONFIG}/SDL2_imaged.dll
    ${BUILD_DIR}/SDL_mixer/${BUILD_CONFIG}/SDL2_mixerd.dll
    ${BUILD_DIR}/SDL_ttf/${BUILD_CONFIG}/SDL2_ttfd.dll
    ${BUILD_DIR}/box2d/src/${BUILD_CONFIG}/box2dd.dll
    ${BUILD_DIR}/rtaudio/${BUILD_CONFIG}/rtaudiod.dll
)



# Directories for finding the library builds and includes
set(SDL2_INCLUDE_DIRS       ${LIB_DIR}/SDL/include)
set(SDL2_IMAGE_INCLUDE_DIRS ${LIB_DIR}/SDL_image/include)
set(SDL2_MIXER_INCLUDE_DIRS ${LIB_DIR}/SDL_mixer/include)
set(SDL2_TTF_INCLUDE_DIRS   ${LIB_DIR}/SDL_ttf)
set(BOX2D_INCLUDE_DIRS      ${LIB_DIR}/box2d/include)
set(RTAUDIO_INCLUDE_DIRS    ${LIB_DIR}/rtaudio/include)
set(ENGINE_INCLUDE_DIRS     ${CMAKE_CURRENT_LIST_DIR})

# Combined include directories
set(ALL_INCLUDE_DIRS
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
    ${BOX2D_INCLUDE_DIRS}
    ${RTAUDIO_INCLUDE_DIRS}
    ${ENGINE_INCLUDE_DIRS}
)

# Add subdirectories for dynamically linking libraries
add_subdirectory(dependencies/SDL)
add_subdirectory(dependencies/SDL_mixer)
add_subdirectory(dependencies/SDL_ttf)
add_subdirectory(dependencies/box2d)
add_subdirectory(dependencies/SDL_image)
#add_subdirectory(dependencies/rtaudio)

# Libraries to link
set(LIBRARIES_TO_LINK
    SDL2
    SDL2_image
    SDL2_mixer
    SDL2_ttf
    box2d
#    rtaudio
    d2d1
    dwrite
)

set(SOURCES     
    Scene.cpp
    Sprite.cpp
    SDLApp.cpp
)

# Add the shared library target
add_library(umbra SHARED ${SOURCES})

set_target_properties(umbra PROPERTIES DLLS_TO_COPY "${DLLS_TO_COPY}")

# Build the static library (LIB)
#add_library(umbraStatic STATIC ${SOURCES})

# Include directories for the target
target_include_directories(umbra PUBLIC ${ALL_INCLUDE_DIRS})

# Link the target with required libraries
target_link_libraries(umbra PUBLIC ${LIBRARIES_TO_LINK})

# Copy the required DLLs to the output directory
add_custom_command(TARGET umbra POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${DLLS_TO_COPY}
    $<TARGET_FILE_DIR:umbra>
)
