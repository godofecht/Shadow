# Demo Gallery

Shadow makes more sense when you can see the breadth of the engine at a glance. This page is a curated map of real examples and games in the repository: rendering, effects, simulation, physics, AI, audio, performance, and complete game loops.

<div class="shadow-gallery-feature" markdown>

<div class="shadow-gallery-mark" aria-hidden="true">100</div>

<div markdown>

## The 100-game program

The engine is being exercised through a growing catalog of complete games, from arcade fundamentals to action, puzzle, platformer, shooter, and roguelike systems. The point is not just quantity: each game pushes the same `Game2D`, rendering, physics, audio, UI, and agent-facing surfaces from a different direction.

[Browse the game catalog](GAMES.md){ .md-button .md-button--primary }
[Browse `Games/` on GitHub](https://github.com/godofecht/Shadow/tree/main/Games){ .md-button }

</div>

</div>

## Rendering & effects

<div class="shadow-demo-grid" markdown>

<div class="shadow-demo-card" markdown>
<div class="shadow-demo-poster demo-bloom"><span>✦</span></div>
<div class="shadow-demo-meta"><span>POST FX</span><span>RENDERING</span></div>

### Bloom Showcase

Glow and bloom rendering as a focused post-processing showcase.

[View source](https://github.com/godofecht/Shadow/tree/main/Examples/BloomShowcase){ .md-button }

</div>

<div class="shadow-demo-card" markdown>
<div class="shadow-demo-poster demo-particles"><span>· ✦ ·</span></div>
<div class="shadow-demo-meta"><span>PARTICLES</span><span>FX</span></div>

### Particle Showcase

A dedicated stress-and-style example for Shadow's particle systems.

[View source](https://github.com/godofecht/Shadow/tree/main/Examples/ParticleShowcase){ .md-button }

</div>

<div class="shadow-demo-card" markdown>
<div class="shadow-demo-poster demo-lighting"><span>◉</span></div>
<div class="shadow-demo-meta"><span>LIGHTING</span><span>2D</span></div>

### Lighting

Dynamic 2D lighting and the renderer paths that support it.

[View source](https://github.com/godofecht/Shadow/tree/main/Examples/Lighting){ .md-button }

</div>

<div class="shadow-demo-card" markdown>
<div class="shadow-demo-poster demo-mesh"><span>△</span></div>
<div class="shadow-demo-meta"><span>MESH</span><span>2D</span></div>

### Mesh2D

Manual and engine-managed 2D meshes, vertex colors, alpha, and repeated textures.

[View source](https://github.com/godofecht/Shadow/tree/main/Examples/Mesh2D){ .md-button }

</div>

</div>

## Simulation & engine systems

<div class="shadow-demo-grid" markdown>

<div class="shadow-demo-card" markdown>
<div class="shadow-demo-poster demo-physics"><span>↯</span></div>
<div class="shadow-demo-meta"><span>BOX2D</span><span>REACTIVE</span></div>

### Physics Reactive

Box2D state driving visible game behavior rather than existing as an isolated subsystem.

[View source](https://github.com/godofecht/Shadow/tree/main/Examples/PhysicsReactive){ .md-button }

</div>

<div class="shadow-demo-card" markdown>
<div class="shadow-demo-poster demo-procedural"><span>⌁</span></div>
<div class="shadow-demo-meta"><span>PROCEDURAL</span><span>GEOMETRY</span></div>

### Procedural 3D

Procedural geometry generation through Shadow's experimental 3D-facing paths.

[View source](https://github.com/godofecht/Shadow/tree/main/Examples/Procedural3D){ .md-button }

</div>

<div class="shadow-demo-card" markdown>
<div class="shadow-demo-poster demo-slime"><span>≈</span></div>
<div class="shadow-demo-meta"><span>EMERGENT</span><span>SIMULATION</span></div>

### Slime Mold

An emergent simulation that exercises dense updates and evolving spatial state.

[View source](https://github.com/godofecht/Shadow/tree/main/Examples/SlimeMold){ .md-button }

</div>

<div class="shadow-demo-card" markdown>
<div class="shadow-demo-poster demo-compute"><span>∑</span></div>
<div class="shadow-demo-meta"><span>COMPUTE</span><span>SIMULATION</span></div>

### Compute Sim

A compute-oriented simulation example for high-volume state evolution.

[View source](https://github.com/godofecht/Shadow/tree/main/Examples/ComputeSim){ .md-button }

</div>

<div class="shadow-demo-card" markdown>
<div class="shadow-demo-poster demo-navigation"><span>⌖</span></div>
<div class="shadow-demo-meta"><span>AI</span><span>NAVIGATION</span></div>

### Navigation

Movement and navigation systems for agents operating through game space.

[View source](https://github.com/godofecht/Shadow/tree/main/Examples/Navigation){ .md-button }

</div>

<div class="shadow-demo-card" markdown>
<div class="shadow-demo-poster demo-audio"><span>◖)))</span></div>
<div class="shadow-demo-meta"><span>AUDIO</span><span>SPATIAL</span></div>

### Spatial Audio

Position-aware audio built on the same engine loop as rendering and gameplay.

[View source](https://github.com/godofecht/Shadow/tree/main/Examples/SpatialAudio){ .md-button }

</div>

<div class="shadow-demo-card" markdown>
<div class="shadow-demo-poster demo-bunny"><span>×10k</span></div>
<div class="shadow-demo-meta"><span>PERF</span><span>STRESS</span></div>

### Bunnymark

A familiar rendering stress test for quickly seeing how the engine behaves under load.

[View source](https://github.com/godofecht/Shadow/tree/main/Examples/Bunnymark){ .md-button }

</div>

<div class="shadow-demo-card" markdown>
<div class="shadow-demo-poster demo-ai"><span>◇→◆</span></div>
<div class="shadow-demo-meta"><span>AI</span><span>STATES</span></div>

### AI States

State-driven agent behavior and transitions in a compact example.

[View source](https://github.com/godofecht/Shadow/tree/main/Examples/AIStates){ .md-button }

</div>

</div>

## Complete game loops

<div class="shadow-demo-grid" markdown>

<div class="shadow-demo-card" markdown>
<div class="shadow-demo-poster demo-rogue"><span>#@</span></div>
<div class="shadow-demo-meta"><span>GAME</span><span>ROGUELIKE</span></div>

### Roguelike

Turn-based movement, procedural dungeon structure, combat, pickups, and progression in one compact game.

[View source](https://github.com/godofecht/Shadow/tree/main/Examples/Roguelike){ .md-button }

</div>

<div class="shadow-demo-card" markdown>
<div class="shadow-demo-poster demo-snake"><span>●━━</span></div>
<div class="shadow-demo-meta"><span>GAME2D</span><span>GRID</span></div>

### Snake

The smallest useful demonstration of the grid framework, input, game state, score, and restart loop.

[View source](https://github.com/godofecht/Shadow/tree/main/Examples/Snake){ .md-button }

</div>

<div class="shadow-demo-card" markdown>
<div class="shadow-demo-poster demo-life"><span>▦</span></div>
<div class="shadow-demo-meta"><span>CELLS</span><span>SIMULATION</span></div>

### Game of Life

A compact cellular-automata example that makes grid updates and rendering immediately visible.

[View source](https://github.com/godofecht/Shadow/tree/main/Examples/GameOfLife){ .md-button }

</div>

<div class="shadow-demo-card" markdown>
<div class="shadow-demo-poster demo-mines"><span>✹</span></div>
<div class="shadow-demo-meta"><span>GAME</span><span>LOGIC</span></div>

### Minesweeper

Mouse interaction, flood-fill reveal, flags, state transitions, and UI in a complete puzzle loop.

[View source](https://github.com/godofecht/Shadow/tree/main/Examples/Minesweeper){ .md-button }

</div>

</div>

## Everything else

The gallery is deliberately curated. Shadow has many more focused examples covering sprites, text, parenting, tweening, fixed timestep, tile maps, viewport transforms, skeletal animation, terrain, drawing backends, and other engine surfaces.

[Open the complete examples index](Examples/README.md){ .md-button .md-button--primary }
[Browse all examples on GitHub](https://github.com/godofecht/Shadow/tree/main/Examples){ .md-button }
