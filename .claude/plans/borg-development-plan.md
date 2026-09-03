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

---

### B1 — Classes, races and realms

*The largest phase, and the gate for everything about coverage. Most likely of
the five to be underestimated: the ratings pass is a judgement call about 200
spells, not a mechanical conversion.*

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

---

### B2 — The world

*Medium. Second most likely to be underestimated — wilderness flow is a
pathfinding problem across 144 × 144 rather than a table.*

- **BRG-12 — The borg crosses the wilderness on purpose.** Treat depth 0 as
  ground to cross rather than a town to shop in: flow toward dungeon mouths,
  prefer `FEAT_ROAD`, and treat deep water, open sea and rock as the obstacles
  they are.
- **BRG-13 — The borg knows a dungeon has a floor, and where the next one
  is.** Read the `dun_type` depth ranges. When a dungeon bottoms out, walk to a
  deeper one rather than scumming for stairs that do not exist. `borg_prepared`
  needs to become a function of (dungeon, depth), not depth.
- **BRG-14 — The borg uses the six wilderness services.** The healer and the
  inn first, since they change what "recover" means; then magesmith, recharger,
  magetower and chaos tower.

**Exit:** a borg run started in the fixed starting town reaches a named dungeon
it did not start next to, and its recorded depth stops being capped by the
first dungeon's floor.

---

### B3 — Allegiance and the newer player systems

*Medium. Cannot start before M10 is playable, and should not start much
after — see principle 4 of the Phase 2 plan.*

- **BRG-15 — The borg never attacks its own pets, and counts them as help.**
  Read `MON_ALLEGIANCE_*`: exclude pets from the kill list and from
  `borg_danger`, and count them on the player's side of a fight. This is the
  one requirement here that produces visibly wrong behaviour the moment M10
  ships, rather than merely missed opportunity.
- **BRG-16 — Mutations, virtues and luck are borg traits.** Add them to
  `borg.trait[]` so the borg can at least avoid the mutations that hurt it.
  Commanding pets (PLR-23…PLR-27) is deliberately **not** in scope: BRG-15 is
  about not making things worse.

**Exit:** a borg with pets does not attack them, and the mutation set is
visible in `^z 0`.

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
