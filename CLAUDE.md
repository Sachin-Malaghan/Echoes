# ECHOES — instructions for Claude Code

Read this file in full before changing anything. It is the standing spec for the project. The build recipe
it follows is `C:\SACHIN\Hollowlight\PLAYBOOK.md` (same machine, tools and fixes).

## What this is

A 2D puzzle platformer in Unreal Engine 5.8 (C++) for Android first, then iOS; Windows for development.
Every level is a 10-second time loop. When the loop ends (or you press Rewind) your run replays as a
translucent, solid **Echo** while you play again. Echoes press plates, hold doors, and you can stand on
their heads. Tagline: "You are your only teammate." App id `com.brainrotinteractive.echoes`,
publisher Brainrot Interactive Studios.

Decisions (2026-09-30, user): Unreal 5.8 C++ instead of the pitch's Unity; custom deterministic physics
instead of engine physics; the character and palette designs below.

## Architecture

```
Private/Core    engine-agnostic C++ (no Unreal headers): ECTypes, ECLevel(s), ECSim, ECAutopilot
Private/Game    Unreal wiring: AECGameMode (no pawn), AECPlayerController (input, lifecycle, capture),
                AECHUD, FECGame (screens, fixed-step loop, loop effects, progress), UECSaveGame
Private/Render  FECDraw (batched canvas triangles), FECWorldRenderer (world, Subject 7, Echoes, effects)
Private/UI      FECUI (HUD, menus, touch pads)
Private/Audio   UECAudioSynth (USynthComponent: every sound and the music synthesised live)
Private/Tests   automation tests
Tools/SimHarness  builds Core with plain MSVC and tests it in seconds
```

- **Core must never include Unreal headers.** `Tools/SimHarness/run.ps1` compiles it with `cl`.
- **No authored assets.** The game runs on `/Engine/Maps/Entry` with world rendering disabled; `AECHUD`
  paints everything with canvas triangles; the only asset is the engine's Roboto font.
- The sim is plain data (no pointers into other objects, no wall clock, no randomness). Keep it that way.
- The sim is ~80 KB (six full recordings), so `AECPlayerController` holds `FECGame` in a `TUniquePtr`
  (a UObject may not embed more than 64 KB).

## Echoes (the core system)

- Fixed tick **50 Hz**, a loop is **500 ticks**. Units are tiles; y is up; a body is 0.6 x 1.6 tiles.
- **State snapshots, not input replay.** Each tick the player's `{x, y, vx, vy, facing, anim, flags}` goes
  into a preallocated `FRecording` (10 KB). An Echo is moved to its recorded pose each tick, so replay is
  exact by construction (harness checks: error 0.000000).
- An Echo whose run ended early (Rewind) **holds its last pose** for the rest of the loop (flat ring on
  the floor). This is what makes "walk onto the plate, Rewind" work.
- Echoes are solid **from above only** (one-way platforms: you can stand and ride on them, walk through
  them sideways). They press plates and hold closing doors. Spikes and pickups ignore them.
- Every loop resets the level (doors, plates) — the `ILoopResettable` idea of the pitch is
  `FSim::ResetWorld()`. Only Echoes carry information across loops.
- Loop cap: `MaxEchoes` per level; `MaxEchoes + 1` runs. Ending the last run unsolved = OUT OF LOOPS
  (retry last loop / restart level). Death retries the current loop, Echoes kept.
- **Paradox rule (exact):** an Echo is in paradox when, for 3 consecutive ticks, it overlaps a closed door
  by more than 0.12 tiles on both axes, or when for 5 consecutive ticks it was recorded standing but has
  nothing under it (tile, closed door, or an *earlier* Echo). Paradox restarts the level. With only
  plates and doors (World 1) a paradox cannot happen in play (more pressing only opens doors); it
  becomes reachable with levers, boxes and paradox gates. It is tested with hand-made tracks.
- Stars (saved separately, best of all attempts): solved; solved within par loops; hidden shard.
  The shard needs one Echo more than par, so the three stars take two different solutions.

## Levels

`Core/ECLevels.cpp`. 16 x 9 tiles, one screen. Chars: `#` wall, `P` spawn, `E` exit, `*` shard, `^` spikes,
`a-d` plates, `A-D` doors (a door opens while *all* plates of its channel are pressed), `>` `<` `v` laser
emitters (wall tiles firing right / left / down).

**Lasers** (World 2): every emitter in a level follows one schedule, `LaserOn` / `LaserOff` ticks repeating
from tick 0 (`LaserOff` 0 = always on). A beam runs until a wall, the solid part of a door, or an Echo -
Echoes absorb beams (they are ghosts), the player dies in one. The beam flickers for 15 ticks before it
turns on and shows as a dotted guide while dark. The core trick: step into a beam's path while it is dark
and rewind before it fires - the Echo holds that pose and shields everything behind it for good. Later
runs must wait until their Echoes are in place (Echoes replay from the start of the loop).
Each level carries two autopilot plans (`m<x>` move, `j` jump, `w<n>` wait, `u<n>` wait until tick,
`r` rewind, `|` next run): `Plan` must solve **at par**, `ShardPlan` must solve with the shard. The
harness also runs the final par run alone (must fail) and a list of cheat routes (must fail).
Keep plates at least 2 tiles from their door: a closing door stops on anyone under it.
Jump height ~2.26 tiles; standing on one Echo reaches ~3.9; on two ~5.5.

20 levels, 10 per world (level select pages by world; the HUD numbers them `world-n`):

| # | World 1, The Lab | idea | par | # | World 2, The Factory | idea | par |
|---|---|---|---|---|---|---|---|
| 1-1 | First Step | plate far from its door | 2 | 2-1 | Hot Wire | drop in the dark, rewind: the Echo takes the floor beam | 2 |
| 1-2 | Stepping Stone | Echo as a step | 2 | 2-2 | Curtain | key under a ceiling beam; time the second beam | 2 |
| 1-3 | Two Keys | two plates, one door | 3 | 2-3 | Crossfire | beams from both walls, an Echo on each side | 3 |
| 1-4 | Tower | two-Echo stack | 3 | 2-4 | Stairwell | stack under a beam, climb in one dark window | 3 |
| 1-5 | Relay | two doors in series | 3 | 2-5 | Hot Walkway | beam sweeps the walkway over spikes | 3 |
| 1-6 | Handoff | one Echo moves between two keys | 2 | 2-6 | Assembly Line | three keys under three beams | 4 |
| 1-7 | Mind the Gap | spikes, walkway one Echo up | 2 | 2-7 | Blind Corner | key in the laser's mouth: one Echo, two jobs | 2 |
| 1-8 | Ledge Door | key + step | 3 | 2-8 | Overtime | the key's beam sleeps only late in the loop | 3 |
| 1-9 | Upstairs | the key holder is also the step | 3 | 2-9 | Gauntlet | two keys under beams, two doors | 3 |
| 1-10 | Clockwork | two doors, stack on the key | 4 | 2-10 | The Core | everything | 4 |

The harness also runs a list of cheat routes (fewer Echoes, sprinting through one dark window) that must
all fail. Add one for every new level.

## Art direction

- **Subject 7** (player): hooded lab coat with a torn hem that trails and flutters, warm white `#F4F7FF`,
  dark hood opening with two glowing cyan eyes, a cyan time-core on the chest that pulses faster in the
  last 3 seconds, cyan rim light. Legs/arms animate from distance travelled, so Echoes animate exactly.
- **Echoes**: glass versions of the same figure (translucent fill, bright edges), each with its own colour
  *and* effect: 1 amber `#FFB020` soft trail, 2 magenta `#FF3D9A` chromatic split, 3 violet `#9B6BFF`
  scanlines, 4 lime `#B6FF3D` flicker, 5 ice `#7FD4FF` wide afterimages, 6 rose gold `#FFC2A8` blur.
- **World palettes** (near-black background, one accent, one danger; `GPalettes` in the renderer):
  Lab `#0A1220`-`#14243D`, mid `#1E3A5F`, accent cyan `#3DF2FF`, danger `#FF4D5E` (glass panels, monitors).
  Factory `#160D0A`-`#2A1610`, mid `#4A2A1C`, orange `#FF8A2B`, hot pink lasers `#FF2E63` (turning gears,
  steam). Subject 7 and the shard stay cyan in every world. Clocktower `#0F0A1E`-`#221541`, gold `#FFD166`, `#FF5A7A`. Rift `#05050F`-`#120A2E`,
  magenta `#FF3DDC` + cyan.
- Plate/door channels share a colour (teal `#2EE6C5`, violet `#A77BFF`, amber `#FFC857`, rose `#FF6FA0`);
  a dotted circuit on the floor links them and pulses while pressed. Exit is always a warm-white portal.
- Big warm light goes *behind* the play layer; only small cores on top (playbook lesson).
- Background: glass panels, dim monitors, ceiling lamps with light shafts, and a huge dial that is the
  loop clock.
- Effects: rewind = 0.6 s VHS scrub (the run plays backwards, doors too, scanlines, tracking bands);
  paradox = glitch bars + flicker; solve = flash, the player dissolves into the portal while Echoes keep
  moving in slow motion.

## Audio

All sound is synthesised in `UECAudioSynth` (no audio files): voices with envelope, tone sweep, soft square,
band-passed noise, vibrato, and a small Schroeder reverb. The game thread calls `IECAudioSink`
(`Play(EECSound, pan, strength, delay)`, `OnBeat`, `SetMix`); commands cross to the audio thread through a
lock-free queue, mix targets through atomics.

- **Effects** (`EECSound`): jump, land, footsteps, softer "ghost" jump/land for Echoes, plate chime, door
  servo + thud, shard sparkle, laser zap / charge warning / continuous hum, clock ticks in the last 3 s,
  tape-rewind sweep, paradox glitch, death, solve chord, out-of-loops, stars, menu clicks.
  Silent in attract mode (only the music plays behind the title).
- **Music is locked to the loop**: 120 BPM, 20 beats = one 10-second loop = 5 bars (i - VI - III - VII - i).
  `OnBeat(tick / 25)` is called from the sim clock, so every loop plays the same bars. The clock tick,
  bar pulse and drone always play; **each Echo adds one instrument**: 1 bass, 2 arpeggio, 3 bell melody,
  4 shimmer, 5 hats, 6 pads. The Lab is A minor and glassy; the Factory D minor, lower, with an anvil clank.
- Settings: MUSIC / SOUND toggles in the pause menu (saved).
- Test `Echoes.Audio.EverySoundAudibleNoClipping` renders every effect and each music layer offline,
  checks they are audible and never clip, and writes them to `Saved/AudioPreview/*.wav` to listen to.

## Controls

Keyboard A/D or arrows, Space/W jump, **R rewind now**, T restart level, Esc/P pause. Gamepad: stick/D-pad,
A jump, Y or RB rewind, View restart, Menu pause. Touch: arrows bottom-left, jump bottom-right, HUD
buttons top-right (rewind, restart, pause). No engine virtual joystick (`CreateTouchInterface` is empty,
`DefaultTouchInterface=None`).

## Commands

```
powershell -ExecutionPolicy Bypass -File Tools\SimHarness\run.ps1 [-Level N] [-Trace]   # core tests, seconds
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="<repo>\Echoes.uproject" -game -rocket
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" EchoesEditor Win64 Development -project="<repo>\Echoes.uproject" -waitmutex
powershell -ExecutionPolicy Bypass -File Tools\Validation\run_tests.ps1 [-NoBuild]      # automation tests
powershell -ExecutionPolicy Bypass -File Tools\Validation\capture.ps1 [-Phone]          # screenshots -> Saved\Screenshots\WindowsEditor
powershell -ExecutionPolicy Bypass -File Tools\Build\package.ps1 -Platform Win64|Android [-Config Development] [-Release]
```

Console: `ec.Play N`, `ec.Autopilot 0|1|2`, `ec.UnlockAll`, `ec.ResetProgress`, `ec.Debug.Paradox`, `ec.HideUI`.
Command line: `-ECLevel=N`, `-ECForceTouch`, `-ECCapture [-ECCaptureTag=x]`.

## Milestones

- [x] **M1 core** — movement, 10 s loop, Echo record/playback (exact), plate + door, standing on Echoes,
  exit + level complete, paradox detection, 3 levels, harness + automation tests, zero allocations per tick.
- [x] M1 Unreal layer — renderer, HUD, touch pads, rewind / paradox / solve effects, menus, save, capture.
- [ ] M1 on a real Android phone (touch, 60 FPS, aspect).
- [x] 20 levels: World 1 (10) and World 2 with lasers (10), all proven at par, with shard solutions and cheat checks.
- [x] Synthesised sound effects and loop-locked layered music; app icon; store listing, privacy site, release guide.
- [ ] M2 — more levels per world (15 each), rewind SFX, synthesized audio (one music layer per Echo), level select polish.
- [ ] M3 — World 2 (levers, boxes, lasers), paradox in play, settings (joystick vs buttons, repositionable pads).
- [ ] M4 — ads/IAP behind interfaces, analytics, store assets, trailer, Play release.

## Working agreement

Plain step-by-step instructions for the user; ask before installing software or publishing; never put
secrets in git; commit and push only when asked; commit messages end with the session's Co-Authored-By line.
