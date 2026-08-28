# Aquarium Event Flow V3

> Superseded by `EVENT_FLOW_V4.md`, which starts the player in Room 4 and uses
> the imitation-aquarium story.

Target scope: a 20-30 minute first playable that can be produced in roughly
two to four weeks.

The route is intentionally linear. Room 4 is a required side puzzle that
returns the player to Room 3; the underwater arch is entered from Room 3.

## Flow overview

```text
1 Entrance
  -> 2 Coastal Gallery
  -> 3 Main Tank reveal
  -> 4 Light Gallery power puzzle
  -> return to 3
  -> 5 Underwater Arch incident
  -> 6 Deep Sea override puzzle
  -> 7 Final Dome break/chase
  -> Escape
```

## Act 0: Entrance lockdown

Duration: 2-3 minutes

- The player enters during normal operation.
- A short blackout interrupts the entrance lighting.
- Emergency shutters close and the public exit locks.
- The first announcement establishes that the aquarium systems are unstable.
- Movement, interaction, and flashlight controls are taught here.

Implementation:

- One trigger volume.
- One door state change.
- One lighting-preset transition.
- One short subtitle/audio sequence.

## Act 1: Coastal Gallery orientation

Duration: 3-4 minutes

- Bright shallow-water lighting provides a safe contrast to later rooms.
- The player finds the first access card or breaker key.
- A small environmental puzzle teaches reading exhibit labels or matching
  symbols without creating a full inventory system.
- A distant silhouette crosses the Main Tank viewing window.

Implementation:

- One three-step symbol puzzle.
- One scripted silhouette, not a full creature AI.
- Checkpoint after unlocking the Main Tank Hall.

## Act 2: Main Tank reveal and hub

Duration: 3-4 minutes

- The player enters the enlarged Main Tank Hall.
- View A presents the entire shared tank and establishes the main landmark.
- The underwater-arch gate is visible but has no power.
- Room 4 is the only powered service route available.
- A crack, shadow, or pressure pulse foreshadows the final event.

Implementation:

- Main Tank Hall owns the route decision and arch-gate state.
- Do not start a chase here; preserve the spectacle beat.
- The Main Tank preset uses the strongest stable blue lighting.

## Act 3: Light Gallery power restoration

Duration: 4-6 minutes

- The player enters Room 4 from Room 3.
- Violet and magenta emissive strips replace expensive extra volume lights.
- A color-order or wavelength puzzle restores power to the arch gate.
- The player returns to Room 3 through a second doorway or the same doorway.
- The arch entrance opens in front of the hero tank.

Implementation:

- Required side loop, shown as the dashed purple route.
- One puzzle with three switches and a data-authored answer.
- Arch unlock event is dispatched to Room 3.
- No additional volumetric render pass.

## Act 4: Underwater Arch incident

Duration: 4-5 minutes

The 34 m tunnel has three authored beats:

1. Dark entry: the player sees only silhouettes overhead.
2. Skylight center: caustics and volume reach their peak.
3. Blue-violet exit: warning lights begin and visibility falls.

At the central skylight, use one major incident:

- A large shadow passes overhead.
- A glass impact occurs behind the player.
- The entry shutter closes.
- Water begins leaking, forcing forward movement.

For the first playable, use a spline/scripted threat rather than general
navigation AI.

Implementation:

- Three sub-zone presets inside one tunnel zone.
- One forward-only pressure event.
- Checkpoint at the Deep Sea Gallery entrance.

## Act 5: Deep Sea override

Duration: 4-5 minutes

- Exposure falls and only sparse local emissive lights remain.
- View C reframes the same hero tank through small portholes.
- The player obtains an emergency override code or valve sequence.
- Something visible through one porthole disappears when viewed through the
  next, building tension without combat.

Implementation:

- One observation/sequence puzzle.
- Reuse the same tank simulation through four small portals.
- Keep the creature as a timed animation or silhouette.

## Act 6: Final Dome and escape

Duration: 4-6 minutes

- The Final Dome begins in calm blue.
- The override activates, then the tank-break event starts.
- Lighting transitions to emergency red.
- The threat enters or appears behind the glass.
- A short chase ends at the escape door.

Recommended first-playable chase:

- Fixed route.
- Two scripted obstacle breaks.
- Distance-based fail condition.
- No free-roaming enemy search behavior.

This is cheaper, easier to tune, and more cinematic than building full enemy
AI for a two-week schedule.

## State model

Use one small event-state enum:

```text
Normal
Lockdown
MainTankReached
ArchPowerRestored
ArchIncident
DeepSeaOverride
FinalBreak
Escaped
```

Each room reads the state and exposes only its local response. Avoid one giant
scene script that directly manipulates every door and light.

Useful events:

```text
OnRoomEntered(roomId)
OnPuzzleSolved(puzzleId)
OnLightingPresetRequested(presetId, blendSeconds)
OnDoorStateRequested(doorId, state)
OnCheckpointReached(checkpointId)
OnSequenceRequested(sequenceId)
```

## Scope protection

For the first playable:

- Three puzzles maximum.
- One scripted creature.
- One chase.
- One tank-break sequence.
- No combat.
- No general inventory.
- No full dynamic destruction.
- No full enemy navigation mesh search.

This keeps the Main Tank, underwater lighting, and glass-break presentation as
the portfolio-quality features instead of spreading the schedule across
unfinished systems.
