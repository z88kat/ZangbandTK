# The Borg — Development Plan

**Status:** draft · **Scheduled as:** [§7 of the Phase 2 development
plan](phase2-development-plan.md#7-automated-play-testing--revive-the-borg) ·
**Recorded as:** DEC-66 in [decisions.md](decisions.md)

Defines twenty-one requirements, BRG-01 to BRG-21, in five phases. Not a
gameplay milestone: nothing in M0–M11 blocks on it, and it blocks nothing. It
exists because there is no test in this repository that plays the game, and the
borg is the only thing that can be made to.

Every number below was measured against `76b6ab2f4` (M10 phase 7) on
3 September 2026, not inferred. The method is in [§6](#6-how-the-findings-were-measured)
so the figures can be re-checked when they go stale.

---

## 1. Why

We have 110 unit suites running in CI through `ninja alltests`. They test rules
in isolation and they are good at it. **Not one of them plays the game.**
Nothing walks a character out of a town, across the wilderness, into a dungeon
and back — and nothing else in the tree can be made to: the DOS smoke test is a
six-second script, and `scripts/screenshot/take.sh` drives a pre-made savefile
through a fixed key sequence.

The borg can. It is also, incidentally, a fuzzer with a plan: it will find the
crash that happens when a Monk with three mutations reads a scroll of Deep
Descent in a dungeon that bottoms out at fifteen, which no hand-written test
will think to try.

The case is strengthened, not weakened, by how little work it needs to start
running. See [§2](#2-what-the-review-found).

---

## 2. What the review found

### `src/borg/` is upstream's, untouched

59 `.c` and 58 `.h` files, 69,632 lines. Angband 4.2.x's borg, verbatim. 279
commits have touched it and **none of them are ours** — the last is upstream's
`f48a3bb68` (11 August 2026), four days before `a8c2a5e75` started this project.
[docs/hacking/borg.rst](../../docs/hacking/borg.rst) is likewise upstream's
text, still describing Angband, Morgoth and `angband.scr`.

So the borg was not half-converted and abandoned. It was left alone while 250
commits changed the game underneath it. There is nothing to unpick.

It is also still built: `SUPPORT_BORG=ON` in the cmake cache, `-DALLOW_BORG` in
[Makefile.osx](../../src/Makefile.osx), `Makefile.std`, `Makefile.nmake` and
`Makefile.3ds`. It compiles warning-free against the current headers.

### It crashes on the first turn of every game

A fresh Human Warrior, borg activated:

```
* thread #1, stop reason = EXC_BAD_ACCESS (code=1, address=0x21e)
  * frame #0: borg_update_map + 348
    frame #1: borg_update + 10412
    frame #2: borg_think + 2292
    frame #3: internal_borg_inkey + 2184
    frame #4: borg_inkey_hack + 40
    frame #5: inkey_ex + 432
```

The faulting line is `ag->info |= BORG_OKAY` in
[borg-update.c](../../src/borg/borg-update.c), where `ag = &borg_grids[y][x]`.
`borg_grids` is a static array of `AUTO_MAX_Y` row pointers, and
[borg-cave.h:33](../../src/borg/borg-cave.h#L33) fixes that at Angband's
dungeon: **66 rows by 198 columns**. Our depth-0 level is the wilderness
surface, which `wild_surface()` builds square at `view × block_size` —
`wild:cache-blocks:81` gives `view = 9`, and `wild:block-size:16`, so
**144 × 144**.

`borg_update_map` scans the panel, checks only `square_in_bounds(cave, l)`, and
then indexes `borg_grids[y][x]` with a `y` that reaches 143. It walks off the
end of a 66-element pointer array and dereferences whatever it finds.

**The guard that should have caught this asks the wrong question.**
[borg-cave.c:32](../../src/borg/borg-cave.c#L32) compares its constants against
`z_info->dungeon_wid/hgt` — still 198 and 66, so the check passes. The borg
never learns that the level it is standing on is bigger than the only level it
was built for.

### With the array grown, it plays

Rebuilt with `DUNGEON_HGT` at 160 and the equality check relaxed to `<`, the
same Warrior produced roughly 1,400 log lines of real play in one short run:
standing on stairs, flowing toward down-stairs, opening doors, noticing dropped
objects, tracking monsters, attacking with its dagger, fleeing a *"scary guy"*,
stair-scumming, then heading back to town to restock food. Wilderness terrain,
dungeon mouths and level feelings all went past it without complaint.

**This is the finding that shapes the plan.** The borg is not four months behind
in its thinking. It is one static array away from running, and B0 is therefore
small enough to be worth doing on its own.

### Thirteen of fourteen classes fail at startup

[borg-magic.c](../../src/borg/borg-magic.c) holds eight hand-written ratings
tables — 163 entries between them, one per vanilla caster.
`borg_prepare_book_info()` picks a table from `player->class->cidx`, and
`borg_init_spell()` then walks the class's spell list and matches it against
that table **positionally, comparing names**:

```c
if (strcmp(cspell->name, borg_spell_ratings[spell_num].name)) {
    borg_note(format("**STARTUP FAILURE** spell definition mismatch. "
                     "<%s> not the same as <%s>",
        cspell->name, borg_spell_ratings[spell_num].name));
    borg_init_failure = true;
```

M9 replaced all of it. Forcing each class through `borg_init` in turn, every one
of the eight fails on its first spell:

| Class | Game has | Borg expects | Spells now |
|---|---|---|---:|
| Mage | `Zap` | `Magic Missile` | 224 |
| Priest | `Detect Evil` | `Call Light` | — |
| Druid | `Detect Creatures` | `Detect Life` | 32 |
| Necromancer | `Detect Unlife` | `Nether Bolt` | 32 |
| Paladin | `Detect Evil` | `Bless` | — |
| Rogue | `Zap` | `Detect Monsters` | — |
| Ranger | `Detect Creatures` | `Remove Hunger` | — |
| Blackguard | `Detect Unlife` | `Seek Battle` | — |

The loop does not stop on failure, so it also reads
`borg_spell_ratings[30…223]` off the end of a 30-element array.

**The five classes we added are the worse case, because they fail silently.**
`borg_prepare_book_info()`'s `switch` has no arm for them, so `default:` sets
`borg_spell_ratings = NULL` and returns *before* `borg_magics` is allocated. No
warning, no `borg_init_failure`:

| Class | `cidx` | Spells | Result |
|---|---:|---:|---|
| Monk | 9 | 96 | silent NULL |
| Mindcrafter | 10 | 0 | silent NULL |
| Chaos-Warrior | 11 | 32 | silent NULL |
| Warrior-Mage | 12 | 224 | silent NULL |
| High-Mage | 13 | 224 | silent NULL |

`MAX_CLASSES` is 9 and `MAX_RACES` is 11 in
[borg-trait.h:57](../../src/borg/borg-trait.h#L57); we have 14 classes and 20
races. Random respawn uses `randint0(MAX_CLASSES)`, so it can never roll a Monk
or an Amberite, and `borg_init` range-checks `borg_respawn_race` and
`borg_respawn_class` against the old bounds.

### What it has no concept of at all

Reference counts across the whole of `src/borg/`:

| System | Refs | Consequence |
|---|---:|---|
| Pets and allegiance (PLR-22…PLR-33) | 0 | Targets its own pets, fears them, counts them as danger |
| Wilderness (WLD-01…WLD-09) | 0 | No overworld model; depth 0 is simply "the town" to it |
| Named dungeons (WLD-14) | 0 | Flat 1–100 depth ladder; cannot know a dungeon has a floor |
| Mutations (PLR-13…PLR-17) | 0 | Cannot avoid or exploit them |
| Virtues (PLR-18…PLR-21) | 0 | Invisible |

`MON_ALLEGIANCE_HOSTILE`/`FRIENDLY`/`PET` does not exist for the borg. The
thirteen named dungeons each have a depth range — 1–15 for the Vaults of Amber,
90–127 for the Abyss — and a bottom you must leave to get past;
`borg_prepared_aux` is a single ladder of `if (depth <= N)` clauses that knows
nothing of this.

Terrain has gone from about twenty hard-coded `FEAT_` constants in the borg to
42 types in `terrain.txt`. The eleven new wilderness features are merely
unfamiliar — the borg reads passability from flags, which is why it walked at
all. The six new buildings (magetower, healer, inn, magesmith, recharger, chaos
tower) do **not** carry `TF_SHOP`, so the borg walks past every service
WLD-16c added. The eight real stores still match `BORG_HOME == 7`, so nothing
overflows there.

### There is no way to drive it from a script

The only entry point is `^z` then `z`, through the UI. Measuring any of the
above needed [main-test.c](../../src/main-test.c) patched to call
`do_cmd_borg()` programmatically. Driving it by injecting keys through `-mtest`
is **not reproducible**: the borg sometimes activates before
`character_dungeon` is set and aborts with *"reincarnation failure"*, and the
birth menus consume a number of keys that depends on the roll.

Three further harness gaps:

- `borg.txt` (1,129 lines) must be hand-copied to
  `~/.angband/ZangbandZK/borg.txt`, or the borg warns and runs on built-in
  defaults. No build installs it.
- `borg_oops()` merely stops. Nothing exits non-zero, nothing is
  machine-readable, and from outside a crash looks the same as a clean
  retirement.
- Nothing seeds the RNG for a borg run. `ZTK_TEST_SEED` exists and the unit
  suites use it; there is no borg path through it.

---

## 3. Sequencing principles

1. **B0 stands alone.** It turns a segfault into a smoke test and delivers CI
   value before anything else is decided. If the rest of this plan is never
   scheduled, B0 was still worth doing.
2. **B1 is the gate.** Until the ratings problem is solved the borg is a
   Warrior-only harness, which tests the game one class out of fourteen deep.
   Everything about coverage depends on it.
3. **Prefer data to C.** See the standing constraint below. Every phase should
   ask whether the change can live in `lib/gamedata` instead of
   `src/borg/`.
4. **Robustness first, behaviour second.** A borg that plays badly but never
   crashes is already a useful test. A borg that plays well is a later
   luxury.
5. **Each phase ends with something CI can run.** No phase is done because the
   code compiles.
6. **DEC-17 applies, and the document it applies to is upstream's.**
   [docs/hacking/borg.rst](../../docs/hacking/borg.rst) still describes Angband,
   Morgoth and `angband.scr`. It is not a manual chapter — the borg is a
   developer tool, so this is the hacking guide rather than the player manual —
   but the rule is the same: no phase is done until that file describes what the
   phase changed. Tracked as BRG-21, standing rather than scheduled.

### Standing constraints

- **DEC-11 applies here in reverse.** DEC-11 gave up merge compatibility and
  kept cherry-pick compatibility. `src/borg/` is the one subsystem where we have
  contributed nothing and upstream is still contributing — 279 commits, the most
  recent a month before this plan. Every line changed there is permanent fork
  cost against an active upstream. **Keep C changes narrow, mark them, and reach
  for a data file first.** A `borg-rating:` field in `class.txt` conflicts with
  nothing; a rewritten `borg_spell_ratings_MAGE[]` conflicts forever.
- **BAL-08 does not apply, and that is a trap.** Nothing here imports a
  Zangband number, so the "check the consuming formula" rule is silent. It is
  replaced by a weaker one: **no borg constant may be duplicated from game
  data.** `MAX_CLASSES`, `MAX_RACES`, `DUNGEON_HGT` and `BORG_HOME` are all
  copies of things the game already knows, and three of the four are now wrong.
- **The borg is not a correctness oracle.** It does not know the rules, so it
  cannot tell us a rule is wrong. It can tell us the game crashed, asserted,
  corrupted a savefile or wedged. Assertions written *for* it (BRG-19) are what
  turn it into a correctness tool; the borg itself only supplies situations.
- **PLR-22's standing constraint is now discharged.** The Phase 2 plan asked
  that nothing before M10 deepen the assumption that every monster is hostile.
  The borg is the largest remaining body of code that makes exactly that
  assumption, and BRG-15 is where it is paid off.

---

## 4. Phases

### B0 — Make it run, and make it drivable

*Small. The whole of the immediate win, and independent of everything else in
this document.*

- **BRG-01 — The borg's map is sized from the level it is on.** Allocate
  `borg_grids` from the actual level dimensions and reallocate on level change.
  The same treatment for the other arrays keyed on `AUTO_MAX_*`:
  `borg_fear_region`, `borg_fear_monsters`, and `borg_flow`'s `data[][]`
  ([borg-danger.h:67](../../src/borg/borg-danger.h#L67),
  [borg-flow.h:59](../../src/borg/borg-flow.h#L59)).
- **BRG-02 — The dimension guard compares against the largest level the game
  can generate.** `borg_init_cave`'s check against `z_info->dungeon_wid/hgt`
  passes while the borg is about to read off the end of its own array. Either
  compare against the wilderness surface span as well, or — better — delete the
  check, since BRG-01 makes it meaningless.
- **BRG-03 — A headless entry point activates the borg without keypress
  injection.** Either a `-mborg` front end or `main-test` commands
  (`borg-start`, `borg-run N`, `borg-status?`). It must wait until
  `character_dungeon` is true before activating, so the *"reincarnation
  failure"* abort cannot happen.
- **BRG-04 — A borg run is reproducible from a seed.** `ZTK_TEST_SEED` reaches
  the borg path, and the same seed gives the same run. Without this a failing
  run cannot be re-run.
- **BRG-05 — A borg run reports failure through its exit status.** Non-zero on
  `borg_init_failure`, on `borg_oops`, and on a signal. A machine-readable
  summary line — turns taken, depth reached, character level, reason for
  stopping — on stdout.
- **BRG-06 — `borg.txt` is installed rather than hand-copied.** The delivered
  file goes into the user directory on first run, instead of the stripped
  default `borg_init_txt_file()` writes today.

**Exit:** `borg-run` walks a Warrior for N turns from a given seed and returns
non-zero if it crashed or aborted, with a summary line either way. That goes
into CI as it stands.

**Landed 3 September 2026 in 3.81.0**, as `scripts/borg-smoke`. What actually
happened, since almost none of it was the work this phase expected:

- **BRG-01/02 done, and the guard is the better half.** The arrays are sized by
  a checked ceiling rather than by the dungeon, and `borg_init_cave()` now asks
  the game for its largest level -- `MAX(z_info->dungeon_*, wild_view_blocks()
  * wild_block_size)` -- and refuses to start if it does not fit. Growing
  `wild:cache-blocks` to 225 produces a clean startup failure instead of the
  segfault, which is what makes the ceiling an upper bound and not a copy of
  game data.
- **Two build defects blocked the phase outright**, and both were single-target
  option leaks. `ALLOW_BORG` was set on `OurExecutable` only inside
  `if(SUPPORT_WINDOWS_FRONTEND)`, so on macOS and Linux no front end could
  name the borg. `USE_TEST` was set only on `OurExecutable`, so a
  `#ifdef USE_TEST` in `OurCoreLib` -- where the game loop lives -- compiled to
  nothing. Both failed *silently*: guarded code vanished and the files still
  compiled.
- **BRG-03 needed a headless birth, not just an entry point.** The four
  existing front-end tests all `quit` before the game starts, so
  `character_dungeon` had never been true in the test front end at all.
  `start_game()` gained a `ZTK_HEADLESS` branch using `player_make_simple()`
  -- the same public helper `main-spoil.c` uses -- and raising the level the
  way `main-stats.c` does.
- **`borg_cmd_start()` is no longer static**, because activation is a ritual
  and reimplementing half of it is how the first working run crashed:
  `borg_reinit_options()` allocates the arrays `borg_reset_ignore()` frees, so
  setting `borg_active` by hand crashed on the first *de*activation.
- **A headless run has nobody at the keyboard**, and that took three attempts.
  The borg stops on any real keypress; feeding ESCAPE aborted it at turn one,
  feeding nothing hung it because the terminal's event poll sits *below* the
  borg's own hook, and upstream's exemption for key code 10 is unreachable
  because `Term_keypress(10)` is translated to `KC_ENTER` first. Hence
  `borg_headless`.
- **BRG-05 grew a wedge detector**, which was not in the plan and should have
  been: a turn budget assumes the clock moves, and a borg waiting for a prompt
  it cannot see makes decisions forever. A hung CI job is a red build with no
  diagnosis, so `borg_step_limit` counts decisions and running out of them is a
  named failure.
- **`borg-notes?` was added** for the same reason. Every diagnosis in this
  phase came from reading the last dozen things the borg said.
- **BRG-06 is deferred, deliberately.** The borg writes its own `borg.txt`
  defaults when the user file is missing, so a run is not blocked by it -- the
  delivered file only makes the borg play *better*, which is behaviour, and
  principle 4 puts robustness first. Also: `lib/gamedata` is policed by
  `check-build-lists` for parsed data files, so the delivered copy wants a home
  chosen on purpose rather than in passing.

The borg reaches depth 1 in 300 turns as a Warrior and the same seed gives the
same run. `borg-roundtrip` (BRG-19, pulled forward) passes at every seed
tried.

---

### B1 — Classes, races and realms

*The largest phase, and the gate for everything about coverage. Most likely of
the five to be underestimated: the ratings pass is a judgement call about 200
spells, not a mechanical conversion.*

#### The rating scale, decided before the pass

Written first, on the project owner's instruction and because the plan's own
Open section asked for it: *"Before rating 200 spells, decide what the numbers
mean, or the pass will not be consistent with itself."* Upstream's tables use
5-95 with no documented meaning beyond "usefulness", and 163 entries rated
against an undefined scale is 163 opportunities to disagree with yourself.

**A rating answers one question: *how much does the borg want to spend a turn
casting this, compared with hitting something?*** Not how strong the spell is,
not how much a human would like it, and not how flavourful it is. The borg's
alternative to any spell is almost always another melee round, and the rating
is what makes it choose.

| Band | Meaning | Examples of the kind |
|---:|---|---|
| **90-95** | **Saves the character's life, or the run.** Escape, healing at low hit points, curing what will otherwise kill it. The borg should reach for these ahead of anything. | *Phase Door*, *Cure Critical Wounds*, teleport |
| **75-89** | **Its best answer to a fight it is in.** The attack spell it should lead with, or the one that ends a fight it would otherwise lose. | *Magic Missile* early, *Orb of Draining*, *Stinking Cloud* |
| **60-74** | **Information it will act on.** Detection and mapping, which change where it goes next. Rated below combat because knowing is worth less than surviving. | *Find Traps, Doors & Stairs*, *Detect Monsters* |
| **51-59** | **Useful and situational.** Worth casting when there is nothing better to do, and the borg only considers these when it is bored -- a genuine threshold in the code, not a figure of speech. | light, minor utility, resistances |
| **50** | **Unrated.** The fallback, and it means *nobody has judged this spell*. Never assign 50 by hand: it is the value that says the pass has not reached here yet. | -- |
| **1-49** | **Judged and not wanted.** The borg can cast it and should not. Slow, expensive or redundant against what it already has. | duplicated detection, flavour spells |
| **0** | **Never.** Actively harmful for an automaton: anything that needs a judgement it cannot make, or that could kill it. | self-damaging or irreversible effects |

**Rule zero: rate for survival first.** Where a rating is a judgement call
between hitting harder and staying alive, staying alive wins. This is not a
matter of taste, it is what the measurements say: the borg's failure mode is
**dying at character level 3 to 6**, three runs in six, not playing slowly. A
borg that reaches depth 30 alive is verification of M8 to M10; one that fights
beautifully and dies at 12 is not, however elegant the fight was. So escape,
healing and curing sit above attack spells at every level, and an attack spell
that leaves the borg exposed is rated below a worse one that does not.

Three further rules that keep the pass consistent with itself:

1. **Rate against the alternative, not in isolation.** A spell is worth 85 if
   the borg should cast it instead of attacking, and 40 if it should attack
   instead. Asking "is this a good spell?" produces a different and useless
   answer.
2. **Rate the spell, not the level it is found at.** The code already knows the
   spell's level and mana. A weak first-book attack spell is still the best
   answer a level-3 caster has, and the borg stops choosing it when something
   better becomes legal.
3. **50 is reserved.** It is the fallback that `borg_init_spell()` assigns to
   anything the pass has not reached. Rating a spell 50 by hand makes "not yet
   judged" and "judged as middling" indistinguishable, which is exactly the
   confusion the pass exists to remove. Use 51 or 49.

The threshold at 50 is real and worth knowing before rating: `borg_play_magic`
skips any spell rated 50 or less unless the borg is *bored*, so the difference
between 49 and 51 is the difference between "only when there is nothing to do"
and "considered in a fight".

- **BRG-07 — Spell ratings live in the game data, keyed by name and realm.**
  There are 200 distinct spell names across the seven realms, and 1,472
  `spell:` lines in [class.txt](../../lib/gamedata/class.txt) because realms
  repeat per class. Rated once by name-and-realm that is ~200 entries instead
  of a table per class, and a new realm then costs nothing. A `borg-rating:`
  field on `spell:` lines keeps the rating next to the spell it rates; a
  separate `borg_spell.txt` keeps it out of the class file. Prefer the former
  under principle 3.
- **BRG-08 — An unrated spell is usable, not a startup failure.** Derive a
  fallback rating from the spell's effect index. An unrated spell should be
  merely unloved.

  **The derivation landed 3 September 2026 in 3.86.0, and it did not help.**
  Sized from the data first: **224** distinct (realm, name) pairs -- seven
  realms of exactly 32, with 24 names shared between realms -- of which 49
  already had an upstream rating by name, leaving **175** needing a judgement.
  Rated by effect category against the rubric rather than one at a time,
  because 175 individual opinions would not stay consistent with each other,
  and because a category means a spell added tomorrow is rated the day it is
  added.

  Measured over three seeds at 40,000 turns:

  | class | best depth | best level | outcome |
  |---|---:|---:|---|
  | Mage | 1 | **1** | died, all three seeds |
  | Priest | 2 | **2** | died, all three seeds |
  | Warrior | 11 | 6 | one survived |

  **So the ratings are not the binding constraint for casters.** A character
  level 1 Mage has four hit points and two spell points; it dies before spell
  selection means anything. The pass was still worth doing -- it is BRG-08 done
  rather than deferred, and it is needed the moment a caster survives long
  enough to cast -- but the honest reading is that it moved nothing measurable,
  and early survival is what limits the nine caster classes.
- **BRG-09 — An unknown class degrades to melee, and never leaves
  `borg_magics` null.** The `default:` arm must warn and continue, not return
  early with an unallocated array. This is the defect that would have been
  found in M7 had the borg been running.
- **BRG-10 — Class and race counts are derived from the loaded data.**
  `MAX_CLASSES` and `MAX_RACES` come from `classes`/`races`, and the
  `borg_respawn_race`/`borg_respawn_class` range checks come with them.
- **BRG-11 — The borg reads the realms the character actually chose.** A Mage
  picks two of seven under PLR-09; the borg cannot assume a fixed book layout.

**Exit:** the borg starts, and plays for N turns without aborting, as all
fourteen classes and all twenty races. This is checkable exhaustively and
should be a test, not a claim.

**BRG-08 and BRG-09 landed 3 September 2026 in 3.82.0, ahead of BRG-07**, and
the reordering is the plan's own principle 4: nothing crashes first, plays well
second. Measured with `scripts/borg-smoke` rather than taken from this
document, and the measurement corrected it. §2 said thirteen of fourteen
classes fail, the vanilla eight loudly and the added five silently. What
actually happens:

| | before | after |
|---|---:|---:|
| play for 60 turns | **2** | **12** |
| crash the process | 10 | 0 |
| fail without crashing | 2 | 2 |

So it was **twelve** of fourteen failing, not thirteen, and **ten of those
killed the process** rather than reporting a startup failure -- reading
`borg_spell_ratings[30..223]` off the end of a thirty-entry array is fatal, not
merely wrong. The two that worked were the Warrior, which casts nothing, and
the Mindcrafter, whose psionics are a power list with no table to mismatch
(PLR-06).

What the fix was:

- **Ratings are looked up by name, within the table's actual length.** The
  positional match was the crash. `N_ELEMENTS` now travels with the pointer.
- **An unrated spell is merely unloved.** Described from the game's own data,
  rated 50, and given `BORG_SPELL_UNKNOWN` -- a new enum member, appended,
  because there was none and the first value is `MAGIC_MISSILE` at zero, so an
  unrated spell was previously *believed to be Magic Missile* by a `switch` in
  `borg-magic-play.c`.
- **The `default:` arm falls through instead of returning.** It returned before
  `borg_magics` was allocated and raised no failure flag, so the five added
  classes dereferenced NULL hundreds of turns later.
- **A spell with no effect chain no longer dereferences NULL.**
  `player/realm`'s `a-spell-without-an-effect-says-so` proves this game's data
  contains them; upstream dereferenced `cspell->effect` unconditionally.
- **A class with no spells allocates nothing**, which the Warrior and the
  Mindcrafter both need.

**Still open, and it is the last blocker for fourteen of fourteen: the
Necromancer and the Blackguard wedge in the study loop.** Both repeat
*"# Studying spell/prayer Detect Unlife."* forever without the game clock
advancing, so B0's step budget catches them and they fail with `reason=wedged`
rather than hanging. Both are the Death-realm pair and both choose their spells
rather than learning at random -- but so do the Mage, Rogue, Ranger and Druid,
which all play, so the choose/random split is not the cause. The study sends
`G`, a book letter and a spell letter; the most likely candidate is
`as->book_offset` from `borg_get_book_offset(cspell->sidx)` addressing the
wrong slot for a realm whose books M9 rebuilt, which is BRG-11's territory.
Not chased further, because BRG-08's value does not depend on it and a wedge
that reports itself is not an emergency.

---

### B2 — The world

*Medium. Second most likely to be underestimated — wilderness flow is a
pathfinding problem across 144 × 144 rather than a table.*

- **BRG-12 — The borg crosses the wilderness on purpose.** Treat depth 0 as
  ground to cross rather than a town to shop in: flow toward dungeon mouths,
  prefer `FEAT_ROAD`, and treat deep water, open sea and rock as the obstacles
  they are.
- **BRG-13a — `borg_prepared_aux`'s depth ladder is written for Angband's
  world, and produces inexplicable behaviour in ours.** Recorded separately
  from BRG-13 because it is a distinct defect with its own symptom, and it
  wants writing down even though the fix arrives with BRG-13.

  `borg-prepared.c` holds **thirty hardcoded depth comparisons** — `depth <=
  5`, `<= 9`, `<= 55`, `<= 80`, `<= 98`, `>= 82`, `>= 97`, `>= 99` and the rest
  — describing Angband's single 1-to-100 ladder with Morgoth at the bottom. Our
  world is thirteen dungeons with their own bands, from the Vaults of Amber at
  1-15 to the Abyss at 90-127, and **none of those numbers corresponds to
  anything here**. The rules they gate are real ones: what resistances to
  require, when to carry teleport, when the borg considers itself ready for the
  endgame.

  The symptom is not a crash, which is what makes it worth recording. It is a
  borg that demands endgame kit at a depth where this game has none of it, or
  dives without resistances the local dungeon needs, and a reader of the log
  sees only that it behaved oddly at depth. Nobody would connect that to a
  constant written for a different game's dungeon.

  Note also that our deepest dungeon reaches **127**, past every one of these
  bounds, so the ladder simply runs out.

#### BRG-13, measured: the first half is done and the second half is the whole job

**The floor half landed 3 September 2026 in 3.87.0.** `borg_prepared()` now
returns *"no deeper in this dungeon"* once the requested depth passes
`dun_type_by_index(player->dungeon - 1)->max_depth`. Reported as an
unpreparedness rather than handled directly, because the borg's existing answer
to that is to go up, and going up is right.

It matters because `dungeon_get_next_level()` **clamps** rather than refuses:
taking down stairs at the bottom of the Vaults of Amber returns the character
to depth 15. The borg would have read each clamp as a successful descent and
looped for ever -- no crash, no message. Measured as byte-identical to
baseline, which is correct: the borg has never reached depth 15, so this is a
fix for a wall it has not hit yet. The Warrior at depth 11 is four levels away.

**And the second half is not a refinement.** Measured with `borg-mouths?`,
which reports every mouth's world grid against the current surface window:

| | |
|---|---:|
| dungeon mouths in the world | 13 |
| **inside the current 144x144 window** | **0** |
| nearest mouth reaching past depth 15 | **576 grids** (The Grove of the Unicorn, band 30-55) |
| nearest mouth whose band contains 30 cheaply | 2,208 grids (Faiella-Bionin, band 8-30) |

Not one mouth is in the window -- **including the Vaults of Amber's own**, whose
mouth sits 472 grids north and 200 west. The borg has never used a mouth at
all: it reaches the Vaults by the town staircase, which
`player_dungeon_at_stairs()` sends to the shallowest dungeon there is. The
world spans roughly fourteen windows by fourteen.

So in-window walking buys **nothing**, and there is no route to depth 30 that
avoids crossing the world: the town staircase always gives the shallowest
dungeon, and Word of Recall returns to a dungeon already visited.

**What that job actually is, and it may be smaller than "a pathfinder".** The
surface window follows the player -- `wild_adopt_window()` re-anchors it as the
world scrolls -- so crossing 576 grids does not need a global route computed in
advance. It needs a **world-space goal that survives the window being rebuilt**
plus local obstacle avoidance: walk toward a bearing, step around what is in
the way, and keep the destination in world coordinates rather than level ones.
The open risk is terrain: open sea and mountain between here and there would
turn a bearing into a route, and that is the point at which it becomes a
pathfinding project.

- **BRG-13 — The borg knows a dungeon has a floor, and where the next one
  is.** Read the `dun_type` depth ranges. When a dungeon bottoms out, walk to a
  deeper one rather than scumming for stairs that do not exist. `borg_prepared`
  needs to become a function of (dungeon, depth), not depth.
- **BRG-14 — The borg uses the six wilderness services.** The healer and the
  inn first, since they change what "recover" means; then magesmith, recharger,
  magetower and chaos tower.

**Exit (revised 3 September 2026 by the project owner): the borg reaches depth
30.** *"Yer that guy needs to go down to level 30 at least."* This replaces
"crosses to a named dungeon", on the ground that reaching a dungeon is worth
little if it then sits at the top of it.

**Depth 30 forces BRG-13 rather than merely benefiting from it.** The town
staircase leads into the Vaults of Amber, which **bottoms out at 15**
(`dungeon.txt`). The bands are 1-15, 1-20, 8-30, 15-40, 20-45, 25-50, 30-55,
35-65, 40-75, 55-90, 60-95, 75-110 and 90-127, so 30 is reachable only by
crossing to Faiella-Bionin or deeper. There is no route to the target inside
the starting dungeon.

**And 30 is a well-chosen bar for the purpose.** Content here is gated by
character level, and the borg's own rule (`MAXCLEVEL < depth` refuses the
descent) makes depth a proxy for it. Of the 1,472 `spell:` entries across the
seven realms, **64 per cent are reachable by character level 30**; the rest sit
at 31-50. Trump's summons run from 24 to 49, so 30 exercises the early ones and
not the great undead.

#### Follow-up: raising the bar to 50

**Recorded rather than carried.** The project owner, 3 September 2026: *"Let's
stay with level 30 until we get it working, we can expand to 50 later, make a
note for follow up."* So 30 is the target and 50 is a later question, not an
ambition to be worked toward in the meantime.

What raising it to 50 would buy, so whoever picks it up does not have to
re-measure:

| | reachable by clevel 30 | needs 31-50 |
|---|---:|---:|
| `spell:` entries across the seven realms (1,472 total) | **64%** (947) | **36%** (525) |
| Trump summons (24-49) | the early ones | the great undead at 49 |

And the dungeon bands say where it would have to go: 30 is reachable in
Faiella-Bionin (8-30) at its floor, or Garnath (15-40), Kolvir (20-45), Rebma
(25-50) or the Grove (30-55). **50** needs Rebma's floor or deeper --
Tir-na Nog'th (35-65), A Broken Pattern (40-75) -- which is two dungeon
transitions further out than the target, not one.

**Scoping consequence, and it is the useful half of this note: anything only
reachable above character level 30 is out of scope.** Neither the ratings pass
nor the depth work should grow to accommodate it. A spell or a mechanism that
only matters above 30 gets rated by its category and left alone.

**Part-landed 3 September 2026 in 3.83.0. The exit criterion is NOT met, and
the phase stopped at its agreed boundary.** What was found is more useful than
what was built:

**The borg does not reach depth 2, let alone a second dungeon.** Measured over
3000 turns for each of the twelve playable classes, before and after:

| | before | after |
|---|---:|---:|
| best depth reached, any class | 2 | 2 |
| best character level, any class | 2 | 2 |
| classes reaching a second dungeon | 0 | 0 |

So none of the wilderness work in BRG-12/13/14 can be demonstrated yet, because
the borg never gets far enough to need it. That is worth knowing before writing
a wilderness pathfinder: **the blocker is not navigation.**

**What was fixed, and it is real.** The four files that read `DUNGEON_WID/HGT`
as the *level's* extent (flagged in B0 as behaviour and therefore B2's):

- `borg_happy_grid_bold()` rejected every grid at `y >= DUNGEON_HGT - 2`. A
  character starts the surface at about **row 81 of a 144-row level**, so this
  returned false for every grid the player ever stood on -- the borg believed
  the entire surface was the outside wall of the world. After the fix it
  registers the town's shops, which sit at rows 83 to 87 and were previously
  *outside its universe*: `borg-shops?` reports 3 of 8 known where it reported
  none.
- The two panel clamps in `borg-flow-kill.c`, the two map-dump loops in
  `borg-log.c`, and the outside-wall check in `borg-fight-attack.c`, all now
  read `cave->height`/`cave->width`.

**And an unreachable branch made reachable.** `borg_think_dungeon()` visits
shops *after* `borg_leave_level()`, so the shop visit is dead code whenever
leaving succeeds -- which on this game's surface is always, because the borg
arrives from the dungeon standing on the town staircase. Angband does not show
this: there the borg's first town visit happens before it has ever descended,
so it shops on the way *in*. Arriving from below with shopping still to do is a
shape this game creates and Angband does not.

**The blocker, precisely, with the evidence.** The borg loops: come up for
food, descend without buying any, note *"# heading up (bored and unable to
dive: restock food < 3)"*, come up again. Two upstream thresholds disagree --
`borg_restock()` refuses to dive below **three** rations and
`borg_choose_shop()` heads for the General Store only at **zero** -- so between
one and two rations it will neither dive nor shop. Aligning the two thresholds
was tried and **did not fix it**, so that is not the whole cause and the change
was reverted rather than kept on a hunch. What remains is most likely a
bootstrap: `borg_shops[i].ware[]` is filled by *visiting* a shop, and
`borg_think_shop_buy_useful()` cannot want what it has never seen, so nothing
sends the borg inside in the first place.

**What B2 needs next, in order:** the shop bootstrap above; then BRG-12's
surface flow; then BRG-13. None of it is navigation until the borg can restock.

**A harness correction that matters more than any of it: a dead character is
not a failure.** The borg calls `borg_oops("death")` down the same channel it
uses for defects, and three of the twelve classes die within 3000 turns at
character level 1. Left as it was, B4's nightly job would have been permanently
red and everyone would have learned to ignore it. Death now reports
`result=died` with a zero exit, and `maxdepth`/`clevel` on the summary line are
what make a build where everything dies at depth 1 visible.

---

### B3 — Allegiance and the newer player systems

*Medium. Cannot start before M10 is playable, and should not start much
after — see principle 4 of the Phase 2 plan.*

- **BRG-15 — The borg never attacks its own pets, and counts them as help.**
  Read `MON_ALLEGIANCE_*`: exclude pets from the kill list and from
  `borg_danger`, and count them on the player's side of a fight. This is the
  one requirement here that produces visibly wrong behaviour the moment M10
  ships, rather than merely missed opportunity.
- **BRG-11a — The "abandon this level" trigger identifies danger by Angband
  monster name, and it traps every caster at character level 1.** Scoped
  3 September 2026 after five attempts at the symptom; recorded rather than
  fixed, because the fix is a rewrite and the fifth attempt made things worse.

  **What is wrong, and it is structural rather than a constant.**
  `scaryguy_on_level` -- the flag that makes the borg give up on a level -- is
  set in `borg-flow-kill.c` by matching **monster name prefixes**. Upstream
  says so itself: *"!FIX this should be rewritten to not use specific names
  but instead track certain attacks that are particularly scary"*. The
  character level 8 list is `soldier`, `cutpurse`, `acolyte`, `apprentice`,
  `kobold`, `jackal`, `shrieker`, `filthy street urchin`, `battle-scarred
  veteran`, `mean-looking mercenary`; the level 20 list adds `cave spider`.
  It is also deliberately level-wide rather than proximity-based: *"scary guys
  on level, not scary guys near me"*.

  Both choices are right for Angband. Together, here, they are a trap: those
  are the starting town's monsters and the Vaults of Amber's signature
  dwellers (`dweller:base:kobold`, `dweller:base:spider`). A level 1 character
  flees on its first turn and never stops.

  **It is a feedback loop, which is why it looks like a caster problem.**
  Fleeing prevents fighting, not fighting prevents levelling, and not
  levelling keeps the character under the threshold. A Warrior escapes because
  melee works without spells and it out-levels the list; a Mage cannot, and
  measurably does not -- character level 1, dead, in every seed tried.

  **What it is not.** Both alternative explanations were checked and both are
  false. Our depth 1-2 monsters are Angband's to within noise (mean 11.7 hit
  points against 10.7, mean damage 2.6 against 2.5, worst case 8.0 in both,
  measured against the tree as it stood before this project began), and the
  hit dice are 4.2's exactly -- Warrior 9, Mage 0, Priest 2. The content did
  not move; the model is being applied to the same monsters it was written
  for, with a bestiary it does not recognise.

  **Five attempts on the symptom, and what they taught.** The ratings pass, the
  by-effect casting, the book-array overrun and study-while-fleeing all left
  the Mage's runs *byte-identical* -- 10044/3154/6608 turns, character level 1
  -- which is what finally proved it never reaches any of that code. The fifth,
  making the shallow rules require an awake monster within 30 grids, moved it
  the wrong way: the Mage stopped fleeing, started fighting, and died **sooner**
  (2662/2136 turns). That is worth keeping in mind before the next attempt: at
  four hit points with no spells, fleeing was the only thing keeping it alive,
  so removing the flee without giving it a way to fight is a regression.

  **What a fix has to do**, therefore, and it is more than a threshold: give a
  level 1 caster a way to survive *and* a way to level. Reading threat from the
  monster's own attacks rather than its name is the piece upstream asks for and
  the piece this game needs, but on its own it only changes which monsters
  frighten it.

  **The fifth attempt was reverted, and its Warrior number is the reason to
  record it rather than retry it.** Requiring an awake monster within 30 grids
  is defensible in itself -- a sleeping kobold across a 198-grid level is not a
  level to abandon -- and it measured badly in both directions. The Mage died
  sooner (2662 against 10044 turns). The Warrior *survived longer and got far
  shallower*: 40,000 turns at **depth 2** against 26,473 turns reaching **depth
  11**. Fleeing less means fighting more, and fighting more at low level means
  both dying sooner and diving less. Anything that adjusts when the borg runs
  has to be judged on depth as well as survival, or it will look like an
  improvement while making the borg worse at the only thing being asked of it.

- **BRG-16a — The borg wields unknown items straight off the floor, and a
  careful player would not.** Recorded 3 September 2026, not scheduled.

  It wore a `Club (2d2) (+0,+0) [+20] {cursed, ??}` because twenty points of
  armour scores well, learned the rune of ancient and foul curse, and had its
  experience drained back to character level one. A human sees `{??}` -- unknown
  runes -- on a depth-1 club with +20 armour and smells a trap; the project
  owner's summary is *"if it looks too good to be true it probably is... maybe
  get them checked first"*.

  The reason to record it is not the curse, which is uncommon and working as
  designed. It is that **a borg which walks into every trap a careful player
  avoids keeps producing findings that are really just it playing badly.** One
  such finding had to be discounted on exactly those grounds while chasing the
  caster deaths this session, and discounting costs as much time as
  investigating. Wanted: prefer identified items, or use a Scroll of
  Identify/Remove Curse before wearing something suspiciously good for the
  depth.

- **BRG-16 — Mutations, virtues and luck are borg traits.** Add them to
  `borg.trait[]` so the borg can at least avoid the mutations that hurt it.
  Commanding pets (PLR-23…PLR-27) is deliberately **not** in scope: BRG-15 is
  about not making things worse.

**Exit:** a borg with pets does not attack them, and the mutation set is
visible in `^z 0`.

**BRG-15 implemented 3 September 2026 in 3.84.0, and NOT demonstrated.** Both
halves are in and the reason it cannot be shown yet is itself the finding.

**What was done.** `src/borg/` contained **zero** references to allegiance, so
a pet was simply a monster: something to target, to fear and to cross a level
to kill. The seam is the kill list, because `borg_kills[]` is not merely a
target list -- `borg_danger()` is computed over it and `borg_flow_kill()` walks
toward its entries -- so one check covers targeting, fear and pursuit together:

- `borg_new_kill()` refuses to create an entry for a creature that is not
  hostile. Every path that observes a monster reaches this one function, which
  is why the check is there rather than at each of its four call sites.
- `borg_update_kill_old()` deletes an entry whose monster has changed sides,
  which is the case a wand of Tame Monster creates. The reverse needs nothing:
  an angered pet (PLR-33) is a monster the borg has no entry for, and the next
  observation makes one.

**Why it is not demonstrated, and this is the part worth reading.** An
instrument was built for it -- `borg-kills?`, reporting the tracked count and,
separately, how many tracked entries point at a creature that is *not* hostile,
which must be zero. It reads the list with the borg's own idiom
(`for (i = 1; i < borg_kills_nxt; i++)`).

It reports `tracked=0` on every run, with the fix and without it, after sixty
turns on a level holding twenty-four hostile monsters. The borg has no target
list because **it never fights anything**: it descends, stands on the stairs,
and leaves, and `borg_kills_nxt` is reset to 1 at every level change. Three
measurement attempts failed the same way -- short bursts observe nothing, long
ones change level and lose the pets to the carry.

So **B3's verification is gated on B2's blocker**, though its implementation is
not. Until the borg restocks and stays on a level long enough to fight, there
is nothing to observe. The fix is landed anyway rather than held: it is a
two-line guard whose only failure mode would be the borg *ignoring* a monster,
which the condition makes impossible, and leaving master with a borg that
attacks the player's pets is the worse of the two states.

BRG-16 (mutations, virtues and luck as borg traits) is not started.

---

### B4 — The testing product

*Medium, and where the value compounds. Everything before this makes the borg
work; this is what makes it a test.*

- **BRG-17 — A borg suite runs N seeds across M classes and asserts no
  crash.** No segfault, no assertion, no `borg_oops` that is not a clean
  retirement. Modelled on [scripts/check-flakes](../../scripts/check-flakes),
  which already understands that one green run does not mean deterministic.
- **BRG-18 — The suite records depth and character level as a regression
  signal.** A build where every class suddenly stops getting past depth 3 has
  broken something no assertion catches.

  **Landed 3 September 2026 as `scripts/borg-progress`, ahead of the rest of
  B4.** Pulled forward because the same sweep had been hand-rolled four times
  while chasing the depth stall -- each time slightly differently, which makes
  runs incomparable -- and because best-depth-per-class is the number the
  project owner is now asking for. Deaths are reported rather than failed; the
  exit status counts only crashes, aborts and wedges.
- **BRG-19 — Invariants are checked during play, not only at the end.**
  Round-trip the savefile every M turns; assert the player is never on an
  impassable grid; assert pet count and mana upkeep agree (PLR-30); assert the
  wilderness block cache never exceeds `wild:cache-blocks`. This is the
  requirement that turns a crash harness into a correctness one.
- **BRG-20 — The suite runs nightly in CI.** Not per-PR: these runs are
  minutes, not milliseconds. A separate workflow beside
  [linux.yaml](../../.github/workflows/linux.yaml).

**Exit:** a nightly job that fails on a crash, and a recorded depth-and-level
table that a human can compare between releases.

---

## 5. Requirement coverage

Twenty-one requirements, each appearing exactly once. The arithmetic is
checkable: the phase rows sum to 20, and the one that is not scheduled is listed
under them with a reason.

| Phase | Requirements | Count |
|---|---|---:|
| B0 | BRG-01…BRG-06 | 6 |
| B1 | BRG-07…BRG-11 | 5 |
| B2 | BRG-12…BRG-14 | 3 |
| B3 | BRG-15, BRG-16 | 2 |
| B4 | BRG-17…BRG-20 | 4 |
| | **Scheduled** | **20** |

And the one that is not:

| Requirement | Why it is not in a phase |
|---|---|
| BRG-21 | **The hacking guide tracks each phase (DEC-17).** A standing rule, not a deliverable: `docs/hacking/borg.rst` is upstream's text and describes Angband. It applies to B0, B1, B2, B3 and B4 alike, and counting it once would put it in the wrong place four times. |

**21 = 20 scheduled + 1 standing.**

### What can run in parallel

B0 is strictly first; nothing can be tested until the borg runs. After it, **B1
and B2 have no hard dependency on each other** — the wilderness work is
navigation and the class work is spell tables, and they touch different files.
B3 depends on M10 being playable rather than on B1 or B2. B4 is worth starting
as soon as B0 lands, in the minimal form of "run a Warrior nightly", and grown
as later phases widen what can be run.

### Sizing

Deliberately absent in days, following the Phase 2 plan's convention. In
relative terms B1 is the largest and B0 the smallest by a wide margin. The two
most likely to be underestimated are B1's ratings pass — 200 judgement calls,
and the borg plays badly if they are careless — and B2's wilderness flow.

---

## 6. How the findings were measured

So they can be re-checked. All patches were reverted; the tree is as it was.

- `cmake --build build --target OurExecutable` — the borg compiles clean.
- [main-test.c](../../src/main-test.c) patched with a `borg` command calling
  `do_cmd_borg()` after pushing a command letter, plus a
  `borg-class <name>` command swapping `player->class` to force each class
  through `borg_init`.
- [borg-io.c](../../src/borg/borg-io.c) patched to mirror every `borg_note()`
  to stderr under `ZTK_BORG_TRACE`.
- `lldb -b` for the backtrace.
- `borg-cave.h`'s `DUNGEON_HGT` raised to 160 and `borg-cave.c`'s equality
  check relaxed to `<`, to see how far the borg gets once the array fits.
- The 144 × 144 figure derived from `wild_view_blocks()` against
  `wild:cache-blocks:81` and `wild:block-size:16` in
  [constants.txt](../../lib/gamedata/constants.txt), and confirmed by the
  crash address.
- `borg.txt` copied to `~/.angband/ZangbandZK/borg.txt`. **That file is still
  there** — it is the copy step
  [docs/hacking/borg.rst](../../docs/hacking/borg.rst) documents, and the borg
  needs it until BRG-06.

---

## Open

- **BRG-07's rating scale is not defined.** The upstream tables use 5–95 with
  no documented meaning beyond "usefulness". Before rating 200 spells, decide
  what the numbers mean, or the pass will not be consistent with itself.
- **Whether the borg should be scored.** `SCORE_BORGS` is off. It should
  probably stay off, but a borg that reaches depth 40 is a more interesting
  regression signal than one that reaches depth 4, and the scores file is one
  place to record that. BRG-18 currently proposes its own recording instead.
- **The Tcl/Tk front end (Phase 3) will need the borg to work through it, or
  not at all.** The borg steals `inkey_hack`, which is a `main-*.c`-independent
  hook, so it should carry over — but nobody has checked, and it is cheaper to
  check while Phase 3 is being designed than after.
- ~~**Mindcrafter reports 0 spells.**~~ **Resolved 3 September 2026, and it was
  the benign reading.** PLR-06's psionics are on the `power:` path: the class
  has twelve `power:` entries, no `magic:` line, no books and no realms, which
  is what makes it not a realm. `game/wild`'s
  `the-mindcrafter-thinks-for-itself` already asserted exactly that. So the
  borg's observation was accurate and the worry was not — nothing to fix.

  It was still worth chasing, because next door to it there was a real gap: the
  **Monk** casts twelve books of ninety-six spells and its spell count was
  pinned nowhere. `player/realm`'s counts table names classes one at a time and
  the Monk had no row, so two milestones of Monk casting went unasserted. Fixed
  with the row *and* with
  `every-class-agrees-with-its-own-books`, which asks `classes` itself rather
  than a list of names: books and spells present together, the total equal to
  the sum of the books, every book one-to-eight spells of a named realm, and
  every choosable realm backed by books. A class added tomorrow is covered on
  the day it is added.

  The general lesson is the one the borg plan is arguing: a hand-maintained
  table only covers what somebody remembered to add.

### BRG-24 — the borg had no legal move (done, 3.100.0)

Three faults, all presenting as "the borg prefers shallow":

1. `borg_prepared()` demands a Word of Recall from depth 5; `borg_restock()`
   has the matching rule commented out. Forbidden to descend, never sent home.
   Fixed: restock demands the same scroll, and `borg_must_return_to_town()`
   asks about `cdepth + 1` so band boundaries (phase at 6, teleport at 10)
   cannot strand it either.
2. `unique_depths[]` initialised to zero, so its "keep the three shallowest"
   comparison never fires and `borg_depth_hunted_unique` is zero all game.
   Fixed: sentinel 127.
3. The catch-all yes to "Set recall depth to current depth?", which sets
   `max_depth` to the current depth. Fixed: refuse while the borg is shallower
   than its best and still prepared for that best.

Covered by `src/tests/borg/prepared.c` (the deadlock invariant, falsified two
ways) and by the harness's `borg-maxdepth-drop` watch (the other two need a
live game).

### BRG-25 — the borg cannot shop outside the town it starts in (open)

The borg's ceiling is now `restock tele + tele staff < 2` at depth 10. The
Staff of Teleportation is a magic shop item and the starting village has no
magic shop by design (WLD-11a). Bigger towns do, and `wild_town_stores()`
scores the magic shop in from the "town" band upwards.

Before the wilderness there was one town with everything, so nothing in the
borg models "the thing I need is sold somewhere else". `borg_flow_world()`
already crosses the surface to dungeon mouths; the work is to give it towns as
targets and to make the restock reason say which town can satisfy it.

This blocks depth 30: the depth 10 rung cannot be climbed in the village, and
the rungs below 30 want six teleport sources.
