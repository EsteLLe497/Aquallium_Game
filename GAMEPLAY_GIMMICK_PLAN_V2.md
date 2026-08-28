# Aquarium Gameplay Gimmick Plan V2

This revision supports the linear V4 layout and V5 event flow.

## Fixed gameplay budget

The route contains exactly:

- three authored puzzles;
- one spline-driven chase;
- one scripted final sequence.

Adding a scenic room does not authorize adding another puzzle, enemy, item
system, or simulation.

## Puzzle placement

| Puzzle | Stage | Existing system |
|---|---|---|
| Coastal symbols | 3 | Three ordered exhibit inputs |
| Light memory | 6 | Three color channels and lighting preset events |
| Deep Sea pulse walk | 8 | One pulse button, four sequential windows, and one exit lever |

## Chase placement

The Stage 7 Underwater Arch remains the only fail-state pursuit.

- Safe teaching section before the chase.
- Spline distance controls the threat.
- Two deterministic obstacles.
- One distance-based fail condition.
- 45-70 second target duration.
- Restart from the arch entrance.

The Stage 9 Deep Sea Panorama silhouette is presentation only and cannot chase
or navigate.

## Scenic-stage contract

Stages 2, 4, and 5 may use:

- lighting preset blends;
- ambient audio zones;
- static exhibit props;
- material animation;
- one-shot silhouette or state swaps;
- camera framing volumes that do not take control from the player.

They may not use:

- new puzzle definitions;
- collectible requirements;
- timers or fail states;
- new enemy behavior;
- unique render passes.

## Runtime ownership

```text
RouteDirector
  controls one-way gates, objectives, and checkpoints

PuzzleController
  owns the three puzzle definitions

ChaseDirector
  owns the Stage 7 route and fail distance

RoomPresentationController
  requests lighting, audio, and exhibit state changes

AquariumRenderer
  renders requested states and contains no gameplay decisions
```

The linear route simplifies streaming: each gate can release the room two
stages behind the player.
