# ZangbandTK — Project Decision Log

Settled decisions, with rationale. Referenced by the phase requirement docs. Amend rather
than duplicate: if a decision changes, edit it here and note what it supersedes.

See [Idea.md](Idea.md) for the original project statement.

---

## Sources and targets

**DEC-01 — Base is Angband 4.2.6**, this fork. Modern codebase, active upstream, clean
separation between game and UI layers compared to the 2.8.1-era sources.

**DEC-02 — Zangband reference is 2.7.5-pre1**, in
[archive/zangband/](../../archive/zangband/), *curated*. The last available Zangband source
(June 2005). Curated means iconic features are taken and 2005-era accretion is not — see
DEC-05.

**DEC-03 — Difference baseline is Angband 2.8.1**, in
[archive/angband-281/](../../archive/angband-281/). The common ancestor of both lines, so
differences against it isolate what Zangband actually changed.

> **Archive note.** The idea document refers to `archive/tk/AngbandTk` as "ZangbandTK
> 2.9.2". That tree is **AngbandTk 2.9.2** — a Tk front end over vanilla *Angband* 2.9.2.
> The ZAngbandTk C source is `archive/Tk/zangtk-240r5-src.zip` (**ZAngbandTk 2.4.0r5**,
> built against Zangband 2.4.0), now unpacked to
> [archive/Tk/ZAngbandTk-240r5/](../../archive/Tk/ZAngbandTk-240r5/).
>
> **The Tcl UI is not in that archive.** The unpacked source contains **zero** `.tcl`
> files — C only. The Tcl user interface survives solely in
> [archive/zangbandtk/OmnibandTk-1.4/tk/](../../archive/zangbandtk/OmnibandTk-1.4/) — 66
> scripts at the top level, 116 including subdirectories (`birth.tcl`, `building.tcl`,
> `character-window.tcl`, `choose-monster.tcl`, …). Despite being distributed as a Windows
> runtime, that directory is the **only** copy of the ZangbandTK interface in existence
> here, and is therefore the primary Phase 3 asset rather than a dead end.
>
> Its structure is also instructive: the 116 scripts are shared across variants, with
> `variant/AngbandTk-292r2/` and `variant/ZAngbandTk-240r5/` carrying only `config`, `doc`
> and `image`. The UI was already parameterised by variant once — a useful precedent for
> attaching it to 4.2.
>
> **The C↔Tcl bridge is in `commontk`**, unpacked to
> [archive/Tk/CommonTk-1.4/](../../archive/Tk/CommonTk-1.4/) — the piece neither the game
> source nor the script directory contains, and the reason Phase 3 is tractable. 49,285
> lines in `src/common` plus a `src/common-dll` variant:
>
> | Component | Files | Lines |
> |---|---|---:|
> | Tcl interpreter bridge | `interp1.c`, `interp2.c` | 10,161 |
> | Character creation UI | `birth-tnb.c` | 3,716 |
> | Tile / icon engine | `icon1.c`, `icon2.c` | 4,962 |
> | Map widget, incl. isometric | `widget.c`, `widget-iso.c`, `canv-widget.c` | ~4,000 |
> | Sound | `sound.c` | 2,705 |
> | Town, monster and object UI | `town.c`, `r_info.c`, `describe.c` | ~6,400 |
> | Event binding, platform, entry | `bind.c`, `qebind-dll.c`, `plat.c`, `main-tnb.c` | ~3,500 |
>
> So the complete original stack survives: framework (commontk) + UI scripts (OmnibandTk) +
> per-variant game C (ZAngbandTk 2.4.0r5) + tiles and sounds.

**DEC-16 — The official Zangband documentation is a first-class requirements source.**
The manuals and spoilers archived from zangband.org (links in [Idea.md](Idea.md)) describe
what Zangband was *intended* to do. They state design intent where the code states only
what was built, so where the two disagree the documentation is usually the better guide to
what was meant. Requirements derived from them should cite the document.

> Originally justified by the clean-room approach; that justification is void under DEC-20,
> but the decision survives it on the merits above. Documentation and source are now
> complementary sources rather than alternatives — intent from one, algorithm from the other.

> **Availability, checked 2026-08-15:** 34 of the 36 links resolve to real content. Two do
> not: the site root, and `spoilers/life.txt` (the Life realm spell list), which returns
> zero bytes at every snapshot tried from 2005 to 2022. The Life realm content is
> recoverable from [archive/zangband/](../../archive/zangband/) instead. Note also that
> `web.archive.org` cannot be retrieved through WebFetch in this environment; `curl` works.

**DEC-20 — Clean-room is dropped. Zangband's source may be read, ported and adapted.**
Supersedes the clean-room framing in [Idea.md](Idea.md), at the project owner's direction.

*Why it changes things.* Several systems carry their value in the algorithm rather than the
behaviour, and a requirements document cannot convey them. The wilderness is the clearest
case: fractal terrain generation, the height/population/law decision tree, block caching,
and road and river routing between towns are ~9,000 lines in
[wild1.c](../../archive/zangband/src/wild1.c),
[wild2.c](../../archive/zangband/src/wild2.c) and
[wild3.c](../../archive/zangband/src/wild3.c) that took years to get right and would be
expensive to rediscover from a description. The same applies to the Ancient and Foul
Curse's cascade probabilities, mutation effects, chaos patron tables, and building placement
in parameter space.

*How to use it.* **Port where the algorithm is the value; reimplement where 4.2's
architecture differs.** Zangband's wilderness *algorithms* are worth taking almost verbatim;
its wilderness *data structures* are not, because W-1 puts the wilderness on 4.2's chunk
abstraction rather than Zangband's parallel region system. Reading the source to understand
intent is always correct; copying it into a structure it was not written for is not.

*What does not change.* The Phase 1 requirement documents stand — they define what is being
built and why, and source access changes only how. DEC-16 also stands: the official
documentation states *intent*, the source states *implementation*, and where they disagree
the documentation is usually the better guide to what was meant.

> **Licence position — checked, and straightforward.** Zangband's files carry the Angband
> licence alone: *"copied and distributed for educational, research, and not for profit
> purposes provided that this copyright and statement are included in all such copies"*
> ([wild1.c](../../archive/zangband/src/wild1.c)). Angband 4.2 is dual-licensed — GPLv2 **or**
> that same Angband licence, in identical words ([docs/copying.rst](../../docs/copying.rst)).
>
> Both are therefore available under the same terms, and porting is unproblematic. Two
> consequences to accept deliberately:
>
> 1. **Keep the copyright headers.** That is the licence's only substantive obligation, and
>    it is satisfied by leaving the existing block in place and adding ours beneath it.
> 2. **Files containing Zangband code are Angband-licence only, not GPL-dual**, since
>    Zangband never offered the GPL option. In practice this makes the project as a whole
>    effectively Angband-licence — which means **non-commercial distribution**. Standard
>    across the variant community, and what Zangband itself did from Angband 2.8.1.

---

## Scope

**DEC-04 — In scope:** world & towns (wilderness overworld, multiple towns, buildings,
quests) · player systems (races, classes, mutations, virtues, pets) · content & flavour
(Amber / Chaos / Cthulhu monsters, artifacts, ego items, themed levels).

**DEC-05 — Out of scope:** Zangband's Lua scripting layer (`l-*.pkg`, `src/lua/`) and the
Zborg. Both are large, and 4.2 has no scripting layer to displace and its own borg already.

**DEC-06 — Angband 4.2's existing borg stays compiling but is not extended.** It will not
understand wilderness, pets or mutations, and will grow less useful as features land. No
effort is spent on it either way.

**DEC-07 — No legacy savefile support.** Old Zangband and old Angband saves are not
loadable. Removes the `load-old.c` compatibility burden and frees the savefile format to
extend as features require.

---

## Balance

Detail in [phase1-balance-calibration.md](phase1-balance-calibration.md); summarised here.

**DEC-08 — Zangband's experience system is adopted as-is; no house multiplier.** Measurement
showed Zangband has no faster levelling to inherit — its experience per kill is 0.98× of
2.8.1's, its threshold table is byte-identical, and its race `expfact` values are slightly
*higher*. An earlier decision to adopt "5× experience" rested on a comparison of `mexp`
fields between versions that define `mexp` differently; it is void.

**DEC-09 — Zangband's lethality is applied as a global scalar**, hit points × 0.73 and
armour class × 0.50, on top of 4.2's values rather than importing Zangband's per-monster
numbers. Preserves 4.2's relative tuning between monsters while adopting Zangband's
absolute lethality. This is the project's primary balance dial and lives in
`constants.txt` for cheap retuning.

**DEC-10 — Monsters shared with 4.2 keep 4.2's balance.** They are common inheritance from
2.8.1 and are not what makes Zangband distinctive. Zangband-only content is calibrated onto
4.2's curve instead of inheriting Zangband's absolute numbers.

---

## Governing principle

**DEC-18 — Rigour on facts, judgment on taste. Where Zangband cannot be reproduced, choose
the feel and move on.**

Angband did not stand still for twenty years, so a faithful Zangband on a 4.2 base is not
achievable and was never the goal. Many differences have no correct resolution — they are
choices between two reasonable things. Those get decided quickly, by feel, and revisited
after playtest rather than debated in advance.

This does **not** relax the standard on questions that have verifiable answers. The
distinction is the whole point:

- **Facts** — conversion constants, formula semantics, what a field means to the code that
  reads it. These have right answers and getting them wrong produces bugs. Measurement of
  the experience formula caught an error of 20×; measurement of the to-hit divisors caught
  a wrong armour class constant. BAL-08 exists to keep that standard. Never guess here.
- **Taste** — world size, how many towns, which of 389 monsters, whether a mutation earns
  its place. No amount of analysis produces a correct answer. Pick, build, play, adjust.

A corollary worth stating plainly: 4.2's twenty years are mostly *improvements* — better
generation, a proper property system for objects, a real data-driven architecture. Where we
keep 4.2's approach over Zangband's, that is usually the better engineering choice and not
a compromise. The project takes Zangband's *character*, not its 2005 implementation.

Resolutions reached under this principle are recorded in **DEC-19**. They were taken quickly
and deliberately, and playtest outranks all of them — but they are decisions, not
placeholders.

**DEC-25 — Zangband is the reference, not the authority. Amber is the anchor.**

Zangband was not a perfect game, and reproducing its mistakes faithfully would be a poor use
of the effort. Where its execution falls short of its own intent, we follow the intent.

The intent has a source. Zangband was built on Roger Zelazny's *Chronicles of Amber*, and
that is the story the game is set inside: Amber the one true city; the Courts of Chaos at
the other pole; Shadow, the infinity of worlds between them, which those of Amber's blood
can walk and reshape as they travel; the Pattern and the Logrus as the ordering and
unmaking principles; the Trumps as cards that carry you between them; and Oberon's
quarrelling children.

*How this is used.* When a taste question has no clear answer from Zangband — or when
Zangband's answer is weak — the question becomes what serves that story. Three consequences
worth naming now:

- **The wilderness is Shadow.** An Amberite walking between worlds, the landscape shifting
  as they go, is the central image of the books. That is a stronger reading of the overworld
  than "a map between dungeons", and it is available for free — the surface already
  generates from a seed and a position.
- **The Amber material earns priority over the rest.** DEC-19's "theme first" already says
  so for content; this says why. The princes, the Pattern, the Trump realm and the Courts
  are the spine. Zangband's other borrowings — its NetHack vault homages, its cyberdemons,
  Barney the dinosaur — are not, and nothing is lost by letting them go.
- **Gaps may be filled from the books rather than from Zangband.** Where Zangband names
  something from Amber but does little with it, we are free to do more, and the source
  settles what "more" should look like.

This sharpens DEC-18 rather than replacing it. Facts still get rigour; taste still gets
judgment. What changes is that taste now has somewhere to appeal to.

---

## Upstream relationship

**DEC-11 — Hard fork from a pinned Angband base, with cherry-pick discipline.**
Supersedes the earlier "additive-first, keep merges viable" decision.

*Rationale.* Maintaining true mergeability would constrain the design precisely where the
project is most ambitious. The wilderness work in DEC-04 restructures level generation, and
upstream is actively refactoring that same ground — recent commits add a chunk argument to
monster movement and deletion, move terrain references in data files from names to codes,
and rework generation bounds and stair placement. Designing around that churn would force
hooks and indirection where a direct change is clearer, and would still not produce clean
merges.

The value being given up is smaller than it looks. Once generation and the player model
diverge, most upstream gameplay commits either will not apply or will not be *wanted* —
many tune vanilla balance that DEC-09 deliberately replaces.

*What is retained.* Merge compatibility and cherry-pick compatibility are different costs.
Full mergeability requires never restructuring shared files; cherry-picking a targeted fix
requires only that the file still exists and is recognisably related. The latter is nearly
free and captures the real value — crashes, leaks, portability, undefined behaviour. So:

- Record the exact upstream base commit and tag it, so diffing and cherry-picking always
  have a reference point. **Done** — tag `angband-base` at `dc40ec9e0`.
- Design for clarity. Do not contort the architecture to preserve mergeability.

This is the path Zangband itself took from Angband 2.8.1.

> **Upstream remote — deliberately not configured.** An earlier draft of this decision
> called for adding upstream as a git remote for periodic review. The project owner has
> since removed the GitHub fork relationship, making this a standalone private repository.
> The `angband-base` tag preserves everything cherry-picking actually needs, and a remote
> can be added in one command whenever an upstream fix is wanted. Left unconfigured so the
> repository stays genuinely independent.

---

## World

**DEC-26 — The town is part of the surface, not a level of its own.**

Angband's town is a level: you generate it, you stand in it, and the only way
out is the staircase. ZangbandTK's town is a patch of the overworld. It is drawn
into the live window wherever the window covers its position, and walking out of
it is walking, not a level change.

*Why it had to be settled.* The alternative — keeping the town a level and
making its edge a transition to a wilderness level — would have worked and been
less code. It was rejected because the seam would be visible: you would step
from a town map onto a differently-shaped wilderness map, which is exactly what
the original did not do. Zangband's town and the forest outside it are the same
picture, and that continuity is most of what the overworld is for.

*What it costs.* The town no longer persists through `chunk_list` as Angband's
did, because it is not a level for `chunk_list` to hold. Its layout is instead
generated deterministically from the world seed and its own position, so it
comes back identically every time the window is rebuilt. What that does *not*
recover is anything the player changed — a dropped object in town, or a
townsperson killed. The wilderness has the same gap everywhere else on the
surface, and WLD-04 closes it for all of them at once rather than the town
needing a mechanism of its own.

*One thing this forced.* Angband's town is a starburst clearing inside a
permanent wall, with rock filling the corners. Nothing in it was ever meant to
be walked out of, and dropping it into open country produced a town with no exit
— which is what the project owner found on first play. Four ways out are now cut
through, one on each side, working outwards from the middle of each side and
refusing to breach a shop wall. M5's walls and gates will be built around those.

**DEC-27 — Zangband's ideas, Angband 4.2's implementation. Where Zangband merely
looks like Angband 2.8.1, 4.2 wins.**

Zangband was built on Angband 2.8.1, so a great deal of what Zangband *looks* like is
simply what 2.8.1 looked like. Its town is a rectangular grid of shops because that is what
2.8.1's town was; the walls, the moat and the gates are dressing on a 2.8.1 town. None of
that is a Zangband idea. Reproducing it would not be rebuilding Zangband — it would be
undoing twenty-five years of Angband and calling the result a variant.

*The test.* For any feature taken from Zangband, ask: **is this an idea, or is it 2.8.1
showing through?** Ideas are ported. Inherited appearance is not — 4.2's equivalent stands,
and the Zangband idea is expressed through it.

Worked through on the town, which is where this first bit:

| | |
|---|---|
| **Idea — keep** | The town stands in a wilderness and you walk out of it. Several towns, differing in size and character. Buildings that do something, and people in them to take work from. Different towns carrying different stores. |
| **2.8.1 showing through — drop** | Walls, moat and gates. Buildings on a rectangular grid. Anything whose only argument is "the screenshots look like that". |

So the town is Angband 4.2's town — its starburst clearing, its streets, its ruins, its
shops — standing in open country, with a way out. Not a walled compound with a portcullis.

*Applied to the town, including one over-correction worth recording.* Reading this
principle, the first response was to strip the rock 4.2's clearing is blasted out of, on the
grounds that a ring of granite around a town standing in a field reads as a wall, and a
walled town is the thing being avoided. That was wrong, and playing it showed why within
minutes: the rock is not decoration. It is what keeps the wilderness out of the market
square, and it is what stops line of sight at the edge of town. Removing it left the town
open to anything that cared to walk in, and left a new character seeing half a county from
the staircase.

So the rock stays, and the roads go through it — 4.2's town, entire, with four ways out cut
where a road would meet it. The principle was right; the inference drawn from it was not.
**The test is whether a thing is a Zangband idea or 2.8.1 showing through, not whether it
happens to resemble a wall.** 4.2's rock is 4.2's own, and it earns its place on function.

*Where this sharpens what came before.* DEC-25 says Zangband's execution is not the
standard of correctness, and points taste at Amber when Zangband's answer is weak. DEC-27
handles the case where Zangband's answer is not weak but *inherited*: it was right for the
game it was built on, and that game is not this one. DEC-16's preference for the
documentation over the source is the same instinct — intent outranks implementation.

> Raised by the project owner while playing M4, and correctly: *"we would like to impose the
> idea of zangband using angband 4.2 as a base, not to re-write back to the old 2.8 version
> of angband."*

---

## Delivery

**DEC-12 — macOS ARM is the *delivery* target; the code stays portable.** Amended after
measuring the actual platform surface. Ship and test on macOS; do not write macOS-only code.

> **The original framework supported Windows and X11 — never macOS.** commontk carries 109
> `PLATFORM_WIN` and 69 `PLATFORM_X11` conditionals and **zero** Aqua or `MAC_TCL` paths.
> The premise behind the original decision — that macOS-only avoids Windows and X11 work —
> was backwards: those two are the platforms the framework was built for, and **macOS is the
> new port**. Since we are paying for the new platform regardless, the marginal cost of the
> two it already supports is small.
>
> **Where the cost actually sits:**
>
> | Layer | Cross-platform cost |
> |---|---|
> | Angband 4.2 game core | **~zero.** Already builds everywhere, with CI for linux, mac, windows, msys2, cygwin, msbuild, nmake and more in [.github/workflows/](../../.github/workflows/). Our gameplay changes are portable C. |
> | Tcl/Tk 9.0.4 | **~zero.** Portable by design — `unix/` (X11), `win/`, `macosx/` (Aqua). The macOS path is already proven (DEC-13). |
> | commontk port to Tk 9 | **The bulk of Phase 3 — and platform-independent.** The Tk 8.3→9.0 API work and the `interp1/interp2` rewrite must happen once, whatever we target. |
> | Platform shims | **Small.** `plat.c` abstracts only four things: a font chooser, X-window-to-HWND conversion, a `system` command, and a millisecond timer. Tk handles drawing portability itself. |
> | Sound | **Already solved.** Seven pluggable backends exist (SDL, OpenAL, DirectSound, BASS, WaveMix, NoSoundCard). SDL is cross-platform and 4.2 already depends on it. |
> | Build and packaging | **Real but modest**, and mostly per-platform configuration rather than code. |
>
> **Policy this sets.** The expensive work is platform-independent and unavoidable, so
> portability is cheap insurance bought *now* and expensive to retrofit later:
>
> 1. Keep 4.2's existing CI workflows green. They already exist and cost nothing to retain;
>    letting them rot is what makes later platform work expensive.
> 2. Preserve `plat.c`'s shim pattern when porting. Platform-specific behaviour goes behind
>    it, never inline.
> 3. Use Tk's portable APIs; do not call Cocoa directly. The four files touching `tkInt.h`
>    are the exception and need per-platform care regardless.
> 4. Choose **SDL** as the sound backend — cross-platform, and already a 4.2 dependency.
>
> Windows and Linux then become a build-and-test exercise whenever they are wanted, rather
> than a re-architecture.

**DEC-21 — Portability is a design goal from the start; per-platform verification is
deferred.** Confirmed by project owner. Write portable code under DEC-12's four rules and
keep the CI honest, but do not treat Windows or Linux as Phase 2 or Phase 3 deliverables.
They get built, run and fixed on their own platforms at a later date.

The CI position is already favourable and costs nothing to keep: 4.2 ships 34 workflow jobs
across `macos-latest`, `ubuntu-latest` and `windows-latest`. That is compile-level coverage
on all three from day one — it will not catch behavioural bugs, but it catches the build
rot that makes late porting expensive, which is exactly the failure mode DEC-12 guards
against.

**DEC-22 — macOS means Apple Silicon only. Intel Macs are not supported.** macOS Intel
reaches legacy status in September 2026, so x86_64 Mac support has no future worth paying
for.

*What this rules out:* universal binaries, `-arch x86_64` slices, Rosetta considerations,
and any fat-binary packaging step.

*Cost to implement:* none. The build system carries no universal or architecture flags to
remove — an ARM-only build is simply what happens when building on Apple Silicon, and the
Tcl/Tk 9.0.4 verification under DEC-13 already produced pure `arm64` binaries. This decision
is a *non*-requirement: it prevents someone adding universal2 flags later on the assumption
they were wanted.

*Note:* this constrains **macOS only**. Linux and Windows portability under DEC-21 is
architecture-agnostic; x86_64 remains entirely normal there.

**DEC-13 — Tcl/Tk 9.0.4 is built from the in-tree sources** ([tcl9.0.4/](../../tcl9.0.4/),
[tk9.0.4/](../../tk9.0.4/)), not linked from Homebrew. Reproducible, version-pinned and
self-contained for distribution.

> ✅ **Verified on Darwin ARM64** (Apple clang 21, `arm64-apple-darwin25.6.0`). Both build
> and install cleanly out-of-tree:
>
> - Tcl 9.0.4 — `libtcl9.0.dylib` and `tclsh9.0`, both `Mach-O 64-bit arm64`; reports
>   `tcl 9.0.4 / arm64 / Darwin`.
> - Tk 9.0.4 — `libtcl9tk9.0.dylib` and `wish9.0`, both `arm64`, built with
>   **`--enable-aqua`** so there is no XQuartz dependency. Version string confirms the
>   native path: `9.0.4+….aqua.clang-2100.objective-c`, linked against Cocoa, Carbon and
>   QuartzCore.
>
> **One build-order wrinkle to encode in Phase 3's build scripts:** Tcl's bundled-packages
> step (`configure-packages`) runs the freshly built `tclsh`, which is linked against its
> *install* path. On a clean tree that library does not exist yet, so the step fails with
> "cannot find a usable native Tcl 9 tclsh" even though the core compiled fine. Running
> `make install` first and then `make` again resolves it; `--disable-zipfs` or setting
> `TCLSH_NATIVE` are the documented alternatives. This is a bootstrap ordering issue, not
> an ARM incompatibility.

**DEC-14 — The Tk front end is a new `main-tcl.c` term backend**, registered alongside
`main-sdl2.c`, with the game retaining its own main loop. Richer Tcl-side UI grows
incrementally from that base.

> **Rationale revised after examining commontk.** DEC-14 was originally justified by
> rejecting the "game as a shared library driven by Tcl" model on the grounds that it would
> require reimplementing the C-side command surface. Examination shows that framing was
> wrong in two ways, though the decision stands:
>
> 1. **The original framework already owns `main()`.** `main-tnb.c:1095` defines
>    `int main(int argc, char **argv)` and embeds the interpreter; the DLL arrangement in
>    `src/common-dll` exists to let one launcher host several variants, not because Tcl had
>    to drive. The old model is therefore *closer* to DEC-14's choice than assumed.
> 2. **The command-surface rewrite is unavoidable under either model**, so it is not a
>    discriminator. The bridge exposes a hierarchical tree — `angband r_info info`,
>    `angband cave`, `angband inventory`, `angband keypress` and so on — whose leaves are
>    direct accessors onto *2.9.2-era internals*: `artifact_type`, `object_kind`,
>    `monster_race`, `monster_type`, `feature_type`, `object_type`. Every one of those
>    exists in 4.2 under a different name and shape. The 116 scripts call **175 distinct
>    command/subcommand pairs** against that surface.
>
> The practical split for Phase 3:
>
> - **Port largely as-is:** tile and icon engine, sound, map widget, event binding,
>   platform layer. This is game-agnostic rendering infrastructure and the most valuable
>   part.
> - **Rewrite:** `interp1.c` / `interp2.c` (~10,161 lines) against 4.2's data model. This is
>   the bulk of the Phase 3 work.
> - **Reuse in proportion:** the 116 `.tcl` scripts work to the extent the rewritten bridge
>   preserves command names. Keeping the old names where 4.2 has an equivalent concept is
>   therefore worth real effort — it is what converts script reuse from rewrite to
>   adaptation.
>
> **Tcl 9 portability looks favourable.** The code is heavily object-based — `Tcl_SetObjResult`
> ×98, `Tcl_CreateObjCommand` ×15 against `Tcl_CreateCommand` ×2, `Tk_OptionSpec` ×10 against
> `Tk_ConfigSpec` ×1 — which is the direction Tcl 9 requires. Only **four** files reach into
> private headers (`tkInt.h` / `tclInt.h`): `widget-iso.c`, `canv-widget.c`, `TclTk-dll.c`,
> `widget1-dll.c`. Those are the custom widget implementations and are the concentrated
> porting risk, since Tk's internals changed substantially between 8.3 and 9.0. Expect also
> the mechanical `int` → `Tcl_Size` sweep that Tcl 9 imposes across the board.

**DEC-15 — Phase 1 output is structured requirements docs** in this directory, one per
feature family, each requirement numbered and traceable to the source that motivated it,
with explicit exclusion lists.

**DEC-17 — ZangbandTK ships its own manual, written into 4.2's existing Sphinx tree.**
The game will differ from both Angband 4.2 and original Zangband in ways no existing
document describes — a wilderness that 4.2's manual has no concept of, 4.2 classes that
Zangband never had, and the balance decisions in DEC-08 through DEC-10 that match neither
ancestor. Shipping without a manual would leave players with two sets of documentation that
are each half wrong.

*Where it goes.* [docs/](../../docs/) is already a working Sphinx/reStructuredText project
with a `conf.py`, an `index.rst` and a readthedocs configuration, and it already carries
`attack.rst`, `birth.rst`, `command.rst`, `dungeon.rst`, `option.rst` and the rest. The
manual extends that tree rather than starting a new one — new chapters where Zangband adds
systems, edits where behaviour diverges.

*What it needs to cover, at minimum.* The wilderness and travel; multiple towns, buildings
and services; quests; the magic realms; mutations; virtues; pets; races and classes new to
this game; and an honest statement of how balance differs from both ancestors. Zangband's own manual chapter list — `general`, `birth`, `charattr`, `town`,
`dungeon`, `objects`, `monster`, `attack`, `defend`, `magic`, `command`, `commdesc`,
`option`, `pref`, `wizard`, `version` — is the reference for structure, and under DEC-16 its
text is a legitimate source to adapt.

*Timing.* Documentation is written alongside each feature family, not deferred to the end.
A Phase 2 milestone is not complete until its manual chapter is.

---

## Phase 2 status

[phase2-development-plan.md](phase2-development-plan.md) sequences all 90 requirements into
twelve milestones, M0 to M11. Playable from M1 onward. Realms are M9 and pets are M10 —
committed, scheduled late (DEC-19). Not yet estimated.

**Phase 3 has no plan document yet.** DEC-13 and DEC-14 record the toolchain verification
and the port/rewrite/reuse split, but the milestone sequencing equivalent to Phase 2 has not
been written. Best done once Phase 2 is underway, since the bridge rewrite targets a data
model M1–M11 are still changing.

## Phase 1 status

All four requirement documents are drafted:

- [phase1-balance-calibration.md](phase1-balance-calibration.md) — 17 requirements
- [phase1-world-and-towns.md](phase1-world-and-towns.md) — 25 requirements
- [phase1-player-systems.md](phase1-player-systems.md) — 28 requirements
- [phase1-content-and-flavour.md](phase1-content-and-flavour.md) — 16 requirements

**The documentation pass is complete** (M0). All 34 recoverable documents were read; only
`spoilers/life.txt` remains unarchived. It produced **fifteen** requirements that the
data-file and source analysis had entirely missed:

| Requirement | Finding |
|---|---|
| BAL-15…BAL-17 | Nightmare mode — an irreversible birth option and difficulty system |
| CNT-15 | The Ancient and Foul Curse, and its cascade |
| CNT-16 | Random object abilities at generation time |
| **CNT-17** | **Eldritch Horrors** — sanity blasting: INT/WIS drain, fear, confusion, amnesia, and mutation |
| PLR-29 | Allegiance is three-state — hostile, friendly, pet — not a boolean |
| **PLR-30** | **Pets cost mana upkeep** scaling with total pet levels |
| **PLR-31** | **Experience only for the killing blow** — pets earn the player nothing |
| PLR-32 | Summons inherit their summoner's allegiance |
| PLR-33 | Annoying a pet or friendly monster turns it hostile |
| PLR-34/35 | The documented paths by which mutations are gained and removed |
| PLR-36 | Beastmen: one mutation at birth, 20% per level |
| PLR-37 | Seven groups of mutually cancelling mutations |
| PLR-38 | Mutation probability weighted by race |

PLR-30 and PLR-31 are the most consequential: together they are the entire reason pets are
not overpowered, and neither is discoverable from the data files. Had M10 been built from
the pre-M0 requirements it would have shipped a broken game.

Upstream base pinned as git tag **`angband-base`** (`dc40ec9e0`), per DEC-11.
Tcl/Tk 9.0.4 verified on ARM64, per DEC-13.

## Scope and sizing

**DEC-19 — Scope and sizing, confirmed by project owner.**

| Question | Decision | Why |
|---|---|---|
| Magic realms (player Q2) | **Committed. Scheduled late, not conditional.** | Part of the concept. Not needed early, but it *is* in the plan — this is a sequencing decision, not a maybe. |
| Pets (player Q3) | **Committed. Scheduled late, not conditional.** | Wanted. Highest-risk item and cleanly separable, so it comes after the game is playable — but it is happening. |
| Wilderness size (world Q1) | ~~Start at roughly a quarter of Zangband's linear dimension~~ → **Zangband's own 129 blocks**, `wild:blocks` in constants.txt | Reversed once travel, caching and generation *were* proven, which is what the original said to wait for. The premise did not survive measurement either: the risk was never in the world map, which is six bytes a block — 129×129 is 100KB and three plasma fractals. It is in block *contents*, and those are generated on demand and never stored (world & towns §6). A quarter-size world bought nothing and put the map edge a few minutes' walk from the town, which the project owner hit in play. |
| Towns and dungeons (world Q2) | **Fewer and denser** than Zangband's 20 and 20 | A smaller world with distinct places beats a large one with interchangeable ones; WLD-14's per-dungeon character is what makes them worth visiting. |
| Monster catalogue (content Q1) | **Theme first** — import everything carrying Amber, Mythos or Chaos identity; take the generic tail only where it fills a gap | DEC-02's curation applied concretely. The identity content is what nobody else has. |
| Amber material (content Q2) | **Keep as-is** | Load-bearing to the spirit, mechanically free, trivially renameable later if it ever matters. |

**DEC-23 — Building quality is a generic attribute, not enumerated content.** Confirmed by
project owner. Zangband's 113 building types are 40 concepts times a quality ladder
(*Weapon Smiths → Advanced → Expert → Deep → Arcane → Unique*, and similar). Implement as
**type × quality level** so the ladder is generated, not hand-authored.

The ladder is kept rather than trimmed because it is what WLD-15's population/magic/law
scoring selects between — a lawful magical city drawing *Arcane Weapon Smiths* where a
frontier village draws plain *Weapon Smiths* is the whole payoff of that mechanism. Detail
and the three cost buckets in WLD-16a and WLD-16b.

**DEC-24 — Service buildings are selected on mechanical impact.** Confirmed by project
owner: only buildings with real gameplay consequence are worth implementing.

Checking [bldg.c](../../archive/zangband/src/bldg.c) shrank the candidate list before any
judgement was needed. Of the exotic building names in `t_info.txt`, only **eight** ever had
implemented behaviour — Library, Map Maker, Mutatalist, Zymurgist, Castle, Bazaar, Flea
Market and Grocer appear only in placement code and in the borg's tables, never in `bldg.c`.
Zangband routed unimplemented buildings through a Lua hook that its shipped scripts never
filled.

Of the eight real ones, **six are kept**: Magetower (inter-town teleport network), Enchant,
Recharge, Healer, the quest giver (which WLD-19 to WLD-22 require anyway), and the Inn —
confirmed by project owner, carrying the nightmare vision and giving a town somewhere to be
rather than only somewhere to shop.

**A seventh was added after the M0 documentation pass:** the mutation-removal service (the
"Chaos Tower", and the purpose of `t_info.txt`'s Mutatalist). `spoilers/mutation.txt` lists
it as one of only six ways to remove a mutation, which passes this decision's own
mechanical-impact test. It has no implementation in 2.7.5's `bldg.c`, so it must be written
rather than ported, and belongs in M8 alongside mutations. See the correction note in
WLD-16c.

**Two are cut:** the Casino, four gambling minigames whose outcome nothing else in the game
reads, and the Weaponmaster, whose hit and critical probability tables 4.2's object
descriptions already do better. Detail in WLD-16c.

> **On "committed but late".** Realms and pets are the two largest risks in the project and
> the two most characteristically Zangband features. Scheduling them after a playable game
> exists is a risk decision, not a hedge about whether they belong. Phase 2 carries a
> milestone for each. Nothing earlier may be designed in a way that forecloses them — in
> particular, the monster allegiance model of PLR-22 must be anticipated by anything
> touching monster handling, even before pets are built.

## Open

Nothing is currently blocked. All questions raised during Phase 1 planning are resolved or
explicitly scheduled below.

Assigned to Phase 2 **M0** (see [phase2-development-plan.md](phase2-development-plan.md)):

- **Read the remaining official documents against the requirements** (DEC-16). Four
  documents were read and three produced new requirements — a hit rate that suggests the
  other thirty are worth the same treatment. The highest-value unread ones are
  `docs/town.txt` (against world & towns), `docs/magic.txt` and the six available realm
  spoilers (against the realm decision), `spoilers/mutation.txt` (against PLR-13 to PLR-17
  and the question of which mutations matter), and `spoilers/chaospat.txt` (against PLR-05).

Deferred to their own platforms (DEC-21):

- Build, run and fix on **Linux/X11** and **Windows**. Both were supported by the original
  framework, so this is expected to be build-and-test rather than porting — but that
  expectation is unverified until someone runs it.
- macOS packaging. 4.2 ships `pkg_deb`, `pkg_src` and `pkg_win` in
  [scripts/](../../scripts/) but no macOS equivalent, so a `.app` bundling step is new work
  whenever distribution matters. Single-architecture under DEC-22, which simplifies it.

Verification work carried into Phase 2:

- Measure `struct chunk`'s memory footprint against the wilderness live-block target
  (world & towns Q4) — it shapes the core wilderness design.
- Recover the 2.8.1 → 4.2 `sleepiness` mapping (balance Q1).
- Confirm `obj_theme` can drive 4.2's object allocation (content Q4).
- Objects, artifacts and ego items still need the balance treatment given to the bestiary
  (balance §5).
