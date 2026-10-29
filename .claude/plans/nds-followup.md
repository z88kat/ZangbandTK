# Nintendo DS — follow-up

**Status:** open, parked 25 October 2026 · **Diary:** 23–25 October 2026 ·
**Card readme:** [`src/nds/card-readme.txt`](../../src/nds/card-readme.txt)

The DS build works on real hardware: an original DS with the 8 MB Memory Expansion
Pak, launched from a DSpico. The dungeon plays. The town does not play properly yet.
This is everything left over, so that the work can be picked up without re-reading
the session that got it here. The 3DS build is a separate piece of work and is not
covered here.

---

## 1. Where it stands

What the DS build does now, and where it is in the code:

| | |
|---|---|
| Font | 5x8 on both machines. The term is 80 columns, 400 px, in a 512x256 bitmap; the top screen shows 51 columns of it (`src/nds/nds-draw.c`) |
| Over the map | The view centres on the player. Message row 0 and status row 23 stay put while the map rows scroll: a VCOUNT interrupt rewrites `REG_BG2X` at lines 8, 184 and 192 (`nds_video_line()`) |
| Elsewhere | The view follows the cursor between three stops; L or R alone steps it (`nds-buttons.c`) |
| Sidebar | Drawn on the bottom screen under the keyboard, two columns, rebuilt from `Term->scr` at each refresh, only while the map is showing (`main-nds.c`) |
| Messages | Break at 51 columns: `msg_line_width` in `ui-input.c`, zero (the whole line) everywhere but the DS |
| Centring | `center_player` defaults on for new DS characters (`options_init_defaults()` in `option.c`) |
| Memory | Squares, noise and scent rows in the pak (`cave.c`); monster, spell, vault and room text moved there while loading; one info allocation per level; the player's map has no monster table; `level-max:monsters:128`; randname's table in the pak; `-flto` |
| The pak | 16-bit bus, no byte writes. Only halfword-written or read-only data may go there. `mem_free`/`mem_realloc` route pak pointers (`z-virt.c`) |
| World | `wild:blocks:65`, `wild:block-size:4`, `wild:towns:1`, live surface 36x36 (`src/nds/constants-overrides.txt`) |
| Speed | DL1 step 367 ms → 106 ms in melonDS (see §3) |

---

## 2. The town: monsters re-roll on every step

**The problem.** The live surface is 36 grids across. It is rebuilt whenever the player
is within the recentre margin of its edge, `MAX(2 * block_size, max_sight + 8)` = 28,
which on a 36-grid window is always. Every rebuild rolls the country's monsters and the
townspeople afresh, so on the DS they jump about at every step. Objects survive a rebuild
(`wild_harvest()` / `wild_restore()`, WLD-04) and so do uniques the player has hurt
(WLD-04b); pets are carried separately. Nothing else is.

**Tried and taken back: a bigger window.** `wild:cache-blocks:729` gave a 108-grid
window, 26 steps between rebuilds. On the hardware every key was slow and the game soon
locked up. In melonDS a fresh character died generating the surface with "Out of memory!
(50176 bytes)": 171 KB free, but none of it in one piece large enough for the level's
50 KB monster table. The window cost about 70 KB of long-lived blocks, and the per-step
whole-level work (noise, scent, view, lighting) went from 1,296 grids to 11,664.

**The fix to build: carry monsters across a rebuild.** On every platform, not just the
DS; the desktop reshuffles too, every forty steps instead of every one.

- Harvest every monster on the old window, not only hurt uniques: race, world grid, hit
  points, sleep and the other timed effects worth keeping.
- After the rebuild, put back those whose grids fall inside the new window.
- Populate only the grids that were not in the old window, or the density climbs at
  every step. `wild_populate()` takes the window's offset; it needs the old one too.
- `wild_town_people()` only for a town that has just come into the window.
- Groups, held objects and mimics are what make monsters harder to copy than objects
  (see the comment at WLD-04b in `wild_harvest()`). Decide what a carried monster keeps.
- Then measure the per-step cost on the DS: with a rebuild every step, this adds work
  to each one.

---

## 3. Speed

**Measured** in melonDS with timers in a scratch build (§8). A step on DL1:

| Change | Step |
|---|---:|
| Start | 367 ms |
| `player_handle_post_move()` ran `update_view()` and left `PU_UPDATE_VIEW` set, so `update_stuff()` ran it again. Now cleared after the direct call | 234 ms |
| `update_view()` bounded to max-sight boxes around the old and new positions (`cave-view.c`, `cave_view_forget()` from `cave_free()`) | 133 ms |
| `CheckEvents(false)` no longer waits for a frame; input is read at most once a frame | 106 ms |

melonDS does not seem to charge slot-2 wait states: moving the row tables out of the pak
and reordering `make_noise()`'s checks changed nothing there. Expect the DS to be slower.

**Still to do, largest first:**

- [x] `calc_lighting()` still covers the whole level, about 30 ms. Bounding it is not
      exact the way the view loops were: light far from the player can be read by
      monster AI. Work out who reads `square_light()` and `square_islit()` before
      choosing.
      **Done 29 Oct 2026.** Nothing reads it far off: only `update_view()` (squares in
      sight), `map_info()` (squares in view) and the player's own square. It now covers
      the view box and a ring of one. Exact: 0 differences in about 11 million squares
      compared, with and without lava, and the same seeded borg game. 6–9x faster on the
      Mac; not yet measured on the DS.
- [ ] `make_noise()` and `update_scent()`, about 27 ms. Scent ages every grid every
      turn; a turn stamp per grid would make that the 25 grids around the player.
      Noise is cleared and flooded over everything reachable, hearing reaches 100
      grids (200 for one monster), so bounding it changes behaviour.
      **29 Oct 2026, noise:** confirmed that bounding changes behaviour, and by more than
      hearing: a sleeping monster within 50 steps wakes faster for the noise
      (`mon-move.c`, around line 2221), and fleeing and scent-following moves compare
      noise whatever the monster's hearing. So the flood still covers the level, but is
      about 3.5x faster on the Mac with identical values (18.7 million squares
      compared), and its queue is kept between turns in the pak (`mem_alloc_alt`) instead
      of a new level-sized allocation every turn. Scent is untouched; not yet measured
      on the DS.
- [ ] `square(c, grid)->info` reads the info pointer from the square, which is in the
      pak. The flags are one block per chunk, so the pointer can be computed instead.
- [ ] Redraw, about 18 ms a step.
- [ ] Get hardware numbers: `Zangband-debug.nds` shows free memory and how long the last
      command took, above the keyboard.

---

## 4. Input

- [ ] Keys are only read while the game waits for input. A tap made during a slow turn
      is lost, which makes a slow turn feel worse than it is. Scan in the vblank
      interrupt and queue what was pressed.
- [ ] `NDS_BTN_FILE` and `NDS_SCRKEY_FILE` still point at `/angband/nds/`. They should
      be under `/zangbandtk/`. Document the two files in the card readme when they move.

---

## 5. Screen

- [ ] Screens wider than 51 columns need L/R: the options menu's yes/no column, the
      character sheet, stores, long item names.
- [ ] Over the map the status line shows columns 13 to 63. A long run of timed effects
      will be cut off at the right.
- [ ] The cursor on row 0 or 23 over the map moves nothing, by design. A prompt longer
      than 51 columns would be cut off.
- [ ] A fatal error is written by `nds_log()` at a position kept in a static, which can
      be off the screen or under the keyboard by the time it is used. The town lock-up
      probably showed its "Out of memory" there. Make a fatal message always visible:
      clear, pan to 0, split off, print at the top.

---

## 6. Memory

- [ ] Each class keeps its own copy of its realms' spells: 357 KB. Sharing them per realm
      would give the game room to grow.
- [ ] Fragmentation, not the total, is what fails: the level's monster table needs 50 KB
      in one block. Watch it whenever a long-lived allocation is added.
- [ ] While the player is in a dungeon, the surface's known map is kept
      (`wild_keep_knowledge()`). It is small at 36x36; it would not be at any larger
      window.

---

## 7. Not yet played

- [ ] Deeper levels: caverns, vaults, labyrinths, breeders against the 128-monster cap.
- [ ] Memory and step time below DL1.
- [ ] A DSi (no Slot-2). Whether a DSi-mode build would get the DSi's 16 MB has not been
      looked at.
- [ ] Desktop saves on the DS: the world constants differ, so a desktop character lands
      in the wrong world. The readme says not to. A guard that refuses such a save
      would be better.

---

## 8. Testing recipe

- **Build** in the scratch worktree, never in the repo: BlocksDS via
  `docker run --rm -v <worktree>:/work -w /work/src skylyrac/blocksds:slim-latest
  make -f Makefile.blocksds -j8`. Add `CFLAGS="-Werror -DDEBUG_MEMORY_USAGE"` for the
  debug ROM. The 3DS builds with `devkitpro/devkitarm` and `make -f Makefile.3ds`.
- **Card data:** `lib/{gamedata,help,screens,customize}` into `zangbandtk/lib/`, an empty
  `lib/user/save`, then `scripts/nds_constants` on the copy.
- **melonDS 1.1:** DS mode, DLDI on with folder sync pointed at the card folder. Insert
  the pak with File > Insert add-on cart > Memory expansion, then System > Reset, every
  launch. melonDS rewrites the ROM it opens, so keep a clean copy and never put an opened
  one on a real card. It writes the card folder back only when it quits.
- **Profiling** (scratch only, never committed): wrap functions with `cpuGetTiming()` into
  an array; have the debug build read keys from `/zangbandtk/test-keys.bin` when it
  waits, and append each turn's costs to `/zangbandtk/prof.txt`. Then melonDS runs behind
  other windows (`open -g`, `screencapture -l<window id>`) and only the pak menu needs
  focus. Worth committing as a proper test harness, like the DOS `-mtest`.
- **Never send keys to melonDS while someone is using the Mac.** They go to whatever is
  in front.
- **Save:** the scratch test character is on DL1. The debug `^A` commands could not be
  reached with injected keys.
