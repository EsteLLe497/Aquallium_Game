# Aquarium Event Flow V4 - The Imitation Aquarium

Target scope: a 20-30 minute first playable.

## Premise

The player fell asleep inside the Light Gallery of an ordinary aquarium.
They wake in what appears to be the same facility after closing, but the space
is an imperfect reconstruction made from the player's memories.

The first objective is mundane: reach the entrance and leave.
The true objective is revealed gradually: escape the imitation aquarium and
return to the original world.

The working name for the other space is the **Imitation Aquarium**. The final
name can remain hidden from the player.

## Narrative rule

Do not explain the supernatural premise immediately.

The reveal progresses through five levels:

1. **Isolation** - the aquarium appears to be merely closed and empty.
2. **Doubt** - clocks, signs, and exhibits do not match the player's memory.
3. **Contradiction** - the floor plan and the Main Tank contain impossible
   details.
4. **Proof** - the original aquarium is briefly visible through the water.
5. **Explanation** - the Deep Sea records describe a space that imitates
   observed memories.

## Flow overview

```text
4 Light Gallery - wake up
  -> 1 Entrance - exit is locked
  -> 2 Coastal Gallery - first key and first inconsistency
  -> 3 Main Tank Hall - landmark reveal and impossible evidence
  -> 4 Light Gallery - return and solve the light-memory puzzle
  -> 3 Main Tank Hall - arch gate opens
  -> 5 Underwater Arch - original world glimpsed, then full chase
  -> 6 Deep Sea Gallery - align the return path
  -> 7 Final Dome - timed stabilization and return
  -> Original Room 4 - wake again
```

## Prologue: Wake in Room 4

Duration: 2-3 minutes

- The player wakes on a bench in the Light Gallery.
- Most decorative lights are off; only violet emergency strips remain.
- There are no visitors, staff, footsteps, or normal closing announcements.
- The player's phone shows the time but has no signal.
- A cleaning cart or personal item suggests the player really did fall asleep.
- The immediate objective is: **Go to the entrance**.

Keep the scene believable. Do not use a monster, moving mannequin, or obvious
supernatural effect yet.

Cheap environmental irregularities:

- One wall label uses a slightly wrong exhibit name.
- A clock is stopped at the time the player fell asleep.
- A familiar poster has different colors.

## Act 1: Check the Entrance

Room: 1
Duration: 2-3 minutes

- The automatic entrance doors do not react.
- The emergency release has no power.
- The outside glass is black; it does not show the street or reflections.
- The information display reports that all visitors have left.
- The printed floor map looks mostly correct, so the player assumes this is a
  power or security failure.
- The new objective is: **Find another way to restore the exit**.

Do not let the front door loop or teleport yet. A locked physical door keeps
the first act grounded.

## Act 2: Coastal Gallery - first inconsistency

Room: 2
Duration: 3-4 minutes

- The player solves a small exhibit-symbol puzzle and obtains a staff access
  card.
- A recorded closing announcement contains the wrong aquarium name.
- The player's admission ticket and the exhibit date disagree.
- A distant silhouette crosses the Main Tank glass.

Puzzle scope:

- Three symbols.
- One authored answer.
- No general inventory; the access card is a story flag.

Narrative result:

The player still believes they are in the original building, but now suspects
that something is seriously wrong.

## Act 3: Main Tank Hall - contradiction

Room: 3
Duration: 3-4 minutes

- View A reveals the shared hero tank.
- The tank is larger and deeper than the player remembers.
- A species or rock formation from a childhood memory appears inside it,
  despite never belonging to this aquarium.
- The underwater-arch gate is visible but unpowered.
- The room map shows Room 5 even though the player remembers no such tunnel.
- A pressure pulse travels across the glass.

The arch control points back to the Light Gallery.

Narrative result:

The aquarium is not merely closed. It contains personal memories that should
not exist in a public exhibit.

## Act 4: Return to Room 4 - light-memory puzzle

Duration: 4-5 minutes

The player returns to the room where they woke.

- Restored power reveals that the light strips can reproduce colors from the
  player's original visit.
- The player reconstructs the remembered lighting order.
- Correct colors expose hidden writing or a reflected sentence:
  **"This is not the aquarium you entered."**
- The arch gate unlocks in Room 3.

Recommended puzzle:

1. Read three colored clues collected from Rooms 1-3.
2. Set three light channels in the correct order.
3. Hold the completed pattern for several seconds.

Use emissive material changes and bloom. Do not add another volumetric pass.

## Act 5: Enter the Underwater Arch

Route: Room 3 -> Room 5
Duration: 4-5 minutes

The player returns to the Main Tank Hall and enters the arch directly from
Room 3.

Three lighting beats:

1. **Dark entry** - overhead silhouettes are difficult to identify.
2. **Skylight center** - View B briefly shows the original, populated aquarium
   on the other side of the glass.
3. **Blue-violet exit** - the view disappears and warning lights begin.

Incident:

- Something strikes the glass behind the player.
- The entry shutter closes.
- Water starts leaking.
- The tunnel lights extinguish from back to front.
- A spline-driven threat begins the only full fail-state chase.
- Two authored obstacles require the player to read emergency lights and use
  the wall recesses shown before the chase.
- Reaching the Deep Sea door ends the pursuit and creates a checkpoint.

This is the proof stage: the original world exists beyond the imitation.
The full chase specification is documented in `GAMEPLAY_GIMMICK_PLAN_V1.md`.

## Act 6: Deep Sea Gallery - explanation and return puzzle

Room: 6
Duration: 4-5 minutes

- Exposure falls and sparse blue emissive lights guide the player.
- Four View C portholes show slightly different versions of the Main Tank.
- A short audio log describes an observed space that reproduces memories but
  cannot perfectly reproduce time, names, or human faces.
- The player aligns the four portholes into one consistent view of the original
  aquarium.
- The alignment generates the return path in the Final Dome.

The explanation should remain incomplete. Avoid a long scientific exposition.

Possible origin:

- An unknown deep-sea organism reacts to observation.
- The hero tank acts as an anchor.
- Sleeping visitors can be pulled into the reconstructed space.

These details can remain implied through logs and visuals.

## Act 7: Final Dome - stabilize and return

Room: 7
Duration: 4-6 minutes

- The dome starts in calm blue.
- The aligned return image appears as a reflection in the glass.
- The imitation entity tries to prevent the player from reaching it.
- Lighting changes to emergency red.
- The player activates three stabilizers in the order learned from Views A-C.
- A projected flood and shrinking time window create pressure.
- The imitation entity remains visible outside the dome but does not start a
  second pursuit.

Recommended finale scope:

- One authored input order.
- One forgiving time limit.
- Material, exposure, and lighting changes instead of dynamic water simulation.
- A wrong input removes time but does not cause an instant fail.

Return:

- The player passes through the reflected exit or is engulfed by tank water.
- The screen cuts on impact rather than simulating a full portal.

## Epilogue: Original Room 4

- The player wakes on the same bench in the original Light Gallery.
- Visitors and normal aquarium audio have returned.
- Only a few seconds have passed since falling asleep.
- Staff or a companion asks whether the player is okay.
- The access card or admission ticket from the imitation aquarium remains wet.
- In the final shot, the Main Tank contains the same impossible silhouette.

The wet object confirms that the experience was not only a dream while keeping
the larger mystery unresolved.

## Event state

```text
AwakenedInLightGallery
EntranceChecked
CoastalKeyAcquired
MainTankContradictionSeen
LightMemorySolved
ArchGateOpened
OriginalWorldGlimpsed
ReturnViewAligned
ArchChaseCompleted
Returned
```

Room 4 is visited twice. Its behavior must depend on event state, not only on
`OnRoomEntered`.

Useful events:

```text
OnObjectiveChanged(objectiveId)
OnPuzzleSolved(puzzleId)
OnEvidenceDiscovered(evidenceId)
OnLightingPresetRequested(presetId, blendSeconds)
OnDoorStateRequested(doorId, state)
OnSequenceRequested(sequenceId)
OnCheckpointReached(checkpointId)
```

## Checkpoints

1. After the Entrance objective changes to exploration.
2. After the Light Gallery puzzle unlocks the arch.
3. At the Water Arch entrance, before the chase.
4. After the Water Arch chase, at the Deep Sea Gallery entrance.

## Scope protection

For the first playable:

- Three puzzles: Coastal symbols, Light memory, Deep Sea alignment.
- One scripted glass incident.
- One complete chase.
- One hero creature silhouette.
- One ending sequence.
- No combat.
- No general inventory.
- No full dynamic destruction.
- No free-roaming enemy AI.

The portfolio focus remains environmental storytelling, room-based lighting,
water rendering, glass presentation, and a controlled cinematic chase.
