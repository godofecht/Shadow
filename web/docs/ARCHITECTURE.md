# Shadow Engine — Documentation

Shadow is a C++17 2D game engine built on SDL2. It provides a grid-based game framework with physics (Box2D), audio (SDL_mixer), UI, and a first-class interface for LLM-driven gameplay.

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Quick Start](#quick-start)
3. [Game Base Class](#game-base-class)
4. [Game2D — Grid-Based Games](#game2d--grid-based-games)
5. [Grid System](#grid-system)
6. [Input System](#input-system)
7. [Scene & Sprites](#scene--sprites)
8. [Physics](#physics)
9. [Audio](#audio)
10. [UI Components](#ui-components)
11. [Geometry Primitives](#geometry-primitives)
12. [LLM Interface](#llm-interface)
13. [Build & Platform Support](#build--platform-support)

---

## Architecture Overview

```
Game  (SDL window, main loop, subsystems)
 └─ Game2D  (grid-based games + LLM interface)
      ├─ Grid           (tile grid, coordinate mapping)
      │   └─ GridEntity (entities that live on the grid)
      ├─ InputManager2D (bindable key/mouse input)
      ├─ Scene          (sprite collection)
      │   └─ SimpleSprite (renderable object with parts & scripts)
      ├─ PhysicsManager (Box2D world wrapper)
      ├─ AudioEngine    (SDL_mixer groups & players)
      └─ UI             (TextDisplay, Button, GameStats)
```

Subsystem ownership lives in `Game`. `Game2D` extends `Game` and adds the grid layer, entity management, input binding shortcuts, and the `LLMPlayable` interface.

---

## Quick Start

The minimal pattern for a grid-based game:

```cpp
#include "Game2D.h"

class MyGame : public Game2D {
public:
    MyGame() : Game2D("My Game", 800, 600, /*tileSize=*/20) {}

    void initGame() override {
        createGrid(40, 30, 20);           // 40×30 tiles, 20px each

        player = createEntity<GridEntity>(getGrid(), 5, 5);
        player->setColor({0, 200, 255, 255});

        bindKey(KEY_UP).onPress([this]{ player->tryMove(0, -1); });
        bindKey(KEY_DOWN).onPress([this]{ player->tryMove(0,  1); });
        bindKey(KEY_LEFT).onPress([this]{ player->tryMove(-1, 0); });
        bindKey(KEY_RIGHT).onPress([this]{ player->tryMove( 1, 0); });
    }

private:
    std::shared_ptr<GridEntity> player;
};

int main() {
    MyGame game;
    game.run();
}
```

---

## Game Base Class

**Header:** `SDLApp.h`

`Game` owns the SDL window, renderer, physics manager, audio engine, asset manager, and scene list. It runs the main loop and calls your overrides.

### Constructor

```cpp
Game(const char* title, int width, int height);
```

### Overridable hooks

| Method | When called |
|--------|-------------|
| `onStart()` | Once, before `initializeComponents()` — good for audio group setup |
| `initializeComponents()` | Once, after SDL is ready — build scenes and load assets here |
| `update()` | Every frame |
| `renderPostFX()` | After `update()`, for full-screen post-processing |

### Subsystem accessors

```cpp
Renderer*       getRenderer();
AssetManager*   getAssetManager();
AudioEngine*    getAudioEngine();
PhysicsManager* getPhysicsManager();
```

### Scene management

```cpp
void addScene(std::unique_ptr<Scene>&& scene);
```

### FPS

```cpp
void limitFPS(Uint32 fpsLimit);   // No-op under Emscripten (browser controls timing)
void drawFPS();                    // Render FPS counter to screen
```

---

## Game2D — Grid-Based Games

**Header:** `Game2D.h`

`Game2D` is the recommended base class for 2D tile-based games. It wraps `Game` and adds a grid, entity list, input shortcuts, UI helpers, and the `LLMPlayable` interface.

### Constructor

```cpp
Game2D(const char* title, int width, int height, int tileSize = 20);
```

### Required override

```cpp
virtual void initGame() = 0;   // Must be implemented — set up your grid, entities, input
```

### Optional overrides

```cpp
virtual void updateGame(float dt) {}   // Called every frame with delta time in seconds
virtual void renderGame()         {}   // Called after the grid renders each frame
virtual void onGameOver()         {}   // Called once when endGame() is triggered
```

### Grid

```cpp
void  createGrid(int width, int height, int tileSize = 20);
Grid* getGrid() const;
void  renderGrid();
```

Create the grid once in `initGame()`. `renderGrid()` is called automatically; call it manually only for custom render ordering.

### Entity management

```cpp
template<typename T, typename... Args>
std::shared_ptr<T> createEntity(Args&&... args);

void removeInactiveEntities();   // Prune entities where isActive() == false
```

`T` must derive from `GridEntity`. Entities are stored and updated automatically.

### Game state

```cpp
void startGame();
void endGame();                          // Sets gameOver = true, calls onGameOver()
bool isGameRunning() const;
float getDeltaTime() const;              // Frame delta in seconds
float getGameTime()  const;              // Total elapsed game time in seconds
```

### Input shortcuts

```cpp
InputBinding& bindKey(KeyCode key);
InputBinding& bindMouse(MouseButton button);
```

Shorthand for `input.bind(...)` — see [Input System](#input-system).

### UI helpers

```cpp
std::shared_ptr<TextDisplay> createText(int x, int y, const std::string& text = "");
std::shared_ptr<Button>      createButton(int x, int y, int w, int h, const std::string& label = "");
std::shared_ptr<GameStats>   createStats(int x, int y);
```

### Grid coloring helper

```cpp
void setGridColors(int value0, int value1, SDL_Color color0, SDL_Color color1);
```

Assigns `color0` to all cells with `value == value0`, and `color1` to all cells with `value == value1`.

---

## Grid System

**Header:** `Grid.h`

### GridCell

Each cell in the grid is a `GridCell`:

```cpp
struct GridCell {
    int       value       = 0;
    bool      isSolid     = false;
    bool      isVisible   = true;
    SDL_Color color       = {255, 255, 255, 255};
    SDL_Color borderColor = {0, 0, 0, 255};
};
```

### Grid

```cpp
Grid(int width, int height, int tileSize = 20);
```

#### Cell access

```cpp
GridCell&       cell(int x, int y);
const GridCell& cell(int x, int y) const;
int  getValue(int x, int y) const;
void setValue(int x, int y, int value);
```

#### Dimensions

```cpp
int getWidth()    const;
int getHeight()   const;
int getTileSize() const;
```

#### Coordinate conversion

```cpp
Rect<float> getCellRect(int x, int y) const;
void screenToGrid(int screenX, int screenY, int& gridX, int& gridY) const;
void gridToScreen(int gridX,   int gridY,   int& screenX, int& screenY) const;
```

#### Bounds

```cpp
bool isInBounds(int x, int y) const;
```

#### Rendering

```cpp
void render(Renderer* renderer);
void setCellColor(int x, int y, SDL_Color color);
void setBorderColor(SDL_Color color);        // Set uniform border color for all cells
void fill(SDL_Color color);                  // Paint every cell the same color
void clear();                                // Reset all cells to defaults
```

#### TileMap export

```cpp
std::vector<std::vector<float>> toTileMapData() const;
```

---

### GridEntity

Entities that move on the grid. Extend this to add per-entity logic.

```cpp
GridEntity(Grid* grid, int x, int y);
```

#### Position

```cpp
int  getX() const;
int  getY() const;
void setPosition(int x, int y);
void move(int dx, int dy);             // Unconditional move (no collision check)
bool tryMove(int dx, int dy);          // Returns false if destination is out of bounds or solid
bool canMoveTo(int x, int y) const;
```

#### Visual

```cpp
SDL_Color getColor()  const;
void      setColor(SDL_Color c);
char      getSymbol() const;           // ASCII char for text/LLM rendering
void      setSymbol(char s);
```

#### Lifecycle

```cpp
virtual void update(float deltaTime) {}
virtual void render(Renderer* renderer);
bool isActive()        const;
void setActive(bool a);                // Set false to queue for removal
```

---

## Input System

**Header:** `InputManager.h`

### Key codes

```
KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT
KEY_W, KEY_S, KEY_A, KEY_D
KEY_SPACE, KEY_ENTER, KEY_ESCAPE
KEY_R, KEY_P, KEY_TAB
KEY_0 … KEY_9
```

### Mouse buttons

```
MOUSE_LEFT, MOUSE_RIGHT, MOUSE_MIDDLE
```

### InputBinding — fluent API

`bindKey` and `bindMouse` return an `InputBinding&` that supports method chaining:

```cpp
bindKey(KEY_SPACE)
    .onPress(  [this]{ jump(); }  )
    .onHold(   [this]{ charge(); })
    .onRelease([this]{ release(); });
```

| Method | Trigger |
|--------|---------|
| `onPress(cb)`   | First frame the key goes down |
| `onHold(cb)`    | Every frame the key is held |
| `onRelease(cb)` | First frame the key comes up |

### Direct state queries (InputManager2D)

```cpp
bool isKeyPressed(KeyCode key)   const;   // Currently held
bool isKeyHeld(KeyCode key)      const;
bool wasKeyJustPressed(KeyCode key) const; // Pressed this frame only
bool isMousePressed(MouseButton button) const;
void getMousePosition(int& x, int& y) const;

int mouseX, mouseY;   // Public, updated every frame
```

---

## Scene & Sprites

**Header:** `Scene.h`, `Sprite.h`

### Scene

A scene is an ordered collection of `SimpleSprite` objects. `Game` renders all active scenes each frame.

```cpp
void addItem(std::shared_ptr<SimpleSprite> sprite);
std::shared_ptr<SimpleSprite> getSpriteById(const std::string& id);
void render(Renderer* renderer);
void setAssetManager(AssetManager* am);
```

### SimpleSprite

A renderable object with an optional background texture, child `Part`s, and attached `Script`s.

```cpp
// Constructors
SimpleSprite(Renderer* renderer, const std::string& imagePath, const std::string& id);
SimpleSprite(Renderer* renderer,                                const std::string& id);

// Image
void setImage(const std::string& path);

// Scripts
void attachScript(std::shared_ptr<Script> script);
std::vector<std::shared_ptr<Script>>& getScripts();

// Hierarchical parts
Part* addPart(const std::string& imagePath, const std::string& id);
Part* getPart(const std::string& id);

// Physics component
template<typename T>
void addComponent(PhysicsManager* physicsManager);   // T = PhysicsComponent

// Lifecycle
void setActive(bool state);
void destroy();
virtual void update(float deltaTime) {}
Scene* getScene() const;
```

### Sprite utility functions

```cpp
Vector2D calculateDirection(const Point2D& origin, const Point2D& target);
float    calculateAngle(const Point2D& delta);        // Degrees
Point2D  getMousePosition();
Point2D  getSpriteCenter(SimpleSprite* sprite);
```

---

## Physics

**Header:** `Physics.h`
**Dependency:** Box2D v3

### PhysicsManager

Top-level owner of the physics world. Retrieved via `Game::getPhysicsManager()`.

```cpp
World& getWorld();
```

### World

Wraps a `b2World` and manages simulation stepping and collision events.

```cpp
b2WorldId getId();
void simulateStep();              // Advances simulation by 1/60 s, fires onCollision callbacks
void addBody(Body* body);
void deleteBodyWithId(const std::string& uid);
```

Default gravity is `(0, 10)` (downward).

### Body

Base class for physics bodies.

```cpp
b2BodyId  bodyId;
b2BodyDef bodyDef;
std::function<void()> onCollision;   // Fired on contact begin

void       setUid(const std::string& uid);
std::string getUid();
b2BodyId   getId();
```

### RigidBody

A dynamic box body, 1×1 units, density 1.0, friction 0.3. Created and added to the world automatically.

```cpp
RigidBody(World& world);
```

### PhysicsObject

Convenience class for placing static or dynamic boxes at an arbitrary position/size.

```cpp
PhysicsObject(World& world, float x, float y, float w, float h, bool isDynamic);
```

### Attaching physics to a sprite

```cpp
sprite->addComponent<PhysicsComponent>(getPhysicsManager());
Body* body = static_cast<PhysicsComponent*>(sprite->components.back())->getBody();
```

---

## Audio

**Header:** `AudioEngine.h`
**Dependency:** SDL_mixer

Audio is organized in **groups** containing named **players**. Initialize in `onStart()`, load files in `initializeComponents()`.

### AudioEngine

Retrieved via `Game::getAudioEngine()`.

```cpp
bool initialize();   // Opens SDL_mixer at 44100 Hz, stereo, 2048-byte buffer

void addAudioMediaGroup(const std::string& id);
AudioMediaGroup& getAudioMediaGroupById(const std::string& id);
AudioMediaGroup& getAudioMediaGroupByIndex(int index);

void playAudioInGroup(const std::string& groupId, const std::string& audioId, bool loop = false);
void stopAudioInGroup(const std::string& groupId, const std::string& audioId);
void stopAudioInAllGroups();
```

### AudioMediaGroup

```cpp
void addAudioPlayer(const std::string& filePath, const std::string& id);
void playAudio(const std::string& id, bool loop = false);
void stopAudio(const std::string& id);
```

### AudioPlayer

Supports WAV (chunk) and MP3/OGG (streamed music). Detected automatically by extension.

```cpp
AudioPlayer(const std::string& filePath, const std::string& id);
void play(bool loop = false);
void stop();
```

### Example

```cpp
// In onStart():
getAudioEngine()->initialize();
getAudioEngine()->addAudioMediaGroup("sfx");

// In initializeComponents():
getAudioEngine()->getAudioMediaGroupById("sfx")
    .addAudioPlayer("assets/jump.wav", "jump");

// In-game:
getAudioEngine()->playAudioInGroup("sfx", "jump");
```

---

## UI Components

**Header:** `UI.h`

All UI components render using SDL's built-in drawing — no font file required for `TextDisplay` and `GameStats` (they use `drawSimpleText`). `Button` requires mouse state to be passed each frame.

### TextDisplay

```cpp
TextDisplay(int x, int y, const std::string& text = "");

void setText(const std::string& text);
const std::string& getText() const;
void setPosition(int x, int y);
void setColor(SDL_Color color);
void render(Renderer* renderer);

// Stream-style append
display << "Score: " << 100;
```

### Button

```cpp
Button(int x, int y, int w, int h, const std::string& text);

void update(int mouseX, int mouseY, bool pressed);  // Call every frame
void render(Renderer* renderer);
bool isClicked() const;

void setLabel(const std::string& label);
void setPosition(int x, int y);
void setSize(int w, int h);
```

### GameStats

Key/value integer stats display.

```cpp
GameStats(int x, int y);

void setStat(const std::string& key, int value);
int  getStat(const std::string& key) const;
void addStat(const std::string& key, int delta);   // Increment/decrement
void render(Renderer* renderer);
```

### ExplanationOverlay

Multi-line text box, useful for debug overlays.

```cpp
ExplanationOverlay(int x, int y, int width);

void addLine(const std::string& line);
void clear();
void render(Renderer* renderer);
```

---

## Geometry Primitives

**Header:** `Geometry.h`

### Point2D

```cpp
Point2D(float x = 0, float y = 0);

Point2D operator+(const Point2D& other) const;
Point2D operator-(const Point2D& other) const;
Point2D operator-()                     const;   // Negate
bool    operator==(const Point2D& other) const;
void    translate(const Point2D& delta);
```

### Vector2D

A direction/magnitude represented as a `Point2D origin`.

```cpp
Vector2D(float x = 0, float y = 0);

Vector2D  normalized()               const;
Vector2D  operator*(float scalar)    const;
Vector2D  operator+(const Vector2D&) const;
Vector2D  operator-(const Vector2D&) const;
Vector2D& operator+=(const Vector2D&);
Vector2D& operator-=(const Vector2D&);
bool      operator==(const Vector2D&) const;
bool      operator!=(const Vector2D&) const;
```

### Vector3

```cpp
Vector3(float x = 0, float y = 0, float z = 0);

Vector3 operator+(const Vector3&) const;
Vector3 operator-(const Vector3&) const;
Vector3 operator*(float scalar)   const;
```

### Matrix4x4

Column-major 4×4 matrix with static factory methods.

```cpp
static Matrix4x4 identity();
static Matrix4x4 rotationX(float angle);    // Radians
static Matrix4x4 rotationY(float angle);
static Matrix4x4 rotationZ(float angle);
static Matrix4x4 translation(float x, float y, float z);
static Matrix4x4 projection(float fov, float aspect, float near, float far);

Matrix4x4 operator*(const Matrix4x4& other) const;
Vector3   multiplyVector(const Vector3& v)  const;
```

### Rect\<T\>

```cpp
Rect<T>(T x, T y, T width, T height);

T x, y, width, height;
Point2D center, topLeft, topRight, bottomLeft, bottomRight;
void updatePoints();   // Recompute corners after mutating x/y/width/height
```

---

## LLM Interface

**Header:** `Engine/Core/GameState.h`, `Game2D.h`

Any `Game2D` subclass automatically implements `LLMPlayable`. Use this to drive your game programmatically — from an LLM, an AI agent, or automated tests.

### Registering actions

Call `registerAction` inside `initGame()`:

```cpp
registerAction("move_up", [this]() -> ActionResult {
    if (player->tryMove(0, -1))
        return {true, "Moved up"};
    return {false, "Blocked"};
});
```

`ActionResult` has two fields: `bool success` and `std::string message`.

### LLMPlayable interface

```cpp
GameState                    getState()             const override;
ActionResult                 executeAction(const std::string& action) override;
std::vector<std::string>     getAvailableActions()  const override;
void                         reset()                      override;
bool                         isGameOver()           const override;
bool                         isGameWon()            const override;
```

### GameState

```cpp
struct GameState {
    bool   gameRunning;
    bool   gameOver;
    int    score;
    int    level;
    int    gridWidth, gridHeight;
    std::vector<std::vector<int>>              grid;       // 2D cell values
    std::unordered_map<std::string, Point2D>   entities;   // name → position
    std::vector<std::string>                   availableActions;

    std::string toString() const;   // ASCII art representation
    std::string toJSON()   const;   // JSON representation
};
```

#### Text output format

```
=== GAME STATE ===
Status: PLAYING
Score: 100
Level: 2

=== GRID ===
....................
..#.................
..$.................

=== ENTITIES ===
player: (5, 10)
food: (3, 6)

=== AVAILABLE ACTIONS ===
  - up
  - down
  - left
  - right
==================
```

#### JSON output format

```json
{
  "gameRunning": true,
  "gameOver": false,
  "score": 100,
  "level": 2,
  "gridWidth": 20,
  "gridHeight": 20,
  "availableActions": ["up", "down", "left", "right"]
}
```

### Driving the game from C++

```cpp
MyGame game;
game.run();   // Not needed for headless — call onStart() + initializeComponents() instead

while (!game.isGameOver()) {
    GameState state   = game.getState();
    std::string action = myAI.chooseAction(state.toString(),
                                           game.getAvailableActions());
    ActionResult result = game.executeAction(action);
}
```

### Games with built-in LLM support

| Game | Executable | Actions |
|------|------------|---------|
| Snake | `snake_example` | up, down, left, right, restart |
| Minesweeper | `minesweeper_example` | reveal, flag, restart |
| Tic Tac Toe | `tictactoe_example` | place_X, place_O, restart |
| Roguelike | `roguelike_example` | up, down, left, right, attack, restart |

---

## Build & Platform Support

### Prerequisites

**macOS / Linux**
```bash
brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer   # macOS
# or: apt install libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev
```

**Windows** — use the vendored SDL2 binaries in `dependencies/` (set `-DMYGAME_VENDORED=ON`).

### Build

```bash
cd Shadow
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DMYGAME_VENDORED=OFF
cmake --build . -j$(nproc)
```

Outputs:
- `sdl_app` — default entry point (runs `main.cpp`)
- `sdl_app_tests` — unit test suite
- `snake_example`, `minesweeper_example`, `tictactoe_example`, `roguelike_example`

### Run

```bash
./sdl_app            # Requires a display
./sdl_app_tests      # Headless unit tests
ctest --output-on-failure
```

### WebAssembly (Emscripten)

Shadow supports Emscripten. The main loop uses `emscripten_set_main_loop` automatically when `__EMSCRIPTEN__` is defined. `limitFPS` is a no-op in WASM — the browser controls frame timing.

```bash
emcmake cmake .. -DCMAKE_BUILD_TYPE=Release
emmake make -j$(nproc)
```

### Platforms

| Platform | Status |
|----------|--------|
| macOS | Supported |
| Linux | Supported |
| Windows | Supported (vendored deps) |
| WebAssembly | Supported (Emscripten) |

### CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `MYGAME_VENDORED` | `OFF` | Use vendored SDL2 from `dependencies/` |
| `CMAKE_BUILD_TYPE` | `Debug` | `Debug` or `Release` |

---

## Common Patterns

### Fixed-rate game logic (e.g. snake tick)

```cpp
float tickTimer = 0;
const float TICK_RATE = 0.15f;  // seconds per move

void updateGame(float dt) override {
    tickTimer += dt;
    if (tickTimer >= TICK_RATE) {
        tickTimer = 0;
        snake->advanceOneStep();
    }
}
```

### Marking a cell solid

```cpp
getGrid()->cell(x, y).isSolid = true;   // GridEntity::tryMove() will block on these
```

### Updating a score display

```cpp
auto scoreText = createText(10, 10, "Score: 0");

// In updateGame():
scoreText->setText("Score: " + std::to_string(score));
```

### Adding collision response

```cpp
RigidBody* rb = new RigidBody(getPhysicsManager()->getWorld());
rb->onCollision = [this]() { handleHit(); };
```
