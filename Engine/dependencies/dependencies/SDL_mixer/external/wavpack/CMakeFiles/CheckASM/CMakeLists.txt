cmake_minimum_required(VERSION 3.30.0)
set(CMAKE_MODULE_PATH "C:/Users/abhis/gamedev/Shadow/Engine/dependencies/SDL_mixer/cmake")
project(CheckASM ASM)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/result.cmake"
  "set(CMAKE_ASM_COMPILER \"${CMAKE_ASM_COMPILER}\")\n"
  
  )