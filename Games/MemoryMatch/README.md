# Memory Match (game #14)

The flip-and-find classic: a grid of face-down cards hides matching pairs.
Flip two cards a turn — a match locks them face-up and pays out with a combo
multiplier; a miss flips them back and costs a mistake. Clear all three levels
(4×4 → 6×4 → 6×6) to win, or burn your mistake budget and lose.

## Build & run

```bash
make game GAME=MemoryMatch       # or: cmake --build build-games --target MemoryMatch_game
./build-games/MemoryMatch_game
```

Web export: `./tools/build_web_game.sh MemoryMatch` → `web/MemoryMatch.js`.

Headless smoke (CI uses this): `PONG_SMOKE=1 SDL_VIDEODRIVER=dummy timeout 10 ./build-games/MemoryMatch_game`.

## Controls

- **Mouse** — click a card to flip it.
- **Keyboard** — arrows move the cursor, Enter/Space flips.
- **P** pause · **R** restart.

## Feel (GameJuice — from day one)

Ships to the AAA-feel bar via `Engine/Core/GameJuice.h`, all synthesized in
memory (zero asset files — see `Engine/Core/GameJuice.h`):

- **Flip** — every reveal pops sparks with a soft ping.
- **Match** — each card bursts its own symbol color with screen shake, a
  hit-stop beat, a rising `+N`, and the two-note coin payoff. Consecutive
  matches stack a streak (`STREAK xN`) that multiplies the payout.
- **Miss** — a thock, a small shake, and the streak resets.
- **Level clear / win** — confetti fanfares, the win arpeggio, and a
  `NEW BEST`-style celebration on a session record.

## Depth

- **Combo streaks** — consecutive matches pay 10, 20, 30…, resetting on a miss.
- **Difficulty ramp** — 8 → 12 → 18 pairs across three levels, with a
  level-clear bonus.
- **Mistake budget** — one miss flips the cards back and costs a mistake;
  run out and it's game over.

## LLM surface

Actions: `move_up`, `move_down`, `move_left`, `move_right`, `flip`, `restart`.
`getState()` exposes `score`, `best`, `level`, `pairs_matched`, `pairs_total`,
`misses`, `misses_max`, `combo`, the cursor, and the juice stats. The board is
reported as `0` hidden / `1` face-up / `2` matched — **card values stay
hidden**, so an agent must remember like a human.

## Autopilot

The smoke autopilot plays with perfect memory: it remembers every card it has
seen face-up and greedily matches known pairs, so it clears boards headlessly
without ever reading the hidden values — which is exactly what the headless
CI smoke and the unit tests drive.
