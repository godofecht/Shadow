cmake_minimum_required(VERSION 3.10)
project(sdl_app)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Debug)

add_subdirectory("dependencies")



# Set source files
set(SOURCE_FILES
    main.cpp
    SDLApp.cpp
    Sprite.cpp
    Scene.cpp
)



# Create the executable
add_executable(sdl_app ${SOURCE_FILES})


# Include directories for SDL2 and dependencies
target_include_directories(sdl_app PRIVATE 
    ${SDL2_INCLUDE_DIRS} 
)

# set_target_properties(sdl_app PROPERTIES WIN32_EXECUTABLE OFF)

target_link_libraries(sdl_app PRIVATE 
    box2d
    d2d1
    dwrite
    SDL2::SDL2
    "${CMAKE_SOURCE_DIR}/dependencies/SDL_image/build-win/Debug/SDL2_imaged.lib"
    "${CMAKE_SOURCE_DIR}/dependencies/SDL_mixer/build-win/Debug/SDL2_mixerd.lib"
    "${CMAKE_SOURCE_DIR}/dependencies/SDL_ttf/build-win/Debug/SDL2_ttfd.lib"

#     ${RTAUDIO_LIB}
)



# # Set DLL paths based on build type
# if(CMAKE_BUILD_TYPE STREQUAL "Debug")
#     set(SDL2_DLL_PATH "${CMAKE_SOURCE_DIR}/dependencies/sdl/build-win/Debug/SDL2d.dll")
#     set(SDL2_IMAGE_DLL_PATH "${CMAKE_SOURCE_DIR}/dependencies/sdl_image/build-win/Debug/SDL2_imaged.dll")
#     set(SDL2_TTF_DLL_PATH "${CMAKE_SOURCE_DIR}/dependencies/sdl_ttf/build-win/Debug/SDL2_ttfd.dll")
#     set(RTAUDIO_DLL_PATH "${CMAKE_SOURCE_DIR}/build-win/rtaudio_build/Debug/rtaudiod.dll")
#     set(BOX2D_DLL_PATH "${CMAKE_SOURCE_DIR}/build-win/box2d/src/Debug/box2dd.dll")
#     set(SDL2_MIXER_DLL_PATH "${CMAKE_SOURCE_DIR}/dependencies/SDL_mixer/build-win/Debug/SDL2_mixer.dll")
# else()
#     set(SDL2_DLL_PATH "${CMAKE_SOURCE_DIR}/dependencies/sdl/build-win/Release/SDL2.dll")
#     set(SDL2_IMAGE_DLL_PATH "${CMAKE_SOURCE_DIR}/dependencies/sdl_image/build-win/Release/SDL2_image.dll")
#     set(SDL2_TTF_DLL_PATH "${CMAKE_SOURCE_DIR}/dependencies/sdl_ttf/build-win/Release/SDL2_ttf.dll")
#     set(RTAUDIO_DLL_PATH "${CMAKE_SOURCE_DIR}/build-win/rtaudio_build/Release/rtaudio.dll")
#     set(BOX2D_DLL_PATH "${CMAKE_SOURCE_DIR}/build-win/box2d/src/Release/box2d.dll")
#     set(SDL2_MIXER_DLL_PATH "${CMAKE_SOURCE_DIR}/dependencies/SDL_mixer/build-win/Release/SDL2_mixer.dll")
# endif()

# # Output directory
# set(OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Debug")

# # Copy DLLs after build
# add_custom_command(TARGET sdl_app POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#         "${SDL2_DLL_PATH}"
#         "${OUTPUT_DIRECTORY}/SDL2.dll"
# )
# add_custom_command(TARGET sdl_app POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#         "${SDL2_IMAGE_DLL_PATH}"
#         "${OUTPUT_DIRECTORY}/SDL2_image.dll"
# )
# add_custom_command(TARGET sdl_app POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#         "${SDL2_TTF_DLL_PATH}"
#         "${OUTPUT_DIRECTORY}/SDL2_ttf.dll"
# )
# add_custom_command(TARGET sdl_app POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#         "${SDL2_MIXER_DLL_PATH}"
#         "${OUTPUT_DIRECTORY}/SDL2_mixer.dll"
# )
# add_custom_command(TARGET sdl_app POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#         "${RTAUDIO_DLL_PATH}"
#         "${OUTPUT_DIRECTORY}"
# )
# add_custom_command(TARGET sdl_app POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#         "${BOX2D_DLL_PATH}"
#         "${OUTPUT_DIRECTORY}"
# )
