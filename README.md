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

> **Status: early.** The game is playable, already feels different from Angband,
> and has the wilderness — Zangband's defining feature — under it. There is a
> long way to go. See [Current state](#current-state).

## Current state

| | |
|---|---|
| **Base** | Angband 4.2.6 |
| **Platform** | macOS on Apple Silicon (see [Portability](#portability)) |
| **Playable** | Yes |
| **Savefiles** | Not compatible with Angband or Zangband, and never will be |

**Done:**

- **Zangband's lethality.** Every monster carries 73% of Angband's hit points
  and 50% of its armour class — the measured difference between Zangband 2.7.5
  and the Angband it forked from. Monsters die sooner and hit more often.
- **389 imported monsters**, including the princes of Amber and the Mythos
  deities. 1013 in total.
- **51 artifacts**, including Grayswandir and Frakir.
- **18 ego types**, including `(Vampiric)`, `(Chaotic)` and `(Trump Weapon)`.
- **Three weapon mechanics** Angband has no equivalent of: vampiric, vorpal and
  chaotic.
- **The Ancient and Foul Curse**, with its cascade intact.
- **A wilderness.** A world 2064 grids square, generated from a seed and never
  stored, with the town standing in it and roads out of it. Terrain follows from
  height, population and law; danger follows from law alone. Deep water can be
  waded and drowned in, the world ends in open sea, and what you drop in the
  country stays where you left it until somebody finds it.

**Not yet:** multiple towns and dungeons, quests, magic realms, mutations,
virtues, pets, and nightmare mode. A Tcl/Tk front end is planned for a later
phase, reviving the original's interface on Tcl/Tk 9.

## Building

### Requirements

- **macOS on Apple Silicon.** Intel Macs are not supported (they reach legacy
  status in September 2026).
- **Xcode command line tools** — `xcode-select --install`
- **CMake** — only to run the test suite. `brew install cmake`
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

941 unit tests and 5 integration tests. They should all pass; if they do not,
that is a bug worth reporting.

Adding a source file or a data file also means telling the build inputs that
are maintained by hand — the Visual Studio project, the DOS 8.3 renames, the
install list. `scripts/check-build-lists` compares them against the tree and
names anything missing; CI runs it on every push, but it needs only a checkout,
so it is quicker to run it yourself:

```sh
scripts/check-build-lists
```

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

macOS is the delivery target, but the code is kept portable and Angband's CI
covers Linux and Windows builds. Neither is tested by us yet.

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
