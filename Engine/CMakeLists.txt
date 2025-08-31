cmake_minimum_required(VERSION 3.10)

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

# Set the build directory based on the platform
if (APPLE)
    set(BUILD_DIR ${CMAKE_CURRENT_LIST_DIR}/build/dependencies)  # Normal build directory for macOS
else()
    set(BUILD_DIR ${CMAKE_CURRENT_LIST_DIR}/build-win/dependencies)  # Specific build directory for Windows
endif()

# Define library names (updated to remove '2' from folder names)
set(SDL_LIBS
    SDL
    SDL_image
    SDL_mixer
    SDL_ttf
)

set(RTAUDIO_LIB "rtaudio")

# Initialize DLLS_TO_COPY
set(DLLS_TO_COPY "")

# Construct paths for each library (updated to reflect folder names)
if (APPLE)
    list(APPEND DLLS_TO_COPY ${BUILD_DIR}/SDL/libSDL2-2.0d.dylib)
    list(APPEND DLLS_TO_COPY ${BUILD_DIR}/SDL_image/libSDL2_imaged.dylib)
    list(APPEND DLLS_TO_COPY ${BUILD_DIR}/SDL_mixer/libSDL2_mixerd.dylib)
    list(APPEND DLLS_TO_COPY ${BUILD_DIR}/SDL_ttf/libSDL2_ttfd.dylib)
else()
    list(APPEND DLLS_TO_COPY ${BUILD_DIR}/SDL/SDL2d.dll)
    list(APPEND DLLS_TO_COPY ${BUILD_DIR}/SDL_image/SDL2_imaged.dll)
    list(APPEND DLLS_TO_COPY ${BUILD_DIR}/SDL_mixer/SDL2_mixer2d.dll)
    list(APPEND DLLS_TO_COPY ${BUILD_DIR}/SDL_ttf/SDL2_ttf2d.dll)
endif()

# # Add RTAudio library path
# if (APPLE)
#     list(APPEND DLLS_TO_COPY ${BUILD_DIR}/${RTAUDIO_LIB}/${BUILD_CONFIG}/librtaudio.dylib)
# else()
#     list(APPEND DLLS_TO_COPY ${BUILD_DIR}/${RTAUDIO_LIB}/${BUILD_CONFIG}/${RTAUDIO_LIB}d.dll)
# endif()

# Add Box2D library path for macOS
if (APPLE)
    # No need to copy the static library, just link against it
    list(APPEND LIBRARIES_TO_LINK /opt/homebrew/Cellar/box2d/2.4.2/lib/libbox2d.a)  # Adjust this path if necessary
endif()

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

# Platform-specific logic for macOS
if (potato)
    # Use Homebrew-installed SDL2 and SDL_mixer on macOS
    find_package(SDL2 REQUIRED)
    find_package(SDL2_mixer REQUIRED)
    find_package(SDL2_image REQUIRED)
    add_subdirectory(dependencies/box2d)

    # Include SDL and SDL_mixer directories from Homebrew
    include_directories(${SDL2_INCLUDE_DIRS} ${SDL2_MIXER_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS})
    
    set(LIBRARIES_TO_LINK
        SDL2
        SDL2_mixer
        SDL2_image
        box2d
    )


else()
    # Windows/Linux setup
    add_subdirectory(dependencies/SDL)
    add_subdirectory(dependencies/SDL_mixer)
    add_subdirectory(dependencies/SDL_ttf)
    add_subdirectory(dependencies/box2d)
    add_subdirectory(dependencies/SDL_image)

    set(LIBRARIES_TO_LINK
        SDL2
        SDL2_image
        SDL2_mixer
        SDL2_ttf
        box2d
    )

        # Add Windows-specific libraries if on Windows
    if (WIN32)
        list(APPEND LIBRARIES_TO_LINK d2d1 dwrite)
    endif()
endif()

set(SOURCES     
    Scene.cpp
    Sprite.cpp
    SDLApp.cpp
)

# Add the shared library target
add_library(umbra SHARED ${SOURCES})

set_target_properties(umbra PROPERTIES DLLS_TO_COPY "${DLLS_TO_COPY}")

# Include directories for the target
target_include_directories(umbra PUBLIC ${ALL_INCLUDE_DIRS})

# Link the target with required libraries
target_link_libraries(umbra PUBLIC ${LIBRARIES_TO_LINK})

# Copy the required DLLs to the output directory (for Windows)
add_custom_command(TARGET umbra POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${DLLS_TO_COPY}
    $<TARGET_FILE_DIR:umbra>
)
