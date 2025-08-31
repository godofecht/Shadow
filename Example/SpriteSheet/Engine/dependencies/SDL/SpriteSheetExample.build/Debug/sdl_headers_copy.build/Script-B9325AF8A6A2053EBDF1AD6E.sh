#!/bin/sh
set -e
if test "$CONFIGURATION" = "Debug"; then :
  cd /Users/abhishekshivakumar/gamedev/shadow/Example/SpriteSheet/Engine/dependencies/SDL
  /opt/local/bin/cmake -E copy_if_different /Users/abhishekshivakumar/gamedev/shadow/Engine/dependencies/SDL/include/SDL_test_crc32.h /Users/abhishekshivakumar/gamedev/shadow/Example/SpriteSheet/Engine/dependencies/SDL/include/SDL2/SDL_test_crc32.h
fi
if test "$CONFIGURATION" = "Release"; then :
  cd /Users/abhishekshivakumar/gamedev/shadow/Example/SpriteSheet/Engine/dependencies/SDL
  /opt/local/bin/cmake -E copy_if_different /Users/abhishekshivakumar/gamedev/shadow/Engine/dependencies/SDL/include/SDL_test_crc32.h /Users/abhishekshivakumar/gamedev/shadow/Example/SpriteSheet/Engine/dependencies/SDL/include/SDL2/SDL_test_crc32.h
fi
if test "$CONFIGURATION" = "MinSizeRel"; then :
  cd /Users/abhishekshivakumar/gamedev/shadow/Example/SpriteSheet/Engine/dependencies/SDL
  /opt/local/bin/cmake -E copy_if_different /Users/abhishekshivakumar/gamedev/shadow/Engine/dependencies/SDL/include/SDL_test_crc32.h /Users/abhishekshivakumar/gamedev/shadow/Example/SpriteSheet/Engine/dependencies/SDL/include/SDL2/SDL_test_crc32.h
fi
if test "$CONFIGURATION" = "RelWithDebInfo"; then :
  cd /Users/abhishekshivakumar/gamedev/shadow/Example/SpriteSheet/Engine/dependencies/SDL
  /opt/local/bin/cmake -E copy_if_different /Users/abhishekshivakumar/gamedev/shadow/Engine/dependencies/SDL/include/SDL_test_crc32.h /Users/abhishekshivakumar/gamedev/shadow/Example/SpriteSheet/Engine/dependencies/SDL/include/SDL2/SDL_test_crc32.h
fi

