# Aquarium Gameplay Gimmick Plan V1

Target scope: a 20-30 minute first playable with one memorable chase.

## Reference patterns

This plan borrows structural lessons, not individual puzzles or scenes.

- [The Aquarium does not dance](https://store-jp.nintendo.com/item/software/D70010000098456)
  combines aquarium exploration, hidden clues, and discrete escapes from
  creatures. The useful lesson is to alternate readable puzzle time with short
  danger spikes instead of keeping an enemy active everywhere.
- [UTOPIA](http://createlab.web.fc2.com/utopia/story.html) connects an aquatic
  space to a sleeping person's memory. The useful lesson is that recurring
  environmental contradictions can carry the mystery without long exposition.
- [Ib](https://store.steampowered.com/app/1901370/Ib/) turns exhibits into
  rules, clues, and threats. The useful lesson is to let each gallery's display
  language become its gameplay language.
- [Ao Oni](https://store.steampowered.com/app/2934190/Aooni/) separates
  exploration and puzzle solving with sudden pursuit. The useful lesson is that
  a simple pursuer is effective when the route and interruption timing are
  authored well.

## Core rhythm

```text
Safe observation
  -> understand the room's visual rule
  -> apply the rule as a puzzle
  -> the same rule becomes threatening
  -> short release and story reward
```

Do not add a free-roaming enemy to every room. One complete chase plus several
non-fail threat sequences will feel more deliberate and cost much less to
implement.

## Room gimmicks

| Room | Gimmick | Threat conversion | Reusable systems |
|---|---|---|---|
| 1 Entrance | Exit and floor-map inspection | The printed route differs after the player looks away | Interactable, decal swap, objective flag |
| 2 Coastal | Set three exhibit tide lamps from animal-depth clues | A wrong answer raises the projected waterline and obscures clues | Light preset, material parameter, authored answer |
| 3 Main Tank | Align View A with two marked glass positions | The tank silhouette changes only between viewpoints | Trigger volume, camera marker, tank-state swap |
| 4 Light Gallery | Reconstruct a three-color memory sequence | Wrong colors create a convincing false exit before resetting | Emissive channels, sequence controller, bloom |
| 5 Water Arch | Three-phase authored chase | Glass impacts, extinguishing lights, and closing shutters compress the route | Spline pursuer, chase director, checkpoint |
| 6 Deep Sea | Pulse four portholes and align one consistent view | A silhouette advances only between light pulses | Pulse light, porthole material variants, puzzle state |
| 7 Final Dome | Stabilize the return image under a time limit | The dome floods visually while safe time windows shrink | Timer, material fill, final sequence |

## Hero gimmick: the same tank tells three different truths

The shared hero tank is viewed from Rooms 3, 5, and 6. Use one tank simulation
but assign a different narrative rule to each view.

1. **View A / Main Hall:** the player learns that the tank contains an
   impossible remembered rock formation.
2. **View B / Water Arch:** the player briefly sees the populated original
   aquarium through the same water.
3. **View C / Deep Sea:** the four portholes disagree until the player aligns
   them.

This makes the existing layout itself the main puzzle and avoids building three
unrelated spectacle rooms.

## Water Arch chase

The Water Arch should contain the only full fail-state chase.

### Phase 0 - teach the route

- Let the player walk the first third safely.
- Show two wall recesses and three numbered emergency shutters.
- A slow silhouette passes overhead, but nothing attacks.
- Keep the exit visible as a stable blue-violet landmark.

The player must see the route before panic begins.

### Phase 1 - pursuit starts

- View B briefly shows visitors in the original aquarium.
- A large silhouette blocks that image.
- The first impact cracks the glass behind the player.
- The entry shutter closes and a spline-driven threat begins moving.
- Ceiling lights turn off from back to front, giving a readable direction.

The threat does not need navigation AI. Its distance along the tunnel spline is
enough.

### Phase 2 - two authored decisions

1. A damaged shutter blocks the center. The emergency lamp indicates the open
   left or right recess before the obstacle becomes visible.
2. A water jet crosses the route in pulses. The player waits half a second in a
   recess, then crosses during the dark interval.

Use one deterministic pattern for the first playable. Randomization makes
testing and camera direction harder without adding much value.

### Phase 3 - finish

- The final lights switch from cyan to violet.
- A second impact passes overhead rather than directly behind the player.
- The exit shutter begins to close but never creates a frame-perfect input.
- Reaching the Deep Sea door ends the pursuit and seals the tunnel.
- A porthole beside the checkpoint shows the threat continuing past the door.

### Fairness rules

- Target chase length: 45-70 seconds.
- Give 1.5-2.0 seconds of reaction time for each route change.
- Use one distance-based fail condition, not collision with a complex creature.
- Reset at the chase entrance with a two-second restart.
- Never hide the correct route using refraction, bloom, or volumetric light.
- Camera shake should affect rotation slightly, not player steering.

## Deep Sea pulse puzzle

The player activates a short blue light pulse. During each pulse:

- all four View C portholes become readable for about one second;
- three show incorrect tank states;
- one detail remains consistent with evidence found in Rooms 2-4;
- the overhead silhouette advances one porthole while the room is dark.

Selecting the four correct states builds a single stable image of the original
aquarium. This is tension without a second chase or new enemy AI.

## Final Dome pressure sequence

Do not repeat the Water Arch chase in Room 7.

- The return image appears on the dome.
- Three stabilizers must be held in the order learned from the hero-tank views.
- Each successful input slows the projected flood and restores blue light.
- A wrong input removes time but does not instantly kill the player.
- On completion, the reflected entrance replaces the dome image and the player
  returns.

The threat remains visible outside the dome, so the climax has pressure without
requiring another pursuit route.

## Production priority

### Must have

1. Room 4 light-memory puzzle.
2. Room 3 two-position viewpoint contradiction.
3. Room 5 three-phase chase.
4. Room 6 four-state pulse puzzle.
5. Room 7 timed stabilizer finale.

### Cut first if time slips

1. Branching chase recess selection; keep one fixed path.
2. Moving projected waterline in Room 2; retain only lamp colors.
3. Animated flood in Room 7; use exposure and material tint instead.
4. The silhouette after the Deep Sea checkpoint.

## Implementation boundary

Keep authored content data-driven:

```text
PuzzleDefinition
  id
  roomId
  requiredFlags
  orderedInputs
  successEvents
  failureEvents

ChaseDefinition
  routeSpline
  playerStart
  threatStartDistance
  threatSpeedCurve
  obstacleEvents
  checkpointId
  failDistance
```

Rendering should only react to events such as lighting preset changes, material
parameter changes, and sequence cues. Puzzle and chase logic must not be placed
inside `AquariumRenderer`.

