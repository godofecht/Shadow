cmake_minimum_required(VERSION 3.15)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

project(TankExample)

set(CMAKE_CXX_STANDARD 17)

# Set the path to the UmbraEngine
set(UmbraEngine_DIR "${CMAKE_SOURCE_DIR}/../Engine")

# Check if the Engine directory exists before adding it
if(EXISTS "${UmbraEngine_DIR}")
    # Add the Engine subdirectory
    add_subdirectory("${UmbraEngine_DIR}" "${CMAKE_BINARY_DIR}/Engine")
    message(STATUS "UmbraEngine directory found and added: ${UmbraEngine_DIR}")
else()
    message(FATAL_ERROR "UmbraEngine directory not found: ${UmbraEngine_DIR}")
endif()

# Ensure the umbra target is set as a shared library in the UmbraEngine CMakeLists.txt
# This step assumes that UmbraEngine's CMakeLists.txt defines `add_library(umbra SHARED ...)`.



# Debug output for UmbraEngine include directories
get_target_property(UMBRA_INCLUDE_DIRS umbra INCLUDE_DIRECTORIES)
message(STATUS "Umbra include directories: ${UMBRA_INCLUDE_DIRS}")

# Glob all source files in the TankGame project directory
file(GLOB_RECURSE TANKGAME_SOURCES
    "${CMAKE_SOURCE_DIR}/*.cpp"
)

# Exclude files if necessary
list(FILTER TANKGAME_SOURCES EXCLUDE REGEX ".*excluded.*")

# Add the executable for TankExample
add_executable(${PROJECT_NAME} 
	main.cpp
)

# Include directories for the project
target_include_directories(${PROJECT_NAME} PRIVATE ${UMBRA_INCLUDE_DIRS})

# Link only the import library for umbra (assumes umbra is shared)
target_link_libraries(${PROJECT_NAME} PRIVATE umbra)

# Debug output for TankExample include directories
get_target_property(TANK_INCLUDE_DIRS ${PROJECT_NAME} INCLUDE_DIRECTORIES)
message(STATUS "TankExample include directories: ${TANK_INCLUDE_DIRS}")

# Link directly to the DLL
#target_link_libraries(${PROJECT_NAME} PRIVATE "${UmbraEngine_DIR}/Debug/umbrad.dll")



# Ensure that runtime paths for shared libraries are handled correctly
if(WIN32)
    # For Windows, copy the DLL to the output directory
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:umbra>
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
elseif(UNIX)
    # For Unix-like systems, set RPATH to find the DLL
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH $<TARGET_FILE_DIR:umbra>
    )
endif()
