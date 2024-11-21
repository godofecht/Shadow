cmake_minimum_required(VERSION 3.15)

project(TankExample)

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

# Glob all source files in the TankGame project directory
file(GLOB_RECURSE TANKGAME_SOURCES
    "${SOURCE_FILES}"
    "${CMAKE_SOURCE_DIR}/*.cpp"
    "${CMAKE_SOURCE_DIR}/*.h"
)

# Exclude files if necessary
list(FILTER TANKGAME_SOURCES EXCLUDE REGEX ".*excluded.*")

# Add the executable for TankExample
add_executable(${PROJECT_NAME} ${TANKGAME_SOURCES})

# Link UmbraEngine to the TankExample project
target_link_libraries(${PROJECT_NAME} PRIVATE umbra)

# Include directories for the project
target_include_directories(${PROJECT_NAME} PRIVATE
    "${UMBRA_INCLUDE_DIRS}"
)

# Debug output for TankExample include directories
get_target_property(TANK_INCLUDE_DIRS ${PROJECT_NAME} INCLUDE_DIRECTORIES)
message(STATUS "TankExample include directories: ${TANK_INCLUDE_DIRS}")

# Debug output for UmbraEngine include directories
get_target_property(UMBRA_INCLUDE_DIRS umbra INCLUDE_DIRECTORIES)
message(STATUS "Umbra include directories: ${UMBRA_INCLUDE_DIRS}")
