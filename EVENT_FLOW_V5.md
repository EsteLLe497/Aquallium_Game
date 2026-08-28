# Aquarium Event Flow V5 - The Aquarium That Kept Growing

Target scope: a 25-35 minute first playable.

## Premise

The player falls asleep on a bench in the Jellyfish Theater. They wake after
closing and check the nearby entrance, but the exit is locked.

The aquarium appears normal at first. As the player follows the official route,
the building becomes deeper, longer, and less plausible. The threat is tied to
the aquarium itself, not to a visible parallel world.

The player never sees the original world through water, glass, reflections, or
portals. The only goal is to reach a physical emergency exit at the end of the
route.

## Flow overview

```text
2 Jellyfish Theater - wake up
  -> 1 Entrance - exit is locked
  -> 2 Jellyfish Theater - one display has changed
  -> 3 Coastal Gradient - first puzzle and access card
  -> 4 Sunlit Reef - scenic release
  -> 5 Grand Ocean Hall - impossible scale and main silhouette
  -> 6 Luminous Gallery - light-memory puzzle opens the arch
  -> 7 Underwater Arch - only full chase
  -> 8 Deep Sea Descent - pulse-guided walk and partial explanation
  -> 9 Deep Sea Panorama - scripted break event and emergency exit
  -> white morning light, then cut
```

Only the short Stage 2-to-1 entrance check is repeated. The rest of the game is
one-way.

## Stage 2 to Stage 1 - wake and check the exit

Duration: 3 minutes

- The player wakes in front of the circular jellyfish tank.
- Most tanks remain on, so the opening is beautiful rather than immediately
  hostile.
- The nearby entrance is visible through a short dark vestibule.
- The automatic doors and emergency release have no power.
- The objective changes to: **Follow the route and find another exit.**

On returning through Stage 2, one previously empty cylinder contains a slowly
moving silhouette. This is a state swap, not a puzzle or enemy.

## Stage 3 - Coastal Gradient

Duration: 4 minutes

- The room moves visually from river mouth to rock pool to shallow sea.
- Three exhibit symbols form the existing coastal puzzle.
- Solving it grants the staff access card and opens the next one-way gate.
- The recorded closing announcement uses the wrong aquarium name.

No inventory UI is required. The access card is a story flag.

## Stage 4 - Sunlit Reef

Duration: 2-3 minutes

- This is the brightest and safest-looking room.
- Broad shafts and strong caustics provide visual relief.
- Seating and a slow curved path encourage the player to look at the water.
- When the player reaches the exit, the entrance behind them is no longer
  visible because the corridor bend has become longer.

There is no interactable, puzzle, chase, or fail state in this stage.

## Stage 5 - Grand Ocean Hall

Duration: 4-5 minutes

- A compressed approach opens into the tallest room in the building.
- The visitor floor is long and narrow, and the entire right wall becomes the
  hero-tank acrylic panel.
- The player first sees only the near edge of the tank; the visible water area
  expands continuously while walking toward Stage 6.
- A few recessed benches on the left preserve the uninterrupted walking axis.
- The tank is physically larger than the exterior building could contain.
- A large silhouette crosses once, far behind the normal fish layer.
- The underwater arch is visible at the far end but remains locked.
- A light-control instruction directs the player forward to Stage 6.

This is the visual centerpiece. Do not interrupt it with a keypad puzzle.

## Stage 6 - Luminous Gallery

Duration: 4 minutes

- Three color clues collected naturally along Stages 3-5 drive the existing
  light-memory puzzle.
- Correctly reproducing the order powers the underwater arch.
- The completed pattern exposes a maintenance message:
  **"The route has no registered end."**
- A one-way shutter closes behind the player when they enter Stage 7.

## Stage 7 - Underwater Arch

Duration: 4-5 minutes

- The first third is safe and teaches the tunnel route and recesses.
- The arch is a separate tank structure, but repeats Stage 5's water color,
  caustics, and distant silhouettes to preserve visual continuity.
- A large silhouette approaches from the Grand Ocean side.
- A glass impact, water leak, and sequential light shutdown begin the only full
  chase.
- Two authored obstacles use emergency lighting rather than random placement.
- Reaching the Deep Sea bulkhead ends the pursuit and creates a checkpoint.

## Stage 8 - Deep Sea Descent

Duration: 4-5 minutes

- Ceiling height and exposure decrease gradually.
- Pressing one blue button starts four portholes in an east-to-west light
  sequence.
- The player follows the light while walking. The final pulse reveals the exit
  lever; missing it only requires pressing the button again.
- A short log implies that the building extends itself around sleeping
  visitors and repeats their remembered exhibits.
- Pulling the revealed lever releases the final emergency bulkhead.

The log does not explain another world and does not show one.

## Stage 9 - Deep Sea Panorama

Duration: 3-4 minutes

- The dome starts nearly black with one overhead water-surface light.
- The pursuing silhouette remains outside the dome glass.
- A controlled crack and red emergency-light sequence begins.
- The player uses the already restored emergency release; there is no new
  puzzle.
- The physical steel exit opens into overexposed morning light.
- Cut before showing a second aquarium or a portal transition.

Epilogue:

- The player is outside with the wet admission ticket.
- The ticket's printed floor map now contains Stages 7-9.
- A final distant water impact is heard after the door closes.

## Event state

```text
AwakenedInJellyfishTheater
EntranceChecked
CoastalAccessAcquired
GrandOceanContradictionSeen
LightMemorySolved
ArchGateOpened
ArchChaseCompleted
DeepSeaPulseSolved
EmergencyExitOpened
Escaped
```

## Checkpoints

1. After the entrance objective changes to following the route.
2. After the Luminous Gallery opens the arch.
3. At the Underwater Arch entrance.
4. After the chase, inside the Deep Sea bulkhead.

## Scope protection

- Three light interactions: Coastal symbols, Light memory, Deep Sea pulse walk.
- One complete chase in the Underwater Arch.
- One hero silhouette.
- One scripted final break sequence.
- Scenic stages add architecture, lighting presets, and ambient audio only.
- No combat.
- No general inventory.
- No dynamic structural destruction.
- No free-roaming enemy AI.
- No parallel-world window or portal rendering.
