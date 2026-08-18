# Game Dev Guide — from zero to a shipped game in ~30 minutes

This guide walks the template game (`Games/_template/`, playable as
"Coin Collector") line by line, then hands you the API you need to make
your own. Read it once, keep it next to you while you build #6 through
#100 in GAMES.md.

Building an engine feature demo instead of a game? `Examples/` is
registered manually (one `add_umbra_example()` line) and enforced by
`make check-examples` — see [Examples/README.md](Examples/README.md).

There are three starter templates — pick the one that matches your game:

| Flag | Starter | Patterns pre-wired |
|---|---|---|
| *(default)* | `Games/_template` — Coin Collector | grid movement, entities, restart |
| `--twin-stick` | `Games/_twin_stick` — arena shooter | continuous movement, mouse aim, projectiles, enemy waves |
| `--platformer` | `Games/_platformer` — jump-and-run | gravity, jumping, solid-cell collision, patrol enemies, goal |

---

## 1. Make your game (2 minutes)

```bash
make new-game GAME=Pong                  # collector starter
make new-game GAME=Blaster --twin-stick  # arena shooter starter
make new-game GAME=Runner --platformer   # jump-and-run starter
make game GAME=Pong
./build-games/Pong_game
```

That's the entire pipeline:

- `tools/new_game.sh` copies the chosen `Games/_*` starter to `Games/Pong/`
- CMake **auto-registers** any `Games/<name>/main.cpp` as a
  `<name>_game` target (`Games/_template/` is special-cased as
  `template_game` so the starter game is always built and CI-tested) —
  no CMake edits, ever
- `make game GAME=Pong` builds just that target

Web version, when you're ready: `./tools/build_web_game.sh Pong`
→ `web/Pong.js`, then serve `web/` with any static server.

## 2. Template anatomy (10 minutes)

Open `Games/Pong/main.cpp`. The whole game is one class inheriting `Game2D`:

```cpp
class CoinCollector : public Game2D {
    // Tunables first - every magic number up top
    static constexpr int GRID_W = 32;

    // World - pointers to the things that exist
    std::shared_ptr<GridEntity> player;
    std::vector<std::shared_ptr<GridEntity>> coins;

    // State - the numbers that change
    int score = 0;

public:
    CoinCollector() : Game2D("Coin Collector", 700, 540, 20) {}

    void initGame() override   { /* setup: grid, entities, HUD, input, actions */ }
    void updateGame(float dt)  { /* rules: timers, movement, win/lose checks */ }
    void renderGame() override { /* draw: grid, entities, text */ }
    GameState getState() const override { /* what the LLM sees */ }
};
```

`main()` at the bottom is boilerplate — leave it alone (the
`__EMSCRIPTEN__` branch is for the web build).

### The three lifecycle methods

| Method | When | What it does |
|--------|------|--------------|
| `initGame()` | every game start (including restarts) | build the world |
| `updateGame(dt)` | every frame | change the world |
| `renderGame()` | every frame | draw the world |

**The one rule that matters:** `initGame()` must **clear everything first**.
The template starts with `entities.clear(); coins.clear(); enemies.clear();`
because restarting re-runs `initGame()` — if you don't clear, restarts leak
old entities. Copy this pattern every time.

### One move, two masters

Input and LLM actions both funnel through one method:

```cpp
ActionResult movePlayer(int dx, int dy);   // the single source of truth

bindKey(KEY_UP).onPress([this]() { movePlayer(0, -1); });   // human
registerAction("move_up", [this]() { return movePlayer(0, -1); }); // LLM
```

Human and LLM can never drift apart, because there's only one code path.
Do this in every game — it's the cheapest correctness win in the engine.

## 3. API cheat sheet (5 minutes)

Everything below is on `Game2D` (see `Engine/Core/Game2D.h`).

### World

```cpp
createGrid(w, h, tileSize);                       // make the board
grid->setValue(x, y, v);                          // color a cell
grid->setBorderColor({r, g, b, a});               // border color
setGridColors(v0, v1, color0, color1);            // cell palette
renderGrid();                                     // draw it in renderGame()
```

### Entities

```cpp
auto e = createEntity<GridEntity>(grid.get(), x, y);  // spawn on the grid
e->setColor({255, 0, 0, 255});
e->setPosition(x, y);  e->getX();  e->getY();
e->setActive(false);   e->isActive();               // "kill" without deleting
e->render(getRenderer());                            // draw it
```

Non-grid games can spawn plain entities too — look at how the
bunnymark example (`Examples/Bunnymark/`) uses raw position/speed for the
hotspot demo.

### Input

```cpp
bindKey(KEY_LEFT).onPress([this]() { ... });   // press (KEY_* constants)
bindKey(KEY_SPACE).onRelease([this]() { ... }); // release
// KEY_UP/DOWN/LEFT/RIGHT/WASD/SPACE/R/P/Q + everything SDL names
```

Mouse and touch exist too — see `Examples/TicTacToe/` for click handling.

### Text UI

```cpp
auto hud = createText(10, 10, "Score: 0");
hud->setText("Score: " + std::to_string(score));
hud->setColor({255, 255, 255, 255});
hud->render(getRenderer());           // in renderGame()
createButton(x, y, w, h, "label");    // clickable buttons
createStats(x, y);                    // persistent stat readout
```

Text works on desktop AND in the browser (SDL_ttf when present, built-in
bitmap fallback otherwise) — you never have to think about it.

### Game flow```cpp
endGame();            // game over (or won, with gameWon = true)
startGame();          // manual restart (R key usually calls this)
isGameRunning();  isGameOver();  gameWon;
getGameTime();        // seconds since start (useful for blinking)
rand()                // plain C rand is fine for games
```

### Game feel (GameJuice)

The AAA-feel bar (see GAMES.md) is met with `Engine/Core/GameJuice.h` — a
header-only, zero-asset toolkit every game includes. One header gives you:

```cpp
uj::ParticleSystem particles;  // bursts, gravity, fade
uj::ScreenShake shake;         // trauma-based shake (world layers only)
uj::HitStop hitStop;           // freeze the world on big hits
uj::FloatingText floatTexts;   // rising "+10" labels
uj::SfxSynth sfx;              // 17 procedural sounds, zero assets
```

Three arcade patterns come pre-built:

```cpp
// Ship respawn + invulnerable blink (Asteroids' "lost a life" beat).
uj::ShipRespawn respawn{1.2f, 2.0f};        // hidden 1.2s, blink 2.0s
respawn.start();                             // ship died, game continues
// updateGame:  respawn.update(dt);          // true once when respawn ends
// collision:   if (respawn.hittable()) shipHit();
// render:      if (respawn.visible()) drawShip(...);

// Split-on-hit entities (rocks that break into smaller rocks).
uj::SplitOnHit split;                        // deterministic per seed
if (split.splits(rock.size)) {               // tier 0 is atomic
    for (int i = 0; i < 2; ++i) {
        const auto c = split.child(rock.vx, rock.vy, 0.7f, 4.0f, 7.0f);
        spawnChild(rock.size - 1, rock.x, rock.y, c.vx, c.vy, c.seed, c.spin);
    }
}

// Projectile pool: spawn, wrap, and cull - no hand-rolled bullet array.
uj::ProjectilePool shots;
shots.setCap(4);                             // classic in-flight cap
if (shots.fire(x, y, vx, vy, 1.6f)) sfx.play(uj::Sfx::Shoot);
// updateGame:  shots.update(dt, GRID_W, GRID_H);   // move + wrap + cull
// hit test:    for (const auto& p : shots.all()) { if (hit) shots.kill(i); }
```

Both are timing/RNG-only: no SDL, no assets, deterministic — they run
identically natively, in WASM, and under the headless test harness.


## 4. The LLM interface (5 minutes)

The engine speaks to language models through a text state + named actions.

**State** — override `getState()` to describe the world:

```cpp
GameState getState() const override {
    GameState state = Game2D::getState();       // defaults (status, score...)
    state.stats["lives"] = lives;               // numbers
    state.stats["coins_remaining"] = coins.size();
    state.entities["player"] = {player->getX(), player->getY()};  // positions
    return state;
}
```

`GameState::toString()` renders it all as text (status, score, grid map,
entity positions, available actions) — that's the prompt the LLM sees.

**Actions** — `registerAction(name, callback)` in `initGame()`:

```cpp
registerAction("restart", [this]() {
    startGame();
    return ActionResult{true, "Restarted"};     // success + feedback
});
```

`ActionResult` fields: `success`, `message`, `scoreChange`. Return
`{false, "why not"}` for illegal moves — the model learns the rules from
your rejections.

Keep actions **semantic** ("move_up", "restart", "buy_sword") rather than
raw ("press_key_38"). Models do better with verbs.

## 5. House rules (3 minutes)

The whole tree builds with `-Werror` (GCC/Clang) and `/WX` (MSVC) — CI
will reject your game if it warns. The three you'll hit:

1. **Unused parameters** — `(void)param;` or `[[maybe_unused]]` (see
   `main()` in the template).
2. **Signed/unsigned compare** — compare `size_t` counts as `int`:
   `if (static_cast<int>(coins.size()) > 0)`.
3. **No VLAs** — `std::vector<T> name(n);` not `T name[n];`.

Run `cmake --build build-games --target <name>_game` and it either builds
or tells you exactly what to fix. That's the whole rulebook.

## 6. Headless dev + testing (3 minutes)

No display? SDL's dummy driver covers it:

```bash
SDL_VIDEODRIVER=dummy timeout 10 ./build-games/Pong_game
```

A clean exit (or being SIGTERM'd at the timeout) means your game starts,
runs its loop, and never crashes. This exact pattern is what CI runs under
valgrind, so what works headless here works everywhere.

Deeper memory checking, locally:

```bash
make memcheck                    # unit tests under valgrind
ctest --test-dir build-games -R memcheck   # games' memcheck passes
make callgrind                   # profile bunnymark (hotspots)
```

## 7. Web export (2 minutes)

```bash
./tools/build_web_game.sh Pong     # needs emcc (see BUILD_AND_TEST.md)
python3 -m http.server 8000 -d web # serve, then open localhost:8000/Pong.html
```

The script compiles `Games/Pong/main.cpp` + the engine core with the
Emscripten SDL ports, exactly like the full `make wasm`. Your game runs in
the browser. If your game loads an asset, add a `--preload-file` — see
`tools/build_web_game.sh` for the pattern (bunnymark's `fly.png`).

When a game is done, it ships with the rest of the catalog: `make wasm`
and `./build_wasm.sh` both build every non-template game in `Games/` to
`web/games/<name>/index.html` alongside the example demos (templates are
skipped automatically).

---

## Now go build

Pick #6 (Pong) from GAMES.md, scaffold it, and follow the five template
edits at the top of `main.cpp`:

1. Rename the class and window title
2. Change `updateGame()` — the rules
3. Change `renderGame()` — the look
4. Extend `registerAction()` / `getState()` — the LLM view
5. Tune the tunables

Mark your entry `✅` in GAMES.md when it ships. One hundred games, one
template, zero CMake edits. Go.
