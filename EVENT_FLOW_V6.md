# Aquarium Event Flow V6 - Follow the Water Down

Target scope: a 24-32 minute first playable.

## Premise

The player wakes on a bench in the Jellyfish Theater after closing. The public
exit has no power. With no staff and no usable return route, the player follows
the ordinary visitor arrows toward an emergency exit.

At first the route resembles a believable aquarium. It then continues downward
far beyond the building's possible depth.

## Flow

```text
2 Jellyfish Theater - wake
  -> 1 Entrance - public exit has no power
  -> 2 Jellyfish Theater - the first display changes
  -> 3 Coastal Promenade - follow the exhibit wall downhill
  -> 4 Reef Slope - bright scenic descent
  -> 5 Grand Ocean Hall - hero tank and impossible scale
  -> 6 Descending Underwater Arch - descend and survive the chase
  -> 7 Twilight Promenade - quiet deep-sea walk
  -> 8 Deep Sea Panorama - final silhouette and physical exit
```

There are no branches after the entrance check and no intermediate gate puzzles.

## Beats

### Opening: 0-4 minutes

- Wake in a beautiful, mostly powered Jellyfish Theater.
- Check the nearby entrance.
- The public exit and release panel do not respond.
- Return to the route; one empty cylinder now contains a faint silhouette.

### Shallow promenade: 4-10 minutes

- Walk continuously through coastal and reef exhibits.
- The floor drops 1.5 m over both zones.
- The waterline appears to rise relative to the player.
- A closing announcement says a different aquarium name.
- No access card, symbol puzzle, or locked transition is used.

### Grand Ocean Hall: 10-15 minutes

- The right-side hero tank reveals itself over the length of the hall.
- The visible tank volume exceeds the exterior dimensions.
- A distant large silhouette crosses once.
- The window curves overhead at the far end.
- The same water becomes the Descending Underwater Arch without an intervening
  room.

### Arch descent and chase: 15-21 minutes

- The first third is a safe descent.
- Surface caustics fade while the floor drops toward the deep level.
- A glass impact and sequential rear-light shutdown start the only chase.
- The player runs, moves around two authored obstacles, and uses two recesses.
- The chase ends as the arch opens into a quiet twilight promenade.

### Twilight walk: 21-26 minutes

- Four deep-sea windows and luminous exhibits guide the route automatically.
- The large silhouette appears one window ahead during dark gaps.
- An optional short announcement implies that this route is absent from the
  official floor map.
- There is no button sequence or pulse puzzle.

### Finale: 26-32 minutes

- Enter the Deep Sea Panorama at the lowest floor.
- The silhouette circles the curved window.
- A controlled crack and emergency-light sequence begin.
- Pull the physical emergency release.
- Walk through overexposed morning light and cut before showing another world.

## Player actions

Required:

1. Inspect the powerless entrance release.
2. Walk the visitor route.
3. Run through the authored arch chase.
4. Pull the final emergency release.

Optional:

- Pause at benches and exhibit windows.
- Listen to one short announcement in the Twilight Promenade.

## Event state

```text
AwakenedInJellyfishTheater
EntranceChecked
PromenadeEntered
GrandOceanContradictionSeen
ArchDescentEntered
ArchChaseStarted
ArchChaseCompleted
TwilightPromenadeEntered
EmergencyExitOpened
Escaped
```

## Checkpoints

1. After the entrance objective changes to following the route.
2. At the Grand Ocean Hall entrance.
3. At the safe start of the Descending Underwater Arch.
4. After the chase, at the Twilight Promenade entrance.

## Scope protection

- No inventory.
- No color-memory puzzle.
- No coastal symbol puzzle.
- No pulse-window puzzle.
- One chase with authored obstacles.
- One hero silhouette reused across distant, arch, and final presentations.
- One final lever.
- No free-roaming enemy AI.
- No dynamic structural destruction.
