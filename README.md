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

> **Status: playable, and most of the way built.** Ten of the eleven milestones
> are done — the wilderness, the towns, the quests, the bestiary, the races and
> classes, the mutations, the virtues, the seven realms of magic and the pets.
> The eleventh, nightmare mode, now does everything Zangband's own nightmare
> mode did; what is left of it is a decision not yet taken rather than work not
> yet done. It has gone out only as pre-releases, and it has not been played
> through by anybody but its author. See [Current state](#current-state).

## Current state

| | |
|---|---|
| **Base** | Angband 4.2.6 |
| **Platform** | macOS, Windows, Linux, DOS, Nintendo DS, 3DS and the browser (see [Portability](#portability)) |
| **Playable** | Yes |
| **Latest release** | [3.121.0](https://github.com/z88kat/ZangbandTK/releases), 15 September 2026 — a pre-release, as all of them are so far |
| **Savefiles** | Not compatible with Angband or Zangband, and never will be. Compatible across ZangbandTK versions: a character saved by an older build loads into a newer one |

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
  have, or another mutation cancelling it out.
- **Nightmare mode.** A birth option that cannot be turned off afterwards, and
  the sixteen things Zangband's own mode does: monsters with twice the hit
  points and ten more speed, awake from the moment the level is made, generated
  far out of their depth and given no free move when they are summoned; stealth
  worth half as much; sustains that hold twelve times in thirteen instead of
  always, and drains that stick when they land; stair creation that does
  nothing; walls that look like floor; a bell in the last hour before midnight
  and the Ancient and Foul Curse on the stroke of it; and one Word of Recall in
  six hundred and sixty-six that arrives deeper than you asked. The status line
  says `Nightmare` in red for as long as the character lives — Zangband showed
  the mode in the character dump alone, which is a file you write after you are
  dead.

**Not yet:** nightmare mode's stage 2 — the things its spoiler describes and
Zangband never actually built — which is a decision that has not been taken
rather than work that has not been started, and is what keeps M11 open. A
Tcl/Tk front end is planned for a later phase, reviving the original's
interface on Tcl/Tk 9: Tcl and Tk 9.0.4 are in the tree, one script builds
them, and CI keeps that toolchain green. None of the original's front-end C is
being ported — the survey concluded it loses to Tk 9 and Angband 4.2 on the
merits, file by file, so the front end is rebuilt on seams 4.2 already has.

## Building

### Requirements

- **macOS**, on Apple Silicon or Intel. Both are built and released; each Mac
  builds for itself by default, and `ARCHS=x86_64` (or `arm64`) asks for the
  other. Apple Silicon needs macOS 11 or later.
- **Xcode command line tools** — `xcode-select --install`
- **CMake** — only to run the test suite. `brew install cmake`
- **GCC** — only for `scripts/check-build`'s third pass, which reproduces what
  CI's Linux runners see. `brew install gcc` lands it as `gcc-16` beside the
  system clang; nothing else in the build uses it and `cc` stays clang.
- **Sphinx** — only to build the manual, which is `scripts/check-build`'s
  fourth pass. One `venv` in the tree; see [docs/README.md](docs/README.md).
- **Python 3.11+** — only for the data conversion tools. macOS ships 3.9, which
  has no `tomllib`; `brew install python@3.13` adds `python3.13` beside it
  without displacing the system `python3`. Name it explicitly when running the
  tools — `python3.13 tools/zconv/zconv.py analyse`. See
  [tools/zconv/README.md](tools/zconv/README.md).

### The game

```sh
cd src
make -f Makefile.osx -j$(sysctl -n hw.activecpu)
```

That produces `ZangbandTK.app` in the repository root. Double-click it, or
`open ZangbandTK.app`.

### The tests

```sh
cmake -S . -B build -DSUPPORT_TEST_FRONTEND=ON
cmake --build build --parallel
cd build && make alltests
```

1443 unit tests and 6 integration tests. They should all pass; if they do not,
that is a bug worth reporting.

**Both build commands above are permissive, and CI runs neither of them that
way.** The macOS job builds with `env OPT="-Werror"`; every Linux job
configures CMake with `-Werror`. A warning that scrolls past here is fatal
there, which has let defects reach master. Before pushing:

```sh
scripts/check-build           # every build and the manual, with CI's flags
scripts/check-build --tests   # ...and run the unit tests after
```

Its exit code is the answer; it does not print a verdict for you to read past.

It runs three builds and the manual, because CI runs all four and none of them
subsumes another:

| | Compiler | Adds |
|---|---|---|
| `Makefile.osx` | clang | `-Wshadow`, `-Wwrite-strings`, `-Wmissing-prototypes`, `-Wnested-externs`, `-Wunused-macros`; c99 |
| CMake | clang | `-pedantic`; gnu99; builds and runs the unit tests |
| CMake | GCC | `-Wlogical-op`, and everything else GCC sees that clang does not |
| Sphinx | — | `-W`, so a broken directive or a bad cross reference is fatal |

A defect can pass any three of them. `depth > 0 \|\| depth > 0` is an error to
GCC and silent to clang; a pointer of the wrong type is fatal to both, and was
caught by neither until the flags were being passed; and a malformed `.rst`
directive is fatal to none of the three, but fails the job that publishes
[zangbandtk.com](https://zangbandtk.com/).

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
Nintendo DS and 3DS, and WebAssembly for the browser. DOS goes further and runs the game under DOSBox from a script,
which is what catches a data file it cannot open. None of them is played
through, so *builds* is a stronger claim than *works*.

There is some irony here: the original ZangbandTK supported Windows and X11 and
never supported macOS at all, so the Tcl/Tk front end will be the *new* port
when it arrives, not the other way round.

## Tools

`tools/zconv` converts Zangband's data files onto Angband 4.2's model. Its
primary output is a review report, not the data files — every value it produces
names the rule that produced it, a confidence level, and whether the tool had to
invent it. See [tools/zconv/README.md](tools/zconv/README.md).

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
the Tcl/Tk framework, tile engine and interface that a later phase of this
project intends to revive. Roughly 49,000 lines of it survive in the archives.

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
