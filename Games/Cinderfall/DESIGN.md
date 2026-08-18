# Cinderfall — Design Document

> **Genre:** top-down action-adventure roguelite (Zelda-style combat + procedural
> floors + permanent progression). **Target:** a single, deep game built up over
> months — the flagship of the 100-game program.
>
> **Status:** Milestones 0-2 shipped (vertical slice + game feel + first new
> bestiary archetypes). See `README.md` for controls and `CinderfallState.h`
> for the simulation.

---

## 1. The hook

You are the last Ember-Knight. The subterranean kingdom of **Cinderfall** went
dark when its soul-forges died; you descend, floor by floor, to rekindle the
final forge at the bottom. Every enemy you slay feeds your blade; every run
leaves something behind for the next.

One sentence: **"Zelda-feel combat + Binding-of-Isaac-style runs + Hades-style
meta-progression, playable by humans and LLM agents through the exact same
controls."**

## 2. Pillars (what we will never compromise on)

1. **Snappy melee** — a sword swing with a real arc, knockback and hit-flash;
   a dodge roll with i-frames. Combat should *feel* good before it *looks* good.
2. **Emergent runs** — floors are procedurally generated (rooms + corridors +
   locked doors + chests), so no two descents are the same.
3. **Permanent progression** — dying still buys you something (meta-upgrades
   between runs), so every run moves the long-term arc forward.
4. **LLM-native** — the human keybindings and the LLM action set are the *same
   actions*; both drive `State::step()`. The whole run is serializable so an
   agent can observe, act, and save/restore.

## 3. The core loop (already playable in M0)

```
Title -> descend floor -> explore -> find the key -> open the door
      -> reach the stairs -> descend ... -> rekindle the final forge -> WIN
      (or die and keep your gold for meta-upgrades)
```

Per floor: a locked door guards the stairs; the key sits in a chest somewhere
earlier; gold and a rare heart are scattered; enemies patrol and hunt you.

## 4. Content roadmap (months, not days)

Each milestone is shippable and independently testable.

| Milestone | Theme | Contents |
|-----------|-------|----------|
| **M0** ✅ | The slice | 3 floor types, sword + roll, 3 enemies (Chaser/Spitter/Brute), doors/keys/chests/gold/hearts, title/death/win, LLM actions, smoke autopilot, unit tests. |
| **M1** ✅ | Game feel | Screen shake (trauma-based), hit-stop, particles (ember sparks, blood, gold), procedural sound effects (swing/hit/kill/pickup/hurt/door/descend/win) - no asset files. |
| **M2** 🔶 | Bestiary | 3 original enemies + **ghost (phases through walls)** + **turret (3-shot volleys)** done; next: shieldbearer, exploder, mimic. |
| **M3** | Bosses | One multi-phase boss per biome (e.g. the Forge Warden: melee -> ember volley -> enrage). Boss arenas + telegraphs. |
| **M4** | Builds & items | Relics that modify the sword (pierce, lifesteal, chain-lightning), trinkets (magnet, thornmail, glass cannon). Run-defining combinations. |
| **M5** | Biomes & floors | 3 biomes (Ashen Crypt, Molten Foundry, Hollow Sanctum) with distinct palettes, hazards (lava, spikes, crumbling floors) and secrets (bombable walls, hidden rooms). |
| **M6** | Meta-progression | Persistent unlocks between runs: max HP, base damage, roll distance, starting key, shop between floors. |
| **M7** | Save & narrative | Floor-boundary save/load (`serialize()` already exists), item/beastiary codex, NPC fragments telling the Cinderfall's fall. |
| **M8** | Ship & polish | Gamepad + rebindable keys, WASM export tuning, difficulty modes, achievements, balance pass, trailer. |

## 5. Tech plan (how it maps to the engine)

- **`CinderfallState.h`** — pure, SDL-free simulation (floor gen, combat,
  enemies, projectiles, doors/chests/pickups, stairs, save/load). The single
  source of truth; unit-tested in `tests/test_cinderfall.cpp`.
- **`main.cpp`** — a `Game2D` shell: input → `Input` → `state.step()`, and
  `state` → pixels. Rendering is procedural pixel-art (colored rects + border +
  eye pixels) so the game needs **zero binary assets** and runs identically in
  native, WASM, and headless CI.
- **Determinism** — an LCG seeded from `runSeed ^ floorNum` regenerates each
  floor, so a seed *is* a save. `serialize()`/`load()` persist the run meta and
  rebuild the floor exactly.
- **LLM interface** — `move_up/down/left/right`, `attack`, `roll`, `interact`,
  `start`, `restart`. `getState()` exposes hp/gold/keys/floor/positions; the
  actions are the same code paths as the keyboard.
- **CI** — builds under `-Werror`, runs the unit tests, and `PONG_SMOKE=1`
  autopilots the real-time loop headlessly.
- **M1 feel is simulation-side** — hit-stop freezes the whole world for a beat
  on impactful hits, trauma drives a screen shake that eases off, and the
  particle system + SFX event queue live in the pure state (unit-testable,
  headless). The shell synthesizes tiny WAVs in memory (SDL_mixer
  `Mix_LoadWAV_RW`), so there are still **zero binary assets**.

## 6. Art direction (procedural pixel art)

A warm-on-dark palette (ember orange vs. ash purple). Everything is drawn from
a 20px tile grid with 1px borders and 2px "eyes", so sprites stay crisp and
consistent with no external art pipeline. A real sprite sheet (16x16 or 32x32)
can drop in later without touching the simulation.

## 7. Testing strategy

- **Unit tests** (`tests/test_cinderfall.cpp`): floor validity + determinism,
  wall collision, attack arc (hit/miss), door/key gating, chest contents,
  pickups, stairs descend/win, contact damage, serialize round-trip, hit-stop
  freeze, particle spawn/cull, trauma decay, SFX event emission, ghost wall
  phasing + contact damage, turret volley timing + stationarity.
- **Sanitizers** (CI): ASan + UBSan over the native tests; the WASM path is
  covered in the Emscripten jobs.
- **Smoke** (CI): the autopilot runs the real-time loop for a full window
  headlessly.
- **Rule:** every new enemy/item/mechanic ships with a unit test first.

## 8. "Done" for the long term

3 biomes × 6+ enemies × 1 boss each, 10+ relics, meta-progression, save/load,
gamepad, a codex, and a WASM build that plays the same in the browser — all
covered by the test suite and CI.
