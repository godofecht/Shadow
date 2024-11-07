cmake_minimum_required(VERSION 3.5)
project(sdl_app)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Debug)

# Option to use vendored libraries
option(MYGAME_VENDORED "Use vendored libraries for SDL, SDL_image, and SDL_ttf" ON)
option(USE_VENDORED_RTAUDIO "Use vendored RtAudio library" ON)

# Audio API options for RtAudio
option(ENABLE_RTAUDIO_ASIO "Enable ASIO support for RtAudio" ON)
option(ENABLE_RTAUDIO_WASAPI "Enable WASAPI support for RtAudio" ON)
option(ENABLE_RTAUDIO_DS "Enable DirectSound support for RtAudio" ON)

# Set directories for SDL2, SDL2_image, SDL2_ttf, and RtAudio
set(SDL2_image_DIR "${CMAKE_SOURCE_DIR}/sdl-image/build")
set(SDL2_IMAGE_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/sdl-image/include")
set(SDL2_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/sdl/include")
set(SDL2_ttf_DIR "${CMAKE_SOURCE_DIR}/sdl_ttf/build")
set(SDL2_TTF_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/sdl_ttf/")
set(SDL2_MIXER_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/SDL2_mixer/include")
set(SDL2_mixer_DIR "${CMAKE_SOURCE_DIR}/SDL2_mixer/cmake")

# Add SDL2, SDL2_image, SDL2_ttf, and SDL2_mixer dependencies
if(MYGAME_VENDORED)
    add_subdirectory(vendored/sdl EXCLUDE_FROM_ALL)
    add_subdirectory(vendored/sdl2_mixer EXCLUDE_FROM_ALL)
    add_subdirectory(vendored/sdl2_ttf EXCLUDE_FROM_ALL)
else()
    find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2)
    find_package(SDL2_image CONFIG REQUIRED)
    find_package(SDL2_ttf CONFIG REQUIRED)
    find_package(SDL2_mixer CONFIG REQUIRED)
endif()

# Add Box2D library
add_subdirectory(box2d)

# Add RtAudio library with specified backends if using vendored RtAudio
if(USE_VENDORED_RTAUDIO)
    if(ENABLE_RTAUDIO_ASIO)
        set(RTAUDIO_WINDOWS_ASIO ON CACHE BOOL "Enable ASIO support for RtAudio" FORCE)
    endif()
    if(ENABLE_RTAUDIO_WASAPI)
        set(RTAUDIO_WINDOWS_WASAPI ON CACHE BOOL "Enable WASAPI support for RtAudio" FORCE)
    endif()
    if(ENABLE_RTAUDIO_DS)
        set(RTAUDIO_WINDOWS_DS ON CACHE BOOL "Enable DirectSound support for RtAudio" FORCE)
    endif()

    add_subdirectory(rtaudio ${CMAKE_BINARY_DIR}/rtaudio_build EXCLUDE_FROM_ALL)
    set(RTAUDIO_LIB rtaudio)
else()
    find_package(RtAudio REQUIRED)
    set(RTAUDIO_LIB RtAudio::RtAudio)
endif()

# Set source files
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

# Conditionally link SDL_mixer if available
if(TARGET SDL2::SDL2_mixer)
    target_link_libraries(sdl_app PRIVATE SDL2::SDL2_mixer)
else()
    target_link_libraries(sdl_app PRIVATE "${SDL2_mixer_DIR}/Release/SDL2_mixer.lib")
endif()

# Include SDL_mixer directories
target_include_directories(sdl_app PRIVATE ${SDL2_MIXER_INCLUDE_DIRS})

# Link libraries: SDL2, SDL2_image, SDL2_ttf, Box2D, RtAudio
target_link_libraries(sdl_app PRIVATE 
    SDL2::SDL2
    box2d
    d2d1
    dwrite
    ${RTAUDIO_LIB}
)

if(TARGET SDL2::SDL2_image)
    target_link_libraries(sdl_app PRIVATE SDL2::SDL2_image)
else()
    target_link_libraries(sdl_app PRIVATE "${SDL2_image_DIR}/Release/SDL2_image.lib")
endif()

if(TARGET SDL2::SDL2_ttf)
    target_link_libraries(sdl_app PRIVATE SDL2::SDL2_ttf)
else()
    target_link_libraries(sdl_app PRIVATE "${SDL2_ttf_DIR}/Release/SDL2_ttf.lib")
endif()

# Include directories
target_include_directories(sdl_app PRIVATE 
    "${CMAKE_SOURCE_DIR}/box2d/include" 
    ${SDL2_INCLUDE_DIRS} 
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
)

# Include RtAudio headers
if(USE_VENDORED_RTAUDIO)
    target_include_directories(sdl_app PRIVATE rtaudio/include)
else()
    target_include_directories(sdl_app PRIVATE ${RTAUDIO_INCLUDE_DIRS})
endif()

# Define NOMINMAX for Windows compatibility
target_compile_definitions(sdl_app PRIVATE NOMINMAX)

# Set DLL paths based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(SDL2_DLL_PATH "${CMAKE_SOURCE_DIR}/sdl/build/Debug/SDL2d.dll")
    set(SDL2_IMAGE_DLL_PATH "${CMAKE_SOURCE_DIR}/sdl-image/build/Debug/SDL2_imaged.dll")
    set(SDL2_TTF_DLL_PATH "${CMAKE_SOURCE_DIR}/sdl_ttf/build/Debug/SDL2_ttfd.dll")
    set(RTAUDIO_DLL_PATH "${CMAKE_SOURCE_DIR}/build/rtaudio_build/Debug/rtaudiod.dll")
    set(BOX2D_DLL_PATH "${CMAKE_SOURCE_DIR}/build/box2d/src/Debug/box2dd.dll")
    set(SDL2_MIXER_DLL_PATH "${CMAKE_SOURCE_DIR}/SDL2_mixer/cmake/Debug/SDL2_mixer.dll")
else()
    set(SDL2_DLL_PATH "${CMAKE_SOURCE_DIR}/sdl/build/Release/SDL2.dll")
    set(SDL2_IMAGE_DLL_PATH "${CMAKE_SOURCE_DIR}/sdl-image/build/Release/SDL2_image.dll")
    set(SDL2_TTF_DLL_PATH "${CMAKE_SOURCE_DIR}/sdl_ttf/build/Release/SDL2_ttf.dll")
    set(RTAUDIO_DLL_PATH "${CMAKE_SOURCE_DIR}/build/rtaudio_build/Release/rtaudio.dll")
    set(BOX2D_DLL_PATH "${CMAKE_SOURCE_DIR}/build/box2d/src/Release/box2d.dll")
    set(SDL2_MIXER_DLL_PATH "${CMAKE_SOURCE_DIR}/sdl-mixer/build/Release/SDL2_mixer.dll")
endif()

# Output directory
set(OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}")

# Copy DLLs to output directory after build
if (NOT MYGAME_VENDORED)
    add_custom_command(TARGET sdl_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL2_DLL_PATH}"
            "${OUTPUT_DIRECTORY}/SDL2.dll"
            )
    add_custom_command(TARGET sdl_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL2_IMAGE_DLL_PATH}"
            "${OUTPUT_DIRECTORY}/SDL2_image.dll"
            )
    add_custom_command(TARGET sdl_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL2_TTF_DLL_PATH}"
            "${OUTPUT_DIRECTORY}/SDL2_ttf.dll"
            )
    add_custom_command(TARGET sdl_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL2_MIXER_DLL_PATH}"
            "${OUTPUT_DIRECTORY}/SDL2_mixer.dll"
            )
    add_custom_command(TARGET sdl_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RTAUDIO_DLL_PATH}"
            "${OUTPUT_DIRECTORY}"
            )
    add_custom_command(TARGET sdl_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${BOX2D_DLL_PATH}"
            "${OUTPUT_DIRECTORY}"
            )
endif()
