# ZangbandTK

**Rebuilding the spirit of Zangband on a modern Angband.**

*The name is the original's, deliberately.* ZangbandTK was Tim Baker's Tcl/Tk
front end to Zangband, and this is a rebuild of that game rather than a
different one wearing its coat. Where the 2005 version is meant, it is called
*the original* below.

Zangband was one of the great Angband variants — a wilderness to cross, towns to
visit, mutations, pets, chaos patrons, and a bestiary drawn from Roger Zelazny's
Amber and H. P. Lovecraft's Mythos as much as from Tolkien. Its development
stopped in 2005, at version 2.7.5-pre1.

Angband did not stop. It is now at 4.2.6, with twenty years of better level
generation, a real data-driven architecture, and a proper object property
system that Zangband never had.

ZangbandTK puts the first on top of the second. It is not a port: Zangband's
2005 codebase is not what is worth preserving. Its *character* is.

> **Status: playable, and most of the way built.** Every gameplay milestone but
> the last is closed — the wilderness, the towns, the quests, the bestiary, the
> races and classes, the mutations, the virtues, the seven realms of magic and
> the pets. The last one is nightmare mode, and what remains of it is a decision
> not yet taken rather than work not yet done. Beside the game there are two
> workstreams the milestones do not cover: a [Tcl/Tk front end](#the-tcltk-front-end),
> which is built and exercised by CI, and [the borg](#the-borg), which is the
> only thing here that plays the game. It has gone out only as pre-releases, and
> it has not been played through by anybody but its author. See
> [Current state](#current-state).

## Current state

| | |
|---|---|
| **Base** | Angband 4.2.6 |
| **Platform** | macOS, Windows, Linux, DOS, Nintendo DS, 3DS and the browser (see [Portability](#portability)) |
| **Playable** | Yes |
| **Releases** | On the [Releases page](https://github.com/z88kat/ZangbandTK/releases), and every one of them so far is a pre-release. The version in the tree is ahead of the newest tag most of the time; `src/buildid.h` is the authority for what you have built |
| **Savefiles** | Not compatible with Angband or Zangband, and never will be. Across ZangbandTK versions the rule is that **content can be dropped and identity cannot be invented**: a character whose objects have been renamed or removed wakes up without them and loads, and a caster whose spell list was replaced under it is refused rather than guessed at. The refusals in force are named, with reasons, in [tests/saves/EXPECTED-FAILURES](tests/saves/EXPECTED-FAILURES) |

**Done:**

- **Zangband's lethality.** Every monster carries 73% of Angband's hit points
  and 50% of its armour class — the measured difference between Zangband 2.7.5
  and the Angband it forked from. Monsters die sooner and hit more often.
- **387 imported monsters**, including the princes of Amber and the Mythos
  deities. 1013 in total.
- **51 artifacts**, including Grayswandir and Frakir.
- **16 ego types**, including `(Vampiric)`, `(Chaotic)` and `(Trump Weapon)`.
- **Three weapon mechanics** Angband has no equivalent of: vampiric, vorpal and
  chaotic.
- **The Ancient and Foul Curse**, with its cascade intact.
- **A wilderness.** A world 2064 grids square, generated from a seed and never
  stored, with towns standing in it and roads between them. Terrain follows from
  height, population and law; danger follows from law alone. Deep water can be
  waded and drowned in, the world ends in open sea, and what you drop in the
  country stays where you left it until somebody finds it.
- **Towns and dungeons.** A dozen towns, differing in size, in who lives in them
  and in which trades they hold, joined by routed roads; thirteen dungeons, each
  with its own depth range, its own inhabitants and its own kind of treasure.
  Six building services, including an inn that sells a night's sleep and the
  dreams that come with it.
- **Quests.** All six of Zangband's kinds, taken from somebody in a town,
  carried, and handed back for payment — a bounty, a delivery, a place to find,
  a killing at a named depth of a named dungeon, a killing in the open, and a
  thing to fetch.
- **Virtues.** Eight of Zangband's eighteen, chosen for each character at birth
  by class, race and magic, and moved by what they kill, spend and spare. Read
  by the Lords of Chaos when they hand down a reward, and by the dream at the
  inn. Zangband tracked them for seven years and never once read one.
- **Twenty-eight races and every class Zangband has.** Seventeen races imported
  beside Angband's own eleven: Amberite, Beastman, Yeek, Draconian, Mindflayer,
  Vampire, Golem, Barbarian, Klackon, Nibelung, Imp, Skeleton, Zombie, Spectre,
  Ghoul, Sprite and Half-Titan. Twenty-two of the twenty-eight carry an
  activatable power, which Angband has no mechanism for. The classes are the
  Monk with unarmed progression, the Mindcrafter with psionics, the
  Chaos-Warrior sworn at birth to one of nine Lords of the Courts of Chaos, and
  the Warrior-Mage and High-Mage, which are defined by the realms they choose
  and so arrived with them.
- **Seven realms of magic** where Angband has four, adding Sorcery, Chaos and
  Trump. Chosen at birth from what your class allows, and permanent. Two
  hundred and twenty-four workings in twenty-eight books, every one of them
  Zangband's: Sorcery has no attack spell in it, Chaos backfires, and Arcane is
  bought outright in town. Seventeen of the two hundred and twenty-four are
  declared inert and say so where you read them.
- **Pets.** A monster can be on your side, which Angband has no notion of.
  Three sides rather than two, nine orders given as a standing policy, mana
  upkeep charged on the sum of your pets' levels, and four of them follow you
  downstairs — where Zangband left them behind.
- **Ninety-six mutations.** Chaos changes you, permanently and rarely for the
  better: standing changes to your body, powers you can invoke, things that
  happen to you unasked, and extra limbs that attack. You do not choose them.
  A Lord of the Courts may hand one down instead of a favour, raw chaos leaves
  them behind, and a Beastman is born mutated and keeps changing. Getting rid
  of one is hard: the rarest potion in the game, a building only great cities
  have, or another mutation cancelling it out. **Some of the ninety-six do
  nothing**, and say so: they need machinery 4.2 has not got — a charisma stat
  it removed, an identification moment it replaced with runes — and they were
  left in rather than dropped, because a mutation that vanishes out of a
  savefile is worse than one that has no effect. [docs/mutations.rst](docs/mutations.rst)
  names them and says what each one is waiting for.
- **Nightmare mode.** A birth option that cannot be turned off afterwards, and
  twelve of the sixteen things Zangband's own mode does: monsters with twice the
  hit points and five more speed, awake from the moment the level is made,
  generated far out of their depth and given no free move when they are
  summoned; stealth worth half as much; sustains that hold twelve times in
  thirteen instead of always, and drains that stick when they land; stair
  creation that does nothing; walls that look like floor; a bell in the last
  hour before midnight and the Ancient and Foul Curse on the stroke of it; and
  one Word of Recall in six hundred and sixty-six that arrives deeper than you
  asked. The status line says `Nightmare` in red for as long as the character
  lives — Zangband showed the mode in the character dump alone, which is a file
  you write after you are dead.

  **The other four were closed by a ruling rather than built**, and the rulings
  are in the decision log rather than in the code, because a reader comparing us
  against Zangband will find the difference and needs to know it was chosen.
  Zangband adds twenty points to a score multiplier; 4.2's score is
  `max_exp + 100 * max_depth`, flat and blind to every birth option, so there is
  nothing to add them to and inventing one would be our design. Zangband strips
  the Golem's stun immunity, but not for nightmare alone — the penalty is shared
  across three of its settings and only one of them is this, so stripping it here
  would be a different bargain from the one the archive offers, and the Golem
  keeps it. And the mode's two nightmares are not built: one has no hook,
  because 4.2 has no player sleep state for it to land on, and the other would
  mean choosing a number nobody has a source for. See DEC-81 and DEC-82.

**Not yet:** nightmare mode's stage 2 — the things its spoiler describes and
Zangband never actually built — which is a decision that has not been taken
rather than work that has not been started, and is what keeps the milestone
open. See DEC-84.

## The Tcl/Tk front end

The original's interface, rebuilt on Tcl/Tk 9 rather than ported. It is **in the
tree and CI builds it on every push**: [src/main-tcl.c](src/main-tcl.c) and
[src/tcl/](src/tcl/), with Tcl and Tk 9.0.4 vendored under [tcltk/](tcltk/) and
one script that builds them. The [Tk workflow](.github/workflows/tk.yaml)
compiles the front end, asserts that the binary links Tk and has not quietly
fallen back to X11, runs the Tcl-side tests, packages `ZangbandTclTK.app`, and
then starts the real game with the real front end and drives it from a script.

It is **not what the releases ship**. The bundle is a CI artifact; the download
is still the Cocoa build below. Where it stands milestone by milestone is in
[.claude/plans/phase3-tcl-tk-frontend.md](.claude/plans/phase3-tcl-tk-frontend.md),
which is the honest account and is worth more than a line here that would go
stale between one week and the next.

To build it:

```sh
scripts/build-tcltk                         # Tcl and Tk 9.0.4 into tcltk/local
cmake -S . -B build -G Ninja \
  -DSUPPORT_TCL_FRONTEND=ON \
  -DTCLTK_PREFIX="$PWD/tcltk/local"
cmake --build build --parallel
scripts/pkg_macos_tcltk build "$PWD/out"    # ZangbandTclTK.app, optional
scripts/run-tcl-tests                       # the Tcl side's own tests
scripts/run-tcl-bridge-test build           # ...and the scripted session; opens a window
```

None of the original's front-end C is being compiled in — the survey concluded
it loses to Tk 9 and Angband 4.2 on the merits, file by file, so the front end is
rebuilt on seams 4.2 already has and the 2001 sources are read as reference.

There is some irony here: the original ZangbandTK supported Windows and X11 and
never supported macOS at all, so this is the *new* port, not the other way round.

## The borg

**Nothing else in this repository plays the game.** The unit suites test rules in
isolation and are good at it; not one of them walks a character out of a town,
across the wilderness, into a dungeon and back. Angband's borg does, and it is
here as test infrastructure rather than as entertainment — a character that plays
for a million turns walks through more allegiance, spell-casting, mutation and
wilderness code than any fixture anybody is going to write.

The borg's own switch is on by default, but it needs the test front end with it,
and that one is not — without it there is no `-mtest` display module and the run
dies before it starts:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DSUPPORT_BORG=ON -DSUPPORT_TEST_FRONTEND=ON
cmake --build build --parallel

scripts/borg-smoke -x build/game/angband        # fixed seeds; non-zero on a break
scripts/borg-progress -x build/game/angband     # how far it actually gets
```

`borg-smoke` exits non-zero on a crash, an abort or a wedge, and prints the seed
so the run can be repeated rather than reported as a rumour. `borg-progress`
sweeps classes against fixed seeds and reports depth and character level, which
is the regression signal: a borg parked on the first floor exercises almost none
of the mutations, the deeper realms' spells or the out-of-depth monsters.

**A dead character is not a failure**, and neither is one that runs out of clock.
The borg dies often and at low level, so pass-and-fail says nothing here. What
the fleet managed is committed in [tests/borg/BASELINE](tests/borg/BASELINE) and
a drop against it is the regression — the same shape as `EXPECTED-FAILURES`, a
statement of what is true now that somebody has to change on purpose. The
[nightly workflow](.github/workflows/borg.yaml) runs the sweep against that
baseline and posts the table to the run summary.

The headless commands the front end adds, and what the borg does and does not
understand about this game, are documented in
[docs/hacking/borg.rst](docs/hacking/borg.rst). The plan is
[.claude/plans/borg-development-plan.md](.claude/plans/borg-development-plan.md).

## Building

### Requirements

- **macOS**, on Apple Silicon or Intel, for the Cocoa build below. Both are built
  and released; `Makefile.osx` builds **arm64** unless told otherwise, and
  `ARCHS=x86_64` asks for Intel, as a separate download rather than a universal
  slice. For other platforms see [Elsewhere](#elsewhere).
- **Xcode command line tools** — `xcode-select --install`
- **CMake** — for the test suite, the borg and the Tcl/Tk front end.
  `brew install cmake`. The Cocoa build below does not use it.
- **Ninja** — for `scripts/check-build` and `scripts/check-flakes`, which
  configure their build trees with it and ask it which suites exist rather than
  globbing for them. `brew install ninja`.
- **GCC** — for `scripts/check-build`'s GCC pass, which reproduces what CI's
  Linux runners see. `brew install gcc` lands it as `gcc-16` beside the system
  clang; nothing else in the build uses it and `cc` stays clang.
- **Sphinx** — for the manual, which `scripts/check-build` also builds. One
  `venv` in the tree; see [docs/README.md](docs/README.md).
- **Python 3.11+** — for the data conversion tools *and* for
  `scripts/check-build`, which runs the converter's own checks against the
  shipped data. macOS ships 3.9, which has no `tomllib`; `brew install python@3.13`
  adds `python3.13` beside it without displacing the system `python3`. The gate
  finds a usable one itself, or says so and exits non-zero; running the tools by
  hand means naming it — `python3.13 tools/zconv/zconv.py analyse`. See
  [tools/zconv/README.md](tools/zconv/README.md).

### The game

```sh
cd src
make -f Makefile.osx -j$(sysctl -n hw.activecpu)
```

That produces `ZangbandTK.app` in the repository root. Double-click it, or
`open ZangbandTK.app`.

### Elsewhere

Every other platform builds through CMake or autoconf, and the recipes are
Angband's own, which this game has not changed:
[docs/hacking/compiling.rst](docs/hacking/compiling.rst) covers Linux and other
UNIX, Windows with MinGW, MSYS2 and Visual Studio, and the cross builds; and
[docs/hacking/cmake.rst](docs/hacking/cmake.rst) covers the options. The short
version on Linux is a front-end option and a build directory:

```sh
cmake -S . -B build -DSUPPORT_GCU_FRONTEND=ON   # or SDL2, or X11, which is the default
cmake --build build --parallel
```

What CI actually runs on each platform is in
[.github/workflows/](.github/workflows/) — `linux.yaml`, `windows.yaml`,
`msys2.yaml`, `cygwin.yaml`, `dos.yaml`, `nintendo.yaml`, `wasm.yaml` and the
rest — and those are the commands kept working, dependencies included.

### The tests

```sh
cmake -S . -B build -DSUPPORT_TEST_FRONTEND=ON
cmake --build build --parallel
cd build && make alltests
```

Unit tests under [src/tests/](src/tests/), and six end-to-end tests under
[tests/](tests/) that feed input to the test front end and diff the output. They
should all pass; if they do not, that is a bug worth reporting. How many there
are is not written down here on purpose — it moves most weeks, and a number in a
README that nobody re-counts is worse than no number. Each suite reports its own
`N/M passed` as it finishes.

**Both build commands above are permissive, and CI runs neither of them that
way.** The macOS job builds with `env OPT="-Werror"`; every Linux job
configures CMake with `-Werror`. A warning that scrolls past here is fatal
there, which has let defects reach master. Before pushing:

```sh
scripts/check-build           # every build, the manual and the data checks
scripts/check-build --tests   # ...and run the unit tests after
```

Its exit code is the answer; it does not print a verdict for you to read past.

It runs several passes, because CI runs all of them and none subsumes another:
the macOS build through `Makefile.osx` with clang; the Linux `-Werror` jobs in
their CMake shape, once with clang and once with GCC, which sees things clang
does not; the manual through Sphinx with `-W`; the hand-maintained build lists;
the converter's data checks; and a sanitizer build. **The list of passes and the
reason for each is the comment at the top of the script**, which is kept current
because the script is — rather than repeated here, where it would not be.

Two of them are worth knowing about before you meet them:

- **The data checks** (`zconv realms --check`, and the same for mutations,
  artifacts, egos and objects) ask whether the shipped game data is still what
  the converter produces from Zangband's own tables. A spell keyed under a name
  the source table does not have comes out with its level, mana and failure rate
  and *no effect at all* — which looks exactly like a deliberate deferral. One
  shipped that way and was found by accident.
- **The sanitizer pass** reproduces the msys2 ASAN/UBSAN job, and does it
  **without** the frontend options the rest of the script passes. That last part
  is the point: `game/saves` failed two Windows runs while every other job was
  green, and it was neither the sanitizers nor Windows — the savefile corpus is
  staged by a step that hung off `SUPPORT_TEST_FRONTEND`, which one workflow
  passes and the other does not. The only way to see it is to build the way that
  job builds.

A defect can pass every pass but one, which is why none of them is optional.

The GCC pass needs Homebrew's compiler — `brew install gcc`, which lands as
`gcc-16` beside the system clang without displacing it. `cc` stays clang,
which is what the macOS build and CI's macOS job both use. Override with
`GCC=/path/to/gcc scripts/check-build` if yours is elsewhere; without one the
script says so and exits non-zero rather than quietly skipping the pass.

The manual pass needs the Sphinx virtualenv from
[docs/README.md](docs/README.md) — `.venv-docs`, gitignored, so a fresh clone
has to make it once. It behaves the same way as the GCC pass: missing Sphinx
prints the two commands that fix it and exits non-zero. Skipping was the old
behaviour and it hid the fact that the manual was going unbuilt for several
releases.

Adding a source file or a data file also means telling the build inputs that
are maintained by hand — the Visual Studio project, the DOS 8.3 renames, the
install list. `scripts/check-build-lists` compares them against the tree and
names anything missing; CI runs it on every push, but it needs only a checkout,
so it is quicker to run it yourself:

```sh
scripts/check-build-lists
```

`scripts/check-build` runs it too, as of 3.79.2. It used to be a separate step
you had to remember and three pushes in a row went red on it, which is the
fourth time a pass has been added to the gate because CI caught something the
gate did not ask about. The answer has been the same every time: build the way
CI builds.

### Flakes

A green run says the tests passed once, not that they are deterministic. Several
suites seed the RNG and then let the game do real work, so a test can be right
about the rule it checks and still fail one run in sixteen because something the
rule does not mention fired — `game/wild`'s `a-refused-power-is-free` did
exactly that, and one run in sixteen is invisible to a gate that runs once.

```sh
scripts/check-flakes -n 12
```

runs every suite twelve times and reports anything that was not the same every
time, naming the suite, the pass and — by asking the suite again with `-v` — the
individual test. It takes the list of suites from `ninja -t commands
allunittests` rather than from disk, and says so, because globbing
`build-*/unittests/*/*/*` picks up binaries from configurations that no longer
exist: a diagnostic suite written, run and deleted leaves its binary behind and
its tests keep being counted. That inflated a reported total by three. Anything
on disk that ninja does not know about is named as an orphan and not counted.

Add `-s player/realm` to run one suite, `-b` for a build directory other than
`build-strict`.

### The manual

```sh
/usr/bin/python3 -m venv .venv-docs
.venv-docs/bin/pip install -r docs/requirements.txt
cd docs && ../.venv-docs/bin/python -m sphinx -b html . _build
```

Use `/usr/bin/python3` explicitly — on macOS, `python3` often resolves to a
tool-specific environment you would rather not install into.

## Tuning it

ZangbandTK's balance dials live in `lib/gamedata/constants.txt` and take effect
on restart, with no rebuild:

```
lethality:hit-points:73      # percent of base monster hit points
lethality:armor-class:50     # percent of base monster armour class
melee:vorpal-chance:6        # a vorpal weapon cuts deep on one blow in this many
melee:vorpal-multiplier:2
melee:chaotic-chance:7       # a chaotic weapon discharges on one blow in this many
```

Setting both lethality values to `100` gives behaviour identical to vanilla
Angband 4.2 — a supported configuration, and a useful comparison.

Inside the app bundle the same file lives at
`ZangbandTK.app/Contents/Resources/lib/gamedata/constants.txt`.

## Portability

macOS is the delivery target, and the one the game is developed and played on —
on Apple Silicon, with the Intel build made and smoke-tested by CI on a real
Intel runner rather than cross-compiled. The rest are built by our own CI on
every push — Windows by both MSBuild and nmake, Linux, Cygwin, MSYS2, DOS, the
Nintendo DS and 3DS, and WebAssembly for the browser. DOS goes further and runs
the game under DOSBox-X from a script — the same scripted session the native
end-to-end tests use, so the two cannot drift apart — which is what catches a
data file it cannot open. None of them is played through, so *builds* is a
stronger claim than *works*.

## Tools

`tools/zconv` converts Zangband's data files onto Angband 4.2's model. Its
primary output is a review report, not the data files — every value it produces
names the rule that produced it, a confidence level, and whether the tool had to
invent it. See [tools/zconv/README.md](tools/zconv/README.md). Its `--check`
modes are also a pass of the pre-push gate, above.

## Where the reasoning is kept

The manual is [docs/](docs/) — `index.rst` is the front of it, and it is
published at [zangbandtk.com](https://zangbandtk.com/). Chapters for the
wilderness, the towns, the quests, the realms, the pets, the mutations, the
virtues and nightmare mode are written for a player rather than for a
contributor, and are the best description of what this game actually is.
[docs/releases.rst](docs/releases.rst) is the release log and
[docs/diary.rst](docs/diary.rst) is the development journal, including the parts
that turned out to be wrong.

The plans and the decision log are in [.claude/plans/](.claude/plans/), and they
are the project's memory:

- **`decisions.md`** is where every *why is it like this* question is answered —
  numbered, dated, and written at the moment the call was made rather than
  reconstructed afterwards. If something here differs from Zangband, or from
  Angband, the reason is in there.
- **`phase2-development-plan.md`** is the gameplay milestones, M0 to M11, with
  what each one delivered and at which version, including the several occasions
  where a milestone was declared complete and was not.
- **`phase3-tcl-tk-frontend.md`** is the front end;
  **`borg-development-plan.md`** is the borg; and
  **`pending-decisions.md`** is what is waiting on the project owner.

[CONTRIBUTING.md](CONTRIBUTING.md) is Angband's, unchanged, and describes
upstream's process and its repository rather than this one; the git advice in it
applies here, the addresses do not. [docs/hacking/](docs/hacking/) is upstream's
too, with a ZangbandTK section added to the pages where this game diverges.

## Credit

This project is the smallest part of the work it depends on.

**Angband** — Ben Harrison, James E. Wilson, Robert A. Koeneke, and everyone who
has maintained and developed it since, currently led by Nick McConnell. Twenty
years of work on generation, balance and architecture is what makes any of this
worth doing.

**Zangband** — created by **Topi Ylinen**, maintained by **Robert Ruehlmann** and
the Zangband DevTeam. Their reputation for devious and contrary design is
entirely deserved, and the Ancient and Foul Curse, which bears Topi's name, is
the proof.

**AngbandTk and ZAngbandTk** — **Tim Baker**, who between 1997 and 2001 wrote
the Tcl/Tk framework, tile engine and interface that this project's front end is
rebuilding. Roughly 49,000 lines of it survive in the archives.

**Roger Zelazny**, whose *Chronicles of Amber* gave Zangband its princes,
its Pattern and its Trumps. **H. P. Lovecraft**, whose Mythos gave it everything
waiting at the bottom of the dungeon.

**The Tcl Core Team**, for Tcl/Tk.

## Licence

ZangbandTK is available under the **Angband licence**:

> This software may be copied and distributed for educational, research, and not
> for profit purposes provided that this copyright and statement are included in
> all such copies. Other copyrights may also apply.

Angband is dual-licensed under the GPL v2 *or* the Angband licence. Zangband was
released under the Angband licence alone, and ZangbandTK incorporates Zangband
material, so the Angband licence is the option available here. In practice that
means **non-commercial distribution** — the same terms Zangband itself carried.

See [docs/copying.rst](docs/copying.rst) for the full statement, including
exceptions covering bundled libraries and graphics.
