# The 100-Game Program

The engine ships with a scaffold, a template, and a catalog. Your job: pick a
game, build it, ship it. Each entry lists the engine features it exercises, so
working down the catalog doubles as an engine feature tour.

## How to build a game

```bash
make new-game GAME=MyGame        # scaffolds Games/MyGame/ from the template
make game GAME=MyGame            # builds it (native)
./build-games/MyGame_game        # play it
```

That's it — Games/ auto-registers in CMake, so there is **zero build wiring**.
Headless dev (no display): `SDL_VIDEODRIVER=dummy timeout 10 ./build-games/MyGame_game`.

Web export: `./tools/build_web_game.sh MyGame` → `web/MyGame.js`, serve `web/`.

Difficulty: ★ = one sitting · ★★ = an afternoon · ★★★ = a weekend.

---

## Flagship — Cinderfall 🚧 (parked; backburner)

The engine's long-term showcase title (top-down action-adventure roguelite).
**On hold while the basics get shipped** — the program builds bottom-up so the
engine and the catalog mature together; Cinderfall resumes once Tier 1 is done.
M0-M2 slices are playable if you want a peek: `make game GAME=Cinderfall`.

---

## How the tiers work

**Basics first, quality over speed.** Every game ships before the next tier
starts. A game is only marked done when it meets the AAA-feel bar below —
mechanics alone don't count. Tier 0 is done (mechanics), Tier 1 is in
progress; feel retrofits roll through the shipped games in order.

## The AAA-feel bar

A game is "done" only when it is fully playable *and* feels like a real game:

- **Full loop** — start → play → win/lose → restart, plus pause (P) and a
  session best score.
- **Juice** — every impactful event pays out: particle bursts, trauma-based
  screen shake, hit-stop, floating score text.
- **Audio** — procedural sound effects synthesized in memory (zero asset
  files; identical native / WASM / headless).
- **Presentation** — distinct pixel-art sprites, a live HUD, and clean
  overlays (pause, game over, win).
- **Playability** — human keyboard controls, an LLM action surface that
  mirrors them exactly, headless smoke autoplay, and web export.
- **Depth** — scoring with a difficulty ramp (speed/level curves), not just
  a binary win/lose.

The shared toolkit `Engine/Core/GameJuice.h` makes this cheap — particles,
screen shake, hit-stop, floating text, and the SFX synth are one include.

**Feel status:** Snake (#1), TicTacToe (#2), Minesweeper (#3), Roguelike
(#4), Breakout (#7), Space Invaders (#8), Tetris (#9), Flappy Bird (#10),
Doodle Jump (#11), Asteroids (#12), Pac-Man (#13), Memory Match (#14),
Simon Says (#15), Whack-a-Mole (#16), Portal Snake (#17), Frogger
(#18), Brick Breaker+ (#19), Galaga (#20), and 2048 (#22) are ✅, plus Cinderfall (flagship, M1). Every new game ships with
the kit from day one — mechanics and feel land together.

---

## Tier 0 — already shipped ✅

| # | Game | Pitch | Features exercised |
|---|------|-------|-------------------|
| 1 | **Snake** ✅ | The classic | grid, input, game-over, GameJuice feel (food burst/death explosion) |
| 2 | **TicTacToe** ✅ | Two-player duel | UI, buttons, win detection, GameJuice feel (placement pops/win-line flash/shake/fanfare) |
| 3 | **Minesweeper** ✅ | Flood-fill logic | grid, mouse, recursion, GameJuice feel (reveal pops/mine blast) |
| 4 | **Roguelike** ✅ | Turn-based dungeon crawl | tilemap, entities, turn loop, GameJuice feel (hit flash/kill burst/stairs popup) |
| 5 | **CoinCollector** ✅ (template) | Chase coins, avoid hazards | template anatomy, restart, text UI |
| 6 | **Pong** ✅ | Two-player rally | float physics, held keys, scoring, LLM actions |

## Tier 1 — next up (★, ~30 min each)

| # | Game | Pitch | Features exercised |
|---|------|-------|-------------------|
| 7 | **Breakout** ✅ | Bounce through a brick wall | physics, collision, full GameJuice feel (particles/shake/hit-stop/SFX) |
| 8 | **Space Invaders** ✅ | Row of invaders marches down | entities, movement patterns, shooting, shields |
| 9 | **Tetris** ✅ | Rotate falling blocks | grid, rotation, wall kicks, line-clear, hold queue, GameJuice feel (clear flash/shake/SFX) |
| 10 | **Flappy Bird** ✅ | One button, one gap | gravity, procedural pipes, state machine, GameJuice feel (hop puff/score pop/death burst) |
| 11 | **Doodle Jump** ✅ | Bounce ever upward | scrolling, bounce physics, GameJuice feel (squash/stretch, wall dust, new-best celebration) |
| 12 | **Asteroids** ✅ | Drift and shoot rocks | thrust/drift, wrap-around, split rocks, saucer, GameJuice feel |
| 13 | **Pac-Man** ✅ | Eat dots, dodge ghosts | maze grid, scatter/chase/frightened ghost AI, GameJuice feel |
| 14 | **Memory Match** ✅ | Flip tiles, find pairs | grid, mouse, timing, GameJuice feel (flip pop/match burst/streak) |
| 15 | **Simon Says** ✅ | Repeat the growing sequence | audio, timing, state machine, GameJuice feel (per-tile chime/glow, wrong-press shake/hit-stop/SFX) |
| 16 | **Whack-a-Mole** ✅ | Hit the mole before it hides | timers, deterministic spawns, combo, GameJuice feel (whack bursts/shake/hit-stop, escape misses, game-over explosion) |
| 17 | **Portal Snake** ✅ | Snake with wrap-around edges | edge wrap portals, torus routing, GameJuice feel (portal slide/food burst/death explosion, portal bonus) |
| 18 | **Frogger** ✅ | Dodge traffic, cross the lanes | lane colliders, deterministic traffic, log riding, BFS autopilot, GameJuice feel (hop pops/splat/splash/goal confetti) |
| 19 | Brick Breaker+ ✅ | Breakout with power-ups | power-up system, multi-ball, GameJuice feel (catch bursts/shake/SFX) |
| 20 | **Galaga** ✅ | Fixed-position shooter with a marching formation and diving bugs | waves, formation movement, GameJuice feel (dive/kill bursts/shake/SFX) |
| 21 | Split-Screen Pong | Two players, one keyboard | dual input, same-keyboard |
| 22 | **2048** ✅ | Slide tiles, merge powers of two | grid logic, swipe/key input, GameJuice feel (merge pops/shake/pitched SFX) |
| 23 | Color Switch | Pass through same-colored rings | color states, timing |
| 24 | Stacker | Stack a shrinking row | grid, timing precision |
| 25 | Memory Simon | Simon with visual tiles | audio + visuals combined |

## Tier 2 — weekend builds (★★, 2–4 hrs)

| # | Game | Pitch | Features exercised |
|---|------|-------|-------------------|
| 26 | Platformer | Run and jump through a level | physics, tilemap, gravity |
| 27 | Tower Defense | Build turrets, stop the wave | pathfinding, waves, economy |
| 28 | Endless Runner | Jump over an infinite gauntlet | procedural generation |
| 29 | Twin-Stick Shooter | Move one stick, aim the other | analog input, bullets |
| 30 | Mini Zelda | Top-down rooms and keys | rooms, doors, inventory |
| 31 | Sudoku | Classic number puzzle | grid, validation, UI |
| 32 | Match-3 | Swap gems to chain combos | grid, swap logic, combos |
| 33 | Blackjack | Beat the dealer | cards, probability, UI |
| 34 | Solitaire | Classic patience | drag, card stacks |
| 35 | Unbeatable TicTacToe | AI you can't beat | minimax AI |
| 36 | Checkers | Jump and capture | board logic, AI |
| 37 | Chess Lite | Simplified chess | piece movement, AI |
| 38 | Reversi | Flip the board in your favor | board logic, AI |
| 39 | Connect Four | Drop and align four | gravity grid, AI |
| 40 | Bomberman | Lay bombs, blast walls | grid, explosions, power-ups |
| 41 | Breakout Editor | Design your own levels | editor UI, save/load |
| 42 | Top-Down Racer | Slide around a track | car physics, drifting |
| 43 | Slingshot | Fling projectiles at castles | physics, trajectories |
| 44 | Fruit Ninja | Slice the fruit, dodge bombs | mouse/touch, particles |
| 45 | Rhythm Runner | Time jumps to the beat | audio sync, timing |
| 46 | Type Blaster | Type words to shoot ships | keyboard text input |
| 47 | Hangman | Guess the word | text, word lists |
| 48 | Wordle | Six guesses, five letters | text, letter states |
| 49 | Trivia Quiz | Answer questions against a clock | text, timers, scoring |
| 50 | Bridge Builder | Build a bridge that holds | physics joints, stress |
| 51 | Bubble Shooter | Match colored bubbles | grid math, aiming |
| 52 | Peggle | Bounce the ball off pegs | physics, angles |
| 53 | Puyo Puyo | Match falling pairs | falling blocks, chains |
| 54 | Dr. Mario | Match pills to viruses | falling blocks, colors |
| 55 | Digger | Tunnel, collect, escape | digging grid, enemies |
| 56 | Snake vs Block | Snake grows, blocks shrink | merging mechanics |
| 57 | Bingo | Fill rows and columns | RNG, board logic |
| 58 | Concentration+ | Memory match with power tiles | timed modes, scoring |
| 59 | Hot Potato | Pass the bomb in time | timers, same-keyboard |
| 60 | Speed Pong | The ball accelerates | difficulty curves |

## Tier 3 — ambitious (★★★, 1–2 weekends)

| # | Game | Pitch | Features exercised |
|---|------|-------|-------------------|
| 61 | Mini Metroidvania | One castle, unlock abilities | rooms, upgrades, save |
| 62 | Rogue-Lite | Permadeath with persistent upgrades | procedural, meta-progression |
| 63 | Survivors | Waves of enemies, auto-fire | hordes, upgrade choices |
| 64 | Bullet Hell | Survive walls of projectiles | dense collision, patterns |
| 65 | Dungeon Crawler | Descend, loot, survive | procedural floors, items |
| 66 | Micro City Builder | Place roads and zones | map data, economy |
| 67 | Idle Forge | Numbers go up while away | incremental systems |
| 68 | Tower Climb | Ascend an infinite tower | procedural, difficulty |
| 69 | Penalty Shootout | Two-player goal duel | aiming, timing |
| 70 | 2D Golf | Physics putting | physics, terrain |
| 71 | Pool Lite | Pocket the balls | physics, friction, spin |
| 72 | Bowling | Curved throws at pins | physics, pin setup |
| 73 | Pinball | Flippers and bumpers | physics, bumpers |
| 74 | Platformer + Editor | Build and share levels | level serialization |
| 75 | Dialogue Adventure | Talk your way through | dialogue system, quests |
| 76 | Visual Novel | Choices change the story | text, branching |
| 77 | Beat Mapper | Build your own rhythm maps | audio, editor |
| 78 | Co-op Maze Escape | Two players, one screen | split-screen coordination |
| 79 | Capture the Flag | Command AI bots | bot AI, team logic |
| 80 | Zombie Horde | Survive the night | waves, upgrades, day cycle |
| 81 | Tower Defense+ | Free-form maze building | pathfinding on demand |
| 82 | FTL-Lite | Manage resources across jumps | resource management, events |
| 83 | Deck Builder | Draft cards, build combos | cards, synergies |
| 84 | Auto-Battler | Position units, watch them fight | unit AI, balance |
| 85 | Fishing Game | Timed catch minigame | reaction timing, RNG |
| 86 | Cooking Rush | Fulfill orders under pressure | timers, queue management |
| 87 | Micro Farm | Plant, grow, sell | day cycle, economy |
| 88 | Shopkeeper | Stock shelves, set prices | economy, customers |
| 89 | Tycoon | Build an empire from pennies | economy, upgrades |
| 90 | Physics Sandbox | Build contraptions, press go | physics, joints, spawn |
| 91 | Grapple Platformer | Swing with a hook | rope physics |
| 92 | Level-You Platformer | You ARE the level | inverse mechanics |
| 93 | Boss Rush | Chain bosses with no healing | boss design, patterns |
| 94 | Speedrun | One room, many strats | timer, replays |
| 95 | Same-Keyboard Duel | Two-player arena brawl | dual input, combos |
| 96 | Learn-It AI | Watch AI learn your game | Q-learning, viz |
| 97 | Text Adventure | LLM-driven world | LLM actions, narrative |
| 98 | LLM NPCs | Talk to characters that answer | LLM dialogue, persistence |
| 99 | Mashup | Fuse two Tier-2 games | design freedom |
| 100 | Your Signature Game | The one you tell people about | everything you've learned |

---

## Rules of the program

1. **One game per `make new-game GAME=<name>`.** Each lives in `Games/<name>/`,
   fully self-contained, warning-free (the tree builds with `-Werror`/`/WX`).
2. **Ship it web-ready.** Every game should build with `tools/build_web_game.sh`
   — a game you can link to is a game you've finished.
3. **Playtest headless.** `SDL_VIDEODRIVER=dummy timeout 10 ./build-games/<name>_game`
   must survive without a display — it's also what CI runs under valgrind.
4. **Copy, don't reinvent.** The template shows the correct restart pattern
   (clear entities in `initGame`), text UI, and LLM actions. Steal it.
5. **Check it off.** Mark `✅` in the table above when you ship. The goal is a
   catalog of 100 shipped games, not 100 started ones.

See GAME_DEV_GUIDE.md for the 30-minute walkthrough of the template.
