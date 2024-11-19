cmake_minimum_required(VERSION 3.10)

# Base path for dependencies
set(LIB_DIR ${CMAKE_CURRENT_LIST_DIR})

# Disable warnings about unsafe CRT functions
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

message(STATUS "LIB_DIR: ${LIB_DIR}")

# Define the build configuration (Debug or Release)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(BUILD_CONFIG ${CMAKE_BUILD_TYPE})

# Paths to the DLLs
set(SDL2_DLL       ${LIB_DIR}/SDL/build-win/${BUILD_CONFIG}/SDL2.dll)
set(SDL2_IMAGE_DLL ${LIB_DIR}/SDL_image/build-win/${BUILD_CONFIG}/SDL2_image.dll)
set(SDL2_MIXER_DLL ${LIB_DIR}/SDL_mixer/build-win/${BUILD_CONFIG}/SDL2_mixer.dll)
set(SDL2_TTF_DLL   ${LIB_DIR}/SDL_ttf/build-win/${BUILD_CONFIG}/SDL2_ttf.dll)
set(BOX2D_DLL      ${LIB_DIR}/box2d/build/${BUILD_CONFIG}/box2d.dll)
set(RTAUDIO_DLL    ${LIB_DIR}/rtaudio/build-win/${BUILD_CONFIG}/rtaudio.dll)

# Directories for finding the library builds and includes
set(SDL2_image_DIR          "${LIB_DIR}/SDL_image/build-win")
set(SDL2_IMAGE_INCLUDE_DIRS "${LIB_DIR}/SDL_image/include")
set(SDL2_INCLUDE_DIRS       "${LIB_DIR}/SDL/include")
set(SDL2_ttf_DIR            "${LIB_DIR}/SDL_ttf/build-win")
set(SDL2_TTF_INCLUDE_DIRS   "${LIB_DIR}/SDL_ttf/include")
set(SDL2_MIXER_INCLUDE_DIRS "${LIB_DIR}/SDL_mixer/include")
set(SDL2_mixer_DIR          "${LIB_DIR}/SDL_mixer/build-win")
set(box2d_DIR               "${LIB_DIR}/box2d/build")
set(rtaudio_DIR             "${LIB_DIR}/rtaudio/build-win")

# Include directories for the headers
set(SDL2_INCLUDE_DIRS 
    ${LIB_DIR}/SDL/include
    ${LIB_DIR}/SDL_image/include
    ${LIB_DIR}/SDL_mixer/include
    ${LIB_DIR}/SDL_ttf
    ${LIB_DIR}/box2d/include
    ${LIB_DIR}/rtaudio/include
    PARENT_SCOPE
)

# Custom command to copy DLLs to the output directory
set(DLLS_TO_COPY
    ${SDL2_DLL}
    ${SDL2_IMAGE_DLL}
    ${SDL2_MIXER_DLL}
    ${SDL2_TTF_DLL}
    ${BOX2D_DLL}
    ${RTAUDIO_DLL}
    PARENT_SCOPE
)

# Add subdirectories for dynamically linking libraries
add_subdirectory(SDL)
add_subdirectory(SDL_mixer)
add_subdirectory(SDL_ttf)
add_subdirectory(box2d)
add_subdirectory(SDL_image)
# add_subdirectory(rtaudio)

# Set libraries to link manually if needed
set(LIBRARIES_TO_LINK
    SDL2
    SDL2_image
    SDL2_mixer
    SDL2_ttf
    box2d
    # rtaudio
    PARENT_SCOPE
)
