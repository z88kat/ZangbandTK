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
> not the site root. Note also that `web.archive.org` cannot be retrieved through WebFetch
> in this environment; `curl` works.
>
> **Re-checked in full 2026-09-01: all thirty-six citations, one at a time.** The result is
> simpler than the two accounts that preceded it, and both of those were wrong.
>
> *There is no parking page.* An earlier note here said zangband.org had expired by June
> 2022 and that the Wayback Machine forwards the 2022-05-27 capture to a squatter's
> placeholder. It does not: fetched with `id_`, that capture of the site root is the genuine
> zangband.org homepage, and the files that fail return *nothing at all* rather than
> somebody else's content.
>
> *`spoilers/life.txt` is not unarchived.* It was recorded here as returning zero bytes at
> every snapshot from 2005 to 2022. It returns **13,854 bytes at its own cited timestamp** —
> the complete Life realm spell list, four books of it. Nothing was ever wrong with that
> citation.
>
> *What is actually wrong is one crawl.* The 2022-04-20 crawl of `www.zangband.org` captured
> everything. The 2022-05-27 crawl of the bare domain captured the homepage and almost none
> of the files under it, so **every citation carrying the timestamp `20220527225941` for a
> file returns zero bytes** — twelve of them, all spoilers — while the same documents are
> whole at `20220420164xxx` under `www.`. All twelve have been repointed.
>
> **The method, and a trap in it.** Fetch with the `id_` modifier after the timestamp, which
> returns the archived bytes without the Wayback wrapper and without following redirects:
> `.../web/20220420164254id_/http://www.zangband.org/spoilers/mutation.txt`. Find candidates
> through the CDX index, and query it **domain-wide with a filter** rather than by exact URL
> — `?url=zangband.org&matchType=domain&filter=original:.*spoilers/mutation\.txt` — because
> the archive stores `www.` and `:80` variants that an exact-URL query silently misses; a
> plain query for `chaospat.txt` returns nothing at all while the file is in fact captured
> more than a hundred times.
>
> And **retry before concluding a document is gone.** A throttled fetch returns zero bytes
> and is indistinguishable from an absent one. A first pass over these thirty-six declared
> twenty-one dead; re-running the failures three times each with a pause reduced that to
> twelve. Nine documents were nearly rewritten out of the plan on the strength of
> archive.org being busy.

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

## Versioning

**DEC-28 — The version moves with the work, and lives in one place.**

`src/buildid.h` holds `VERSION_STRING`, and `Makefile.src` reads it out with `sed`. It was
written in both, each with a comment saying it must agree with the other, which is a
standing invitation to drift — and the number reaches the player from both directions:
the header feeds the title screen and the in-game copyright, the makefile feeds
`Info.plist` and so the About panel.

*The convention.* **Patch** for a fix or a small correction. **Minor** for a feature — a
requirement landing, or a milestone piece. **Major** only for something that changes what
the game is. Bumped in the same commit as the work, so that a screenshot or a release build
identifies itself without anyone having to ask which build it was.

Starting at **3.0.0**: Zangband's last release was 2.7.5-pre1 in 2005 and this line
continues from there, not from the Angband 4.2.6 the code sits on. `BASE_VERSION_STRING`
carries that separately so the About panel can say both.

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

**The documentation pass is complete** (M0). All 35 documents were read, and — corrected
2026-09-01 — **none of them is unarchived**, `spoilers/life.txt` included: it was recorded
as unrecoverable on the strength of a bad availability check and serves the whole Life
realm spell list at the timestamp already cited for it. The pass produced **fifteen**
requirements that the data-file and source analysis had entirely missed:

| Requirement | Finding |
|---|---|
| BAL-15…BAL-17 | Nightmare mode — an irreversible birth option and difficulty system |
| CNT-15 | The Ancient and Foul Curse, and its cascade |
| CNT-16 | Random object abilities at generation time |
| ~~**CNT-17**~~ | ~~**Eldritch Horrors** — sanity blasting~~ **Dropped by DEC-32.** The Mythos path is closed; the inn's nightmare is built without it. |
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

**DEC-29 — The starting village is the poorest place in the world, and that is the
scale everything else is read against.**

WLD-11a asks that towns differ in which stores they hold; WLD-12 asks that the
starting town be fixed rather than drawn. Both are satisfiable at once only if
the fixed set is a *small* one, and it took a survey of forty generated worlds to
see why.

*What the measurement showed.* With the starting village provisioned at six of
the eight shops, the ladder above it had nowhere to go. Making a town richer than
home meant seven shops, and a city eight — and with only eight shops in the game,
seven-of-eight and eight-of-eight leave at most one choice each. Across forty
worlds every single town held the identical seven, and every city the identical
eight. The size bands were real; the variation WLD-11a asks for was not there at
all.

*The decision.* The starting village holds four: general store, alchemist,
bookseller, home. That is the playability floor and nothing above it — food and
light, cure light wounds, somewhere to store a pack, and a spellbook, because no
class begins with one (a mage starts with a rapier and a torch). The armoury and
the weaponsmith move out of the starting village and become the first reason to
travel.

That buys room for a ladder with slack in it: village three or four trades, town
five or six, city six or seven, great city seven or eight, with the count inside
each band drawn from the block's own seed and the choice of trades scored against
population and law. The same forty-world survey then showed four distinct shop
sets among villages, three among towns and four among cities, with the black
market in about two great cities in five.

*The cost, recorded honestly.* A character can no longer buy armour or a better
weapon before the first dive. That is a real increase in early difficulty and it
was chosen, not stumbled into: it is what makes the wilderness a place you have a
reason to cross. If it plays badly, `WILD_VILLAGE_STORES` in [wild.c](../../src/wild.c)
is the one line to change.


**DEC-30 — The drift away from Amber is a defect to be removed, not a legacy to be kept.**

DEC-25 made Amber the anchor for taste: where Zangband's answer was weak, the
question became what serves Zelazny's *Chronicles of Amber*. This goes further,
and states it as the project's goal rather than as a tie-breaker.

*The problem.* Zangband began as an Amber game and did not stay one. Over its
development it accumulated Lovecraft, Tolkien, and a scattering of other
role-playing material, until the Amber spine was one theme among many rather
than the thing the game was about. The result is muddled: a world that is
nominally Shadow, walked by a nominal Amberite, populated by everything.

*The goal.* Bring it back. The Amber material is the game -- Amber the one true
city, the Courts of Chaos at the other pole, Shadow between them, the Pattern and
the Logrus, the Trumps, Oberon's quarrelling children. Everything else is
present on sufferance and has to earn its place against that.

*What this changes in practice.*

- **Imports are filtered, not merely prioritised.** DEC-19 puts theme first and
  DEC-25 gives Amber priority; this says the rest is a candidate for removal, not
  just for going last. When a monster, artifact, or building exists only because
  some other setting had one, the default is to drop it rather than to port it.
- **It applies retroactively.** Content already imported is in scope. The
  bestiary, the artifacts and the building list were brought across before this
  was stated, and should be read again against it.
- **It does not license invention without discipline.** DEC-18 still stands:
  facts get rigour. Filling a gap from the novels is legitimate; inventing Amber
  material that contradicts them is not, and neither is stripping a mechanic that
  works merely because its flavour is borrowed. Reskin before deleting where the
  mechanic is sound.
- **Angband's own inheritance is not the target.** DEC-27 keeps 4.2 as the base,
  and 4.2 is a Tolkien game at root. The aim is not to purge every non-Amber name
  from a codebase built on Angband -- it is that *ZangbandTK's own additions* pull
  towards Amber rather than away from it.

> Stated by the project owner: *"My goal is to bring ZangbandTK back to Roger
> Zelazny's Amber novels, the original concept but somehow got diverted from this
> goal mixing Lovecraft, Tolken, and probably bunch of other RPG's. It got a bit
> muddled over time. I would to turn this back toward the Amber novels."*

*The source.* Ten novels in two series -- the Corwin cycle (*Nine Princes in
Amber*, *The Guns of Avalon*, *Sign of the Unicorn*, *The Hand of Oberon*, *The
Courts of Chaos*) and the Merlin cycle (*Trumps of Doom*, *Blood of Amber*, *Sign
of Chaos*, *Knight of Shadows*, *Prince of Chaos*).
[The Chronicles of Amber](https://en.wikipedia.org/wiki/The_Chronicles_of_Amber)
is the summary to work from where the novels themselves are not to hand; it is
the reference the project owner pointed at, and it carries the cast, the
geography and the mechanics of Pattern, Logrus, Trump and Shadow-walking.


**DEC-31 — The dungeons are Amber's places, and a dungeon has a bottom.**

WLD-14 asks that dungeon entrances carry their own depth range and character.
Two decisions inside that were not settled by the requirement.

*What the dungeons are.* Zangband's twelve dungeon types are generic categories
-- lair, temple, tower, ruin, grave, cavern, mine, city, and so on -- with no
connection to Amber at all. Under DEC-30 that is drift, and the categories are
the least interesting thing about them. They are replaced by thirteen of
Zelazny's places, which cover the same mechanical ground and carry the story
instead of ignoring it: the Vaults of Amber, Arden, Faiella-Bionin, Garnath, the
Caverns of Kolvir, Rebma, the Grove of the Unicorn, Tir-na Nog'th, a Broken
Pattern, Thelbane, the Keep of the Four Worlds, the Courts of Chaos, and the
Abyss. The ladder runs from the cellars under the city out through Shadow to the
Courts and over the edge, which is the shape of the books.

*A dungeon ends.* Its deepest level is the deepest it has; there is no way down
from there and the game says so. To go further you leave, cross the world and
find a dungeon that reaches deeper.

The alternative was a soft ceiling -- descend past a dungeon's range and it stops
applying its own character -- which never blocks a character but makes the depth
ranges nearly meaningless. The hard floor was chosen because it is the only
version in which the world is load-bearing: without it, the wilderness is
scenery on the way to a staircase that would have worked anyway.

*What keeps that from being a wall.* Every dungeon in the game data gets a mouth
in every world, so the ladder has no gap -- rarity decides the order sites are
chosen in, never whether a dungeon exists. The ranges overlap generously (the
Vaults reach 15, Arden 20, Faiella-Bionin starts at 8), so there is always more
than one way onward. Most mouths open on or near a road. And the starting
village's staircase leads into the shallowest dungeon, so nothing has to be found
before the first descent. A test asserts that every depth from 1 to the deepest
is covered by some dungeon.

> Both decisions were put to the project owner, who took the hard floor and the
> village staircase.


**DEC-31 — Zangband's six town inhabitant types were three unused constants.
Four kinds are built instead, and the three it never wrote are dropped.**

WLD-11 calls Zangband's six kinds of townsfolk -- villager, elves, dwarf,
lizard, monster, abandoned -- "the reference taxonomy". Reading the source
before building it: `TOWN_MONST_ELVES`, `TOWN_MONST_DWARF` and
`TOWN_MONST_LIZARD` appear **exactly once each in the whole of Zangband**, in
their own `#define` in [wild.h](../../archive/zangband/src/wild.h#L61), and are
referenced nowhere else. `set_mon_wild_values()`
([wild1.c:3092](../../archive/zangband/src/wild1.c#L3092)) opens with *"This
function is very rudimentary at the moment"*, handles villager, abandoned and
monster, and carries the comment *"Add in other probabilities in here for the
other TOWN_MONST_XXX types"*. Every ordinary town it placed was a villager
town.

So the taxonomy was a plan, not a feature. This is the same shape as the Chaos
Tower under WLD-16c: documented or declared, never implemented, and the
requirement inherited the declaration rather than the behaviour.

*What is built.* Four kinds. The three Zangband gave behaviour to --
**villagers**, **monsters**, **abandoned** -- and **beasts** in place of the
three it did not: a town emptied once, with the animals moved back into it.

*Why beasts rather than the elves and dwarves.* Two reasons that agree.
Mechanically, 4.2 has no town-level elves, dwarves or lizardfolk to draw on --
the level-zero pool is 25 monsters, 19 of them one `townsfolk` base, with a
single elf that is a Christmas joke -- so those three kinds would need a
content import before they meant anything. And under DEC-30 they are precisely
the generic fantasy the game is being steered away from: a town of dwarves is
Tolkien furniture, where a shadow of Amber standing empty, or holding something
that should not be there, is what the novels are full of.

*Measured, and tuned once on the measurement.* Law decides it mostly, with
population second and a seeded roll for the empty ones. The first thresholds
gave 32% monster towns over twenty worlds, which means a third of the player's
shopping is a fight, and it took great cities as readily as hamlets. Scaling
the threshold by size band -- a great city holds out where a hamlet cannot --
gives villagers 62%, beasts 24%, monsters 10%, abandoned 5%, and leaves the
places that carry the magic shop and the black market standing.

*The starting village is always villagers*, by fiat, per WLD-12: the opening
must not depend on procedural luck, and beginning in a town held by monsters is
the worst luck the world could deal.

*Known gap, recorded rather than hidden.* A town held by monsters still trades.
Its shops are terrain with an action behind them, not people, so walking in
past the monsters works. Whether a taken town should refuse to sell is a
question for WLD-17 when the store system is next opened.


**DEC-32 — CNT-17 is dropped. The Mythos path is closed, and the nightmare does
not need it.**

CNT-17 asked for Zangband's sanity blasting: Eldritch Horrors that drain
intelligence and wisdom on sight, inflict fear, confusion and amnesia, and can
grant a mutation. Its stated rationale was that this is *"the mechanic that
makes the Mythos content of §2 mean something"*.

*Rejected.* DEC-30 points the game at Amber and calls the accumulated Lovecraft
drift a defect. A mechanic whose entire purpose is to make that drift matter
pulls the other way -- and building it would mean an insanity system, an
amnesia effect, a mutation trigger and a monster flag, all in service of content
the project is trying to reduce. That is the kitchen sink: adding everything
anyone can think of, in the hope that more is better.

> Decided by the project owner: *"I don't want to go down the Mythos path. It
> would just mean we throw the kitchen sink into the game adding everything we
> can think of. I like to keep it focused on the core objective of Amber. So i
> would not do the sanity blast of CNT-17. But we should keep the nightmare
> feature and add later."*

*And the positive case for it, which is stronger than the negative one and was
missing from DEC-30.* The Mythos is not merely off-theme here, it is **common**.
Lovecraft was what everybody was reaching for in the nineties, and the ground has
been worked over by every roguelike, board game and shooter since; a Zangband
that leans on it is a Zangband competing on the most crowded pitch there is.
Amber is the opposite: it is the one thing Zangband had that nobody else has
built on, before or since.

> *"people were very much into lovecraft and the Mythos world back in the 90s
> its already been done to death and does not make this game unique. My guess
> they just added that in the game as it was interesting at the time. I think
> what makes zangband unique is leaning on the Amber series of novels which has
> not been done elsewhere."*

That reframes the whole of DEC-30. The reason to cut the drift is not tidiness
and not fidelity for its own sake -- it is that the drift is the part of the game
anyone could have written, and Amber is the part nobody else did.

*What this costs, stated honestly.* The Mythos monsters are already imported --
DEC-02's "theme first" curation took everything carrying Amber, Mythos or Chaos
identity, and the bestiary holds Nyarlathotep and Azathoth among the rest.
Without CNT-17 they are large monsters with evocative names, which is precisely
the criticism CNT-17 was written to answer. That is accepted rather than solved:
under DEC-30 the answer is fewer of them, not a mechanic to justify them. Whether
to actually remove any is a content pass nobody has scheduled, and this decision
does not order one.

*The nightmare survives, decoupled.* Zangband reached its sanity blast through
the inn's bed, so the two arrived together and looked like one feature. They are
not. A night's sleep that shows you something is Zelazny's own furniture --
Tir-na Nog'th is a city seen only by moonlight and is already a dungeon here --
and it can be built out of what 4.2 already has, without an insanity system.
Recorded as a follow-up below, with the constraint that it must not reintroduce
CNT-17 by the back door.


**DEC-33 — WLD-16b's seventeen new shops are none. The variation belongs to the
quality ladder, not to a longer list of doors.**

WLD-16b divided Zangband's building types by implementation cost and put roughly
seventeen in a bucket marked *"new shops -- data"*: Jeweler, Fletcher, Swordsman,
Shieldsman, Axeman, Milliner, Statue Store, Figurine Store, Clothes Store, Ammo
Supplies, Supplies Store, Warrior Hall, Heavy Armoury, Scroll Store, Potion
Store, Magic Store, Rare Book Store.

Checked one at a time against 4.2's actual stock lists in
[store.txt](../../lib/gamedata/store.txt), and against Zangband's own building
list in [t_info.txt](../../archive/zangband/lib/edit/t_info.txt), **none of them
survives.**

| Zangband shop | What became of it |
|---|---|
| Swordsman, Axeman, Shieldsman, Milliner, Clothes Store, Heavy Armoury, Warrior Hall | Strict subsets of 4.2's **Armoury** and **Weaponsmith**, which already stock swords, polearms, hafted, shields, helms, crowns, gloves, boots, cloaks, soft, hard and dragon armour |
| Fletcher, Ammo Supplies | The Weaponsmith already stocks `bow`, `shot`, `arrow` and `bolt`. A fletcher would sell a subset of one shelf |
| Potion Store, Scroll Store, Magic Store | Subsets of the **Alchemist** and **Magic shop** |
| Rare Book Store | The **Bookseller** stocks each realm's town book; anything deeper is what the **Black Market** is for, and it already stocks anything |
| Supplies Store, Grocer, Flea Market | Subsets of the **General Store** |
| Jeweler (+ Copper, Silver, Gold, Rare) | 4.2 has **no mundane jewellery**. Every ring and amulet in the game is a significant magic item, so a jeweller would be a second Black Market -- and DEC-29 made the Black Market the reason to travel |
| Statue Store, Figurine Store | Need `TV_STATUE` and `TV_FIGURINE`, which 4.2 does not have. Content invention, not porting |
| Temple (+ Large, High, Hidden) | A 2.8.1 shop 4.2 removed on purpose. DEC-27: where Zangband merely looks like 2.8.1, 4.2 wins |
| Library | WLD-16b already established it has no implementation anywhere in Zangband |

*So WLD-16b is closed by finishing its own job.* It asked for a classification by
implementation cost; the classification says the cost is nil, because the work is
not wanted. Seventeen shops selling narrower selections of goods eight shops
already carry is the kitchen sink again -- the same instinct DEC-32 refused in
the Mythos, in a different coat. A great city with twenty-five doors is not a
richer city, it is a longer walk.

*What this hands to WLD-16a.* All of it. The variation Zangband got from 113
building entries has to come from **building type x quality** applied to the
eight that exist -- an *Arcane Weapon Smith* in a lawful magical city where a
frontier village has a plain one. That was already WLD-16a's argument; this
decision removes the alternative, so the ladder is now the only thing standing
between eight shops and eight identical shops.

*And to WLD-17.* Nothing. It asked that new store types extend
[store.txt](../../lib/gamedata/store.txt) rather than replace it, which is still
the right instruction and now has nothing to instruct. It is met by the quality
ladder using the same file.

*What is deliberately not concluded.* That Zangband was wrong to have them. It
was built on 2.8.1, whose Armoury and Weaponsmith were far thinner; subdividing
them added real choice at the time. 4.2 spent twenty-five years merging that
choice back into fewer, better-stocked shops, and this project sits on 4.2.


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

Outstanding follow-ups raised while building, rather than while planning:

- ~~**The inn's nightmare** (WLD-16c).~~ **Built, as PLR-41.** Both directions
  below were taken: a bad night on a failed save, and a true dream that reveals a
  place. Weighted by the town's law, so a frontier inn gives nightmares and a
  lawful city gives visions. The constraint held -- no insanity, no amnesia, no
  mutation trigger; the failed save spends 4.2's own fear and confusion. What
  follows is the original note, kept because the reasoning is what shaped it.

  The bed is built; the dream is not. It is
  no longer blocked on anything: DEC-32 drops CNT-17, so the nightmare is to be
  built out of what 4.2 already has rather than on top of a sanity system.
  Zangband's version picked a monster from the deepest part of the bestiary and
  rolled a saving throw against its hit dice; that shape is reusable, and 4.2's
  timed effects will carry the consequence.

  Two directions, either or both, for whoever takes it:

  - **A bad night.** A failed save leaves the character afraid, confused or
    hallucinating for a while — 4.2 has all three already. The dream names what
    it showed, so the message does the work the mechanic would otherwise have to.
  - **A true dream.** More interesting and more Zelazny: the dream shows you
    somewhere real. Reveal a dungeon mouth or a town on the world map that the
    character has not found. The Pattern and the Trumps both work by showing
    people places; an inn in Shadow is a fair place for that to happen, and it
    turns a night's rest into something worth paying for beyond the daylight.

  The constraint from DEC-32: no insanity, no amnesia, no mutation trigger. If a
  design needs those, it is CNT-17 wearing a hat.

**DEC-34 — The quality ladder is scored on magic, because magic is the only axis
a town has not already been chosen for. Home and the Black Market are exempt.**

WLD-15 asks for buildings placed "by suitability in the same parameter space as
terrain", which Zangband scored on population, magic and law. Our block map had
population and law and no magic, so the obvious reading is that magic was the
missing piece and adding it was a formality. It was not a formality: it turned out
to be the *only* axis with anything left to say.

Measured over 479 towns in 40 worlds, at the block each town stands on:

| Axis | min | p10 | median | p90 | max | mean |
|---|---:|---:|---:|---:|---:|---:|
| law | 104 | 165 | 213 | 242 | 254 | 208 |
| population | 5 | 34 | 139 | 226 | 252 | 135 |
| magic | 0 | 22 | 130 | 221 | 253 | 125 |

Law is not a free variable at a town: WLD-08a *sites* towns on it, so by the time
there is a town to score, law has already been selected for and reads as a
constant offset with a little noise. Population survives as an axis because the
size bands are cut from it and villages are real. Magic is very close to uniform,
because nothing anywhere selects for it — which is precisely why it discriminates.

So the ladder leans on magic and population, with law contributing little, and
that is a feature rather than a compromise: law decided *whether there is a town
at all*, and asking it a second question would only get the same answer back.

**The exemptions.** Home has no stock to be better. The Black Market is already
the top of every ladder — it stocks anything, at a level keyed to how deep the
player has been — so a tier on top of it is a second opinion about the same
thing, and DEC-29 already made it the reason to travel. The starting village is
plain by WLD-12: a character who happens to begin next to an arcane weaponsmith
is playing a different game from one who does not, and neither of them chose it.

**Thresholds are measured, not chosen.** Guessed first, and the guess put a
quarter of every shop in the world on the top rung, with the tiers coming out in
the wrong order — everything above the highest cutoff piles into it. Taken from
the 60th, 85th and 97th centiles of the measured score distribution instead, the
ladder runs 70 / 18 / 8 / 2 per cent, so the top rung is one shop in fifty and
worth walking to find.

**DEC-35 — Nine of Zangband's twenty-one races, curated on character rather than
imported wholesale. And the mapping between the two games is measured, not guessed.**

PLR-01 asks for the 21 races 4.2 does not have. Taking all of them would repeat the
mistake DEC-32 refused for the Mythos: four are undead needing a mechanism 4.2 has
not got, and several are close variants of races already present — Half-Ogre,
Half-Giant and Cyclops all sit beside Half-Troll; Dark-Elf beside Elf; Nibelung
beside Dwarf.

Nine are in, chosen for having a mechanical identity of their own or Amber's:

| Race | Why |
|---|---|
| Amberite | The flagship. Amber's own blood, and `expfact` 225 says so |
| Beastman | Born mutated, which is what M8 is about |
| Yeek | Zangband's own joke, and inseparable from it |
| Draconian | A breath weapon, and 250 to be one |
| Mindflayer | Psionics — pairs with the Mindcrafter (PLR-06) |
| Vampire | Immune to dark, hurt by light: a bargain rather than a bonus |
| Golem | Not living. Nothing gets in, and the cold gets to it |
| Sprite | Flies |
| Half-Titan | 255, the top of the ladder |

*Four are rejected outright*, on the evidence rather than on taste. Asked which of
the twelve were worth having, the numbers answered for these:

| Rejected | Because |
|---|---|
| Half-Ogre | Half-Troll (`+4 -4 -2 -4 +3`, hd 12) with dark resistance: `+3 -1 -1 -1 +3`, hd 12 |
| Half-Giant | Half-Troll with shard resistance and one more hit die |
| Cyclops | Half-Troll with sound resistance |
| Dark-Elf | An Elf with more INT and DEX and see-invisible, at 150 instead of 120 |

Three of the four are the same race with the resistance swapped, and the fourth
is a dearer Elf. Adding them lengthens the birth menu — which already scrolls on a
24-line terminal — without adding a decision to make. Recorded in the manual under
"Races we decided against", because *it exists in the original* is not on its own
a reason to carry something, and the next person to notice they are missing
deserves the reasoning rather than a gap.

*Eight are deferred, not rejected*: Barbarian, Klackon, Nibelung, Imp, Skeleton,
Zombie, Spectre, Ghoul. Three of those have a real case — Barbarian is strong
*and* dextrous, which nothing else here is; Klackon is innately fast; Nibelung
carries the only disenchantment resistance in the game. The four undead want one
mechanism between them and should arrive together when it exists.

*One correction to the original filing.* Barbarian was first grouped with the
big-and-strong variants and should not have been: at `+3 -2 -1 +1 +2` it has
positive dexterity, which no other strong race does. Grouping by impression rather
than by the numbers is exactly what the measurement was supposed to prevent.

*The mapping was measured.* Ten races appear in both games, which makes the
question answerable rather than a matter of taste:

| Field | Finding | What was done |
|---|---|---|
| stats, hit dice | identical in 8 of 10 | copied |
| disarm, device, save, stealth | identical in 7 to 9 of 10 | copied |
| search | differs with no pattern; 4.2 redesigned it race by race | copied |
| melee, shoot | 4.2 widened the spread, roughly double | doubled |
| experience | 4.2 flattened nearly everything to 120; Zangband ran 100 to 255 | Zangband's kept |

The experience factor is the one that matters and the one PLR-01 already argued
for: 4.2 treats it as a formality and Zangband treated it as the balance dial.
Keeping Zangband's is what makes a Half-Titan slow to level and an Amberite dear.

Charisma is dropped throughout — Zangband has six stats and 4.2 has five.

*What is not yet expressible.* Zangband gated several racial gifts on level: the
Draconian's breath changes every five levels, the Sprite quickens at nine, the
Yeek's acid resistance becomes immunity at nineteen. A 4.2 race is a flat set of
flags, so those wait for PLR-02, which is the mechanism for exactly this.

### DEC-36 — The Monk, and a class mapping that was measured (PLR-03/PLR-04)

Zangband has five classes 4.2 does not. The Monk is the first, taken because its
identity is entirely self-contained — no realms, no patrons, nothing waiting on
another milestone.

*The mapping was measured*, the same way PLR-01's was. Six classes appear in both
games (Warrior, Mage, Priest, Rogue, Ranger, Paladin), which makes the conversion
answerable rather than a matter of taste:

| Field | Finding | What was done |
|---|---|---|
| stats, hit dice | close, and 4.2 retuned a few | copied |
| disarm base, device base, save base | identical in 5 of 6 | copied |
| device *incr* | identical in 6 of 6 | copied |
| save *incr* | 4.2 runs 0.41–0.44 of Zangband's, very tight | × 0.42 |
| search base | 4.2 runs exactly 0.62 in 5 of 6 | × 0.62 |
| search incr | Zangband's `x_srh` is 0 for every class; 4.2 uses 12–16 | no source — class-typical value |
| melee base | 4.2 runs ~3.1× | × 3.1 |
| melee incr | 4.2 runs ~0.56× | × 0.56 |
| shoot base | 4.2 runs ~3.45×, and varies widely | × 3.45 |
| shoot incr | 4.2 runs ~0.94× | copied |
| disarm-magic | 4.2 split one Zangband skill in two; equal to phys in 2 of 6 | copied from phys |
| throw | equal to shoot in 4 of 6 | copied from shoot |
| experience | 4.2 leaves it at **0** for all nine of its classes; Zangband ran 0–40 | Zangband's kept |

The two clean results are worth noting: `c_srh → search` lands on 0.62 in five of
six, and `x_dev → device incr` is 1.00 in six of six. Those are not coincidences —
they are 4.2 having deliberately rescaled one skill and left the other alone.

*The experience factor is kept*, which makes the Monk the only class in the game
that costs anything. That is the same call PLR-01 made for races and for the same
reason: 4.2 treats the field as a formality, Zangband used it as the balance dial,
and a Monk that levels as fast as a Warrior is not paying for eight unarmed blows.

*Charisma is dropped*, as it is throughout.

*What the Monk does not yet have.* In Zangband it casts, choosing between Life and
Nature. Realm selection is PLR-08, two milestones out, so it ships as a pure
martial class; adding a `magic:` block later takes nothing away from it.

> **Discharged in 3.65.0**, and the sentence above was one realm short.
> `realm_choices1[]` gives the Monk **Life, Nature *and* Death**, and
> `realm_choices2[]` gives it `CH_NONE` — so it picks one of three and has no
> second slot, which is the High-Mage's shape narrowed to three realms. It casts
> on its own row of `magic_info[]`, so DEC-55's donor rule never applies to it.
> Recorded here rather than by editing the paragraph, because what DEC-36
> decided is untouched and only its deferral is spent. See the addendum below
> for what building it turned up.

*One defect found while building it.* PLR-02's racial powers read their cost from
spell points only, and `calc_mana()` returns a maximum of zero for any class with
no spellbooks — Warrior, Rogue, Blackguard, and now Monk. A Draconian Warrior
could never once have breathed. Zangband's own answer is that a character short of
mana pays in hit points, which is both the fix and the reason the feature works at
all for the classes that need it most.

### DEC-37 — Psionics are a power list, and psionic force is a new projection (PLR-06)

The Mindcrafter is the second Zangband class taken, chosen because its identity
needs nothing from another milestone. Two decisions came out of building it.

*Psionics are not a realm, and the code says so.* PLR-06 asks for "a power list
independent of the realm system", and the temptation was to express the twelve
powers as a bookless realm because 4.2's spell machinery already has level, mana,
failure and effects. That was rejected. A realm in 4.2 implies books, and books
imply things to find, lose and choose between — none of which a Mindcrafter has.
So the class carries a power list of the same kind PLR-02 gave races, and
`powers:WIS:1` names the stat that feeds it. `calc_mana()` grew a branch for
this: mana is normally derived from the realms a class's books belong to, and a
class with no books had a maximum of zero.

*Powers gained level bands.* Zangband's powers are rarely one thing — Precognition
detects monsters at 2, finds traps at 5, sees invisibility at 15, maps at 20,
grants telepathy from 25 to 39, detects everything at 30 and lights the level at
45. 4.2's effect chain has no notion of when a link applies, so `power-when` was
added: a group of effects and the level range it is good for. Eight of the twelve
powers need it. This applies to racial powers too, and is where a Draconian's
breath changing every five levels can eventually go — the gap PLR-01 recorded as
"not yet expressible".

*Psionic force is a new projection.* Every damaging type 4.2 has is an element,
resisted by a flag or by armour. Zangband's `GF_PSI` is not: it asks whether
there is a mind to hurt. That is the Mindcrafter's whole character, so
`PROJ_MON_PSI` was added rather than approximated with mana or force damage.
`EMPTY_MIND` is complete immunity and `WEIRD_MIND` or `STUPID` takes a third,
both read off flags 4.2 already keeps for telepathy — which is the same question
asked the other way round. `MON_DRAIN`, which spares the nonliving, is the
existing pattern it was written against.

*What was approximated.* Zangband's Psychic Drain returns energy to the caster on
a hit; 4.2's effect system has no way for an effect to give the player a turn
back, so it is damage only for now. Psychometry is a full identify at every
level rather than pseudo-identification below 25, because 4.2 removed
pseudo-identification.

### DEC-38 — A Chaos-Warrior serves a Lord of the Courts, not Khorne (PLR-05)

Zangband's sixteen Chaos-Warrior patrons are Slortar, Mabelode, Chardros,
Hionhurn, Xiombarg, Pyaray, Balaan, Arioch, Eequor, Narjhan, Balo and Khaine --
Moorcock's Elric gods -- plus Khorne, Slaanesh, Nurgle and Tzeentch, who are
Warhammer. Not one has any connection to Amber. Under DEC-30 this is exactly the
drift the project exists to undo, and it is far cheaper to decide before
importing sixteen names than after.

*What was chosen.* Named Lords of the Courts of Chaos, over three alternatives:

- **The Houses** (Sawall, Hendrake, Helgram, Chanicut, Minobee) were rejected
  because they are already the lawless town names. A patron and a town with the
  same name is a collision unless the link is made deliberate, which is a
  separate design question and not this one.
- **The Pattern and the Logrus alone** are the most canonically exact answer --
  both are sentient and both compete for a person's allegiance in the Merlin
  cycle -- but two patrons is thin at birth, where the whole point of the
  mechanic is that a new character feels different.
- **Both poles**, swearing to Order or Chaos, is the most Amber option of all and
  is a larger class than PLR-05 describes. Left open: nothing here forecloses it.

A named Lord wins because Zangband's mechanic is a *capricious personal
relationship*. A person can be pleased, bored or offended; a House or a
Principle cannot, and the reward and punishment messages are half of what the
class is.

*The roster*, nine Lords, verified against the character list rather than
recalled:

| Lord | House | What they are |
|---|---|---|
| Swayvill | -- | King of Chaos, dying slowly of Eric's curse |
| Suhuy | Sawall | Keeper of the Logrus; taught Merlin and Mandor |
| Mandor | Sawall | Machiavellian manipulator; Merlin's stepbrother |
| Dara | Helgram | Great-granddaughter of Benedict, Merlin's mother |
| Gramble | Sawall | Rim Duke, ally of the King |
| Jurt | Sawall | Violent and jealous, Merlin's half-brother |
| Despil | Sawall | Jurt's brother, who avoids a fight |
| Borel | Hendrake | Duke and master of arms; Dara's fencing instructor |
| Gilva | Hendrake | Warrior-maiden |

Nine rather than sixteen. Four -- Mandor, Jurt, Borel and the Serpent -- are
already uniques in the bestiary, so a character can be gifted by a patron it may
later meet and kill.

*Two names I nearly used and did not.* Tmer, Tubble and Bances are not in the
reference, and were dropped rather than written in from recollection: DEC-18
says facts get rigour, and a patron roster is a fact. Two others were corrected
by checking -- Suhuy is of House Sawall and Keeper of the Logrus, not Hendrake,
and Dara is of House Helgram, not Sawall.

*Polymorph uses 4.2's shapechanges rather than a ported do_poly_self().*
Zangband's version permanently mangles the character -- it changes race and
layers on mutations, irreversibly. 4.2 already has a shapechange system with
eight forms, driven by an effect that takes a shape name as data, so a patron's
"Thou needst a new form, mortal!" costs no new code at all. It also has better
mechanical bite for the same flavour: gear merges into the body, so every
equipment bonus and all casting go until the character changes back, and the
change back is always one keystroke away -- any blocked action offers it -- so a
patron cannot strand anyone.

*Mutations are a deliberate gap until M8.* One roll in six replaces a patron's
reward with a mutation, and PLR-13 to PLR-17 do not exist yet. That branch is
left as a hole rather than substituted with something else, because a
substitute would have to be removed again. **When M8 lands, gain_level_reward's
mutation branch must be filled in** -- it is listed against PLR-13 in the M8
milestone for that reason.

*The tension worth keeping.* The Serpent of Chaos is already the endgame. A
warrior sworn to a Lord of the Courts who must finally kill the Serpent is not
an inconsistency to be designed away -- it is the shape of the Merlin cycle, and
the patron's reaction to that ending is a thing the class can eventually say
something about.

### DEC-39 — Virtues are kept, and the consumer is ours to invent (PLR-18 to PLR-21)

Confirmed by project owner: *"it's a good feature that never made it to code, so
let's take that up."*

*What the source says, and it is worse than the documentation suggested.* PLR-21
was written as a gate -- if nothing consumes virtues, cut them rather than ship
inert numbers -- on the assumption that Zangband had consumers to restore. It had
none. Read against [avatar.c](../../archive/zangband/src/avatar.c) and the rest of
2.7.5-pre1:

| Question | Answer in the source |
|---|---|
| Writers | **168** `chg_virtue()` call sites across 18 files -- `effects.c` 32, `xtra2.c` 20, `cmd5.c` 19, `spells1.c` 13 |
| Readers | **None.** Outside `avatar.c` the arrays are touched only by birth ([birth.c:1628](../../archive/zangband/src/birth.c#L1628)), the savefile ([load.c:1431](../../archive/zangband/src/load.c#L1431), [save.c:1014](../../archive/zangband/src/save.c#L1014)), three display paths, one writer ([xtra2.c:93](../../archive/zangband/src/xtra2.c#L93)), and a Lua binding |
| Player-visible | The knowledge-menu entry is **commented out** -- *"Display virtues option is always left out"* ([cmd4.c:4214](../../archive/zangband/src/cmd4.c#L4214)) |
| Lua | `l-player.pkg:234` exposes the arrays; no shipped script reads them, the same pattern DEC-24 found with the buildings |

Nothing in the game ever branched on a virtue value. The feature was accounting
with the display switched off, which also explains why no official document covers
it: by the time the manuals were written there was nothing to see. There is no
virtue spoiler among DEC-16's 34 documents.

*All three consumers PLR-21 named are wrong.* The requirement said Zangband ties
virtues to "Chaos-Warrior patron behaviour and some artifact and spell outcomes".
`gain_level_reward()` ([xtra2.c:3093](../../archive/zangband/src/xtra2.c#L3093),
478 lines) reads player level, the `TR_PATRON` flag, the patron index and chance,
and contains no reference to a virtue or to alignment. `artifact.c` and the
`spells*.c` files appear in the virtue grep only as *writers*. The documentation
was accurate where the requirement was not: `docs/charattr.txt` says the reward
depends on "the Patron Demon ... and chance", and `spoilers/chaospat.txt` gives the
full selection procedure with no virtue term.

*Not to be confused with alignment.* Zangband does have a consumed moral axis and
it is not this one. `p_ptr->align` is set from the alignment of the player's pets
([xtra1.c:3338](../../archive/zangband/src/xtra1.c#L3338)) and read to keep summons
from bringing in their enemies
([monster2.c:2384](../../archive/zangband/src/monster2.c#L2384)). That belongs to
M10, not here.

*The decision.* Keep virtues, and take the feature further than Zangband did. Two
consequences to accept in the open:

1. **Any consumer we add is new design under DEC-30, not restoration.** There is
   nothing to port. That does not make it illegitimate -- DEC-30's test is whether
   it serves the character of the thing -- but it must not be written up as
   fidelity work, and BAL-08's habit of checking the consuming code before trusting
   a number has nothing to check here.
2. **The gate in PLR-21 still binds.** Keeping virtues is a commitment to building
   a consumer, not a ruling that the gate no longer applies. If no consumer is
   built, PLR-18 to PLR-20 still come out.

*What is worth keeping exactly as it was.* The one part of the original that did
work is the birth-time selection. A character carries **eight** virtues, not
eighteen (`MAX_PLAYER_VIRTUES` 8 against `MAX_VIRTUE` 18,
[defines.h:4942](../../archive/zangband/src/defines.h#L4942)), chosen by class,
then race, then each realm, deduplicated, then padded with weighted-random draws
(`get_virtues()`). A Chaos-Warrior gets Chance and Individualism; a Monk gets
Faith, Harmony, Temperance and Patience; an Amberite gets Honour; a Yeek gets
Sacrifice. Which virtues a character has is itself part of who they are, it costs
a birth-time table, and PLR-18 has been corrected to say so.

*One name corrected.* Slot 8's constant is `V_ENCHANT`
([defines.h:4930](../../archive/zangband/src/defines.h#L4930)) but the string a
player saw is **Mysticism** ([avatar.c:30](../../archive/zangband/src/avatar.c#L30)).
The requirement document had taken the constant name.

**Open: which system consumes virtues.** Deliberately not settled here -- it is a
design choice rather than a finding, and both candidates are cheap enough that it
can wait for M8 to start. Recorded so it is not rediscovered:

- **The patron ladder.** `patron_roll_slot()` reads only player level today. A
  virtue term biasing the roll fits the fiction exactly -- a Lord that watches how
  its servant behaves -- and is a few lines. Chaos-Warriors only, so it satisfies
  the gate for one class in twelve.
- **The inn dream.** `player_night_dream()` (PLR-41) already derives two chances
  from a parameter and is available to every character. A virtue term on the true-
  and dark-dream chances is the same shape, universal, and sits well with DEC-33's
  reading of the dream as a Trump-like vision.

Whichever lands decides how many of the eighteen need to move at all. Eighteen
counters with accounting on every kill, spell and item is most of the cost, and a
consumer that reads four of them does not justify the other fourteen.

Verification work carried into Phase 2 — three of the four are now done:

- ~~Measure `struct chunk`'s memory footprint against the wilderness live-block target
  (world & towns Q4).~~ **Done**, on the real structures with a probe compiled against
  the game's headers — phase1-world-and-towns §6.
- ~~Recover the 2.8.1 → 4.2 `sleepiness` mapping (balance Q1).~~ **Done**, and recorded
  as DEC-40.
- ~~Confirm `obj_theme` can drive 4.2's object allocation (content Q4).~~ **Done**:
  `obj_theme_here()` selects the theme and `obj_theme_weight()` biases the roll, in
  [obj-make.c](../../src/obj-make.c) (CNT-12).
- **Still open.** Objects, artifacts and ego items need the balance treatment given to
  the bestiary (balance §5). Importing them did not do it: CNT-06, CNT-07 and CNT-11
  brought all three across at Zangband's own numbers, with no curve fit and no lethality
  scalar. Spot-checked as not visibly out of line — the deepest imported weapon deals
  less than Angband's own — but not measured.

---

### DEC-40 — The sleepiness mapping is recovered, not assumed (BAL-07)

BAL-07 said `sleep` must be *mapped* to 4.2's `sleepiness` rather than copied,
on the grounds that the two scales might differ, and left the recovery method
as balance open question 1: *"BAL-07 assumes it is derivable from the 434
shared monsters. Needs verifying before it is relied upon."*

**It was derivable, it has been derived, and the conversion has been relying on
it since M2.** `derive_sleep_mapping()` in
[tools/zconv/rules.py](../../tools/zconv/rules.py) observes every
(2.8.1 `sleep`, 4.2 `sleepiness`) pair across the monsters both versions carry
and builds the mapping from what it finds. The question is closed; this entry
records the answer and what it cost.

*What the data says.* 434 shared monsters yield 21 distinct 2.8.1 sleep values.
Six of those map to exactly one 4.2 value and are taken as observed. The other
fifteen map to several, because 4.2 retuned individual monsters without
retuning the scale — 2.8.1's `10` appears in 4.2 as 5, 10, 13, 14, 20, 30, 40
and more. For those the median is taken.

*Why the median is the right choice here, and not merely the convenient one.*
For **13 of the 21** values the recovered figure is the source figure — the
median of what 4.2 did with `sleep:20` is 20 — and **275 of the 434** shared
monsters carry the same number in both versions. The two scales are the same
scale. 4.2 did not rescale sleepiness; it edited monsters. So the mapping
recovers an identity with noise on top, and the median is what strips the noise
without inventing a curve. Five values move: 1→10, 3→6, 25→58, 30→45, 90→50,
each from a handful of observations.

*What this does not settle.* The five that move are recovered from few
monsters, and a Zangband monster landing on one of them gets a number with
little behind it. The conversion report names the count on every run, so the
figure is visible rather than buried.

**Consequence.** Balance open question 1 is answered and struck. BAL-07 is met.

---

### DEC-41 — The Scroll of Rumour is dropped (CNT-11)

Confirmed by project owner: *"Drop the scroll of rumours, adds nothing."*

Recorded as a rejection rather than a deferral, so that it is not picked up
again by someone reading the deferred list as a queue. It sits in
`objmap.toml`'s `[reject]` beside the artifact bases, not in `[defer]` beside
the statues.

*The mechanism was never the obstacle.* Zangband's 647 rumours are in the
archive at [lib/file/rumors.txt](../../archive/zangband/lib/file/rumors.txt),
and 4.2's `hints.txt` is already the same shape — a flat list of one-line
strings parsed into a linked list. A `rumor.txt` beside it and an effect that
picks one is a morning's work.

*The content was.* A good many of the lines name things this game does not have
or contradict what it does: an Amulet of Doom that is now an Amulet of
Destruction, wresting one last charge from an empty wand, which 4.2 removed,
monsters that were never imported. A scroll whose whole purpose is to tell the
player something true about the game is the wrong place to ship text that is
false about it — so shipping the list wholesale was never an option, and the
alternative is 647 individual judgements for one flavour scroll.

**Consequence.** CNT-11's deferred list drops from 16 to 15. The scroll is not
in the game and is not waiting for anything. If Zangband's rumours are ever
wanted, they are wanted as new writing rather than as an import.

---

### DEC-42 — The Incandescent Globe of Sawall is dropped (CNT-06)

Confirmed by project owner: *"Drop it."* — the name collision is not worth
resolving for one artifact.

Recorded as a rejection rather than a deferral, and for the same reason as
DEC-41: the deferred list reads as a queue and this is not in one. It sits in
`renames.toml`'s `[artifact_collision]` with the ruling written down, and the
converter reports it under *Skipped* beside the other refusals.

*It is a real artifact, not a fragment.* Zangband has two called "of Sawall":

| | Index 7 | Index 33 |
|---|---|---|
| What | Incandescent Globe (a light) | Hard Leather Cap (a helm) |
| Depth / cost | 35, 4,000 | 52, 10,000 |
| Powers | +3 infravision, a fire aura, an electric aura, light | +3 INT/WIS/CHR, sustains all three, resist blindness, +18 AC |

Neither is a draft of the other. Both auras the Globe carries were built in the
M2/M3 flag work, so it would have converted intact and would have been the only
Zangband artifact light in a game that has three.

*Why they collide, and why only here.* Zangband saved an artifact by **index**
([save.c:605](../../archive/zangband/src/save.c#L605)) and displayed it as base
object plus suffix, so "The Incandescent Globe of Sawall" and "The Hard Leather
Cap of Sawall" were unambiguous on screen and on disk. 4.2 writes the suffix
alone into the savefile and reads it back with `lookup_artifact_name`, which
returns the first exact match — two of one name means a character can come back
holding the other. Angband 2.8.1 has no duplicate artifact names at all;
Zangband introduced the only two in the line, and the other pair ("of the
Dwarves") is inherited from 2.8.1 and never an import candidate.

Some sign it was a slip on Zangband's part rather than a design: its other
place-names take one artifact each, "of Amber" and "of Chaos" among them.
Sawall is the only House with two.

*A rename was available and was declined.* Sawall is a House of the Courts of
Chaos, and three of the nine patrons in
[patron.txt](../../lib/gamedata/patron.txt) belong to it — **Mandor**,
**Gramble** and **Despil** — so the Globe could have taken a name from the
established roster without inventing a word. The owner's judgement was that one
artifact does not justify the question. Recorded here so that the option is
visible to anyone who revisits it, rather than looking like an oversight.

*Why the Cap is the one kept.* Originally by accident: a name-keyed read let
the later record overwrite the earlier. It stays kept on purpose now, because
it is the one already shipped and renaming a shipped artifact makes every
savefile holding it refuse to load
([load.c:149](../../src/load.c#L149)). On merit the Globe is arguably the
better survivor — the Cap lost CHR and SUST_CHR in conversion, 4.2 having no
charisma — but that is not worth a broken savefile.

**Consequence.** The artifact count stays at 51. CNT-06 has nothing outstanding.

---

### DEC-43 — The virtue writers are sized by coverage, not by census (PLR-20)

Zangband has **169** places that move a virtue. Porting all of them was one
option and porting only what the consumers read was the other, and this is
neither: the rule is **every virtue a character can be given must be one their
play can move**, and the writers are sized to that and stopped there.

*Why not all 169.* They are scattered across nineteen files of a codebase
organised differently from this one, and most would be guesswork about which
4.2 path corresponds to which Zangband one. The cost is enormous and the return
is a slightly denser accounting of numbers that two consumers read four at a
time.

*Why not only what the consumers read.* The two consumers between them name
eight virtues — Chance, Individualism, Harmony and Temperance for the patron,
Enlightenment, Knowledge, Unlife and Chance for the dream. But a character is
measured against **eight of eighteen**, chosen from their class, race and
realm, and nothing guarantees any overlap. A Warrior asked about Valour,
Honour, Justice and four more would have carried eight counters that nothing in
the game could move and nothing in the game would read: the exact condition
Zangband shipped in, reached by a different road.

*The rule, and what it cost.* All eighteen are reachable in this game — the
class, race and realm tables plus the weighted pad between them can produce any
of them, which `all-eighteen-are-reachable` pins. So all eighteen need a writer,
and there are **nine hook sites** covering them:

| Site | Virtues |
|---|---|
| A monster dies (`virtue_note_kill`) | Valour, Harmony, Unlife, Vitality, Individualism, Compassion, Faith |
| Striking something asleep | Compassion, Honour |
| A chaotic weapon discharges | Chance |
| Tunnelling, and clearing trees | Diligence, Nature |
| Haste begins | Patience, Diligence |
| Invulnerability begins | Temperance, Honour, Sacrifice, Valour |
| Enchanting an item | Mysticism |
| Mapping a level | Knowledge, Enlightenment |
| Collecting a bounty | Justice |

Nine sites for eighteen virtues, against Zangband's 169 for the same eighteen.
Every one is Zangband's own hook at a place 4.2 has the same event; none is
invented.

*The one substitution, and it is marked.* Zangband's death block asks three
times whether the dead thing was `GOOD`. 4.2 has no such flag — only `EVIL` —
and it is not recoverable: Zangband put `GOOD` on 230 monsters, a set running
from Farmer Maggot to Mughash the Kobold Lord that 4.2 does not record.
`monster_is_living()` stands in on the unique branches only, because `GOOD` was
on a quarter of the bestiary and living is on nearly all of it; the non-unique
branch is dropped rather than substituted. Without something there, Unlife
would have had no writer that raises it and a Necromancer would carry a counter
that only ever fell.

**Consequence.** The accounting is coarser than Zangband's and covers the same
ground. If a third consumer arrives wanting a virtue nobody moves much, the
answer is another hook rather than another 160.

---

### DEC-44 — Mutations are grouped by behaviour, not by bitfield (PLR-13)

PLR-13 names three groups, from §2.4: activatable, behavioural/random, and
permanent physical. Those are Zangband's `MUT1`, `MUT2` and `MUT3`, and they
are a **storage layout** — thirty-two flags is what fits in a `u32b`, and
nothing else is meant by the division.

`spoilers/mutation.txt` describes **four** kinds, and they are what a player
meets: activatable, randomly activating, continuous, and *melee-attack*. The
five melee mutations — scorpion tail, horns, beak, trunk, tentacles — live in
`MUT2` beside the random effects and behave nothing like them: they are extra
blows in the attack round, not things that fire on their own timer.

**Four kinds are implemented**, the spoiler's. Coverage is identical either
way — all 96 arrive — so this changes only how they are grouped in the data
file, on the character sheet, and in the code that runs them. DEC-16 makes the
documentation a first-class source for intent, and PLR-13 names its groups by
behaviour rather than by word boundary, so the spoiler is the better reading of
what that requirement is about.

PLR-37 settles it independently: its seven cancelling groups include "beak /
trunk", so the melee mutations are in scope and are treated as a set already.

---

### DEC-45 — The mutation regeneration penalty is not ported (PLR-13)

`spoilers/mutation.txt` opens its effects section with a warning:

> Zangband 2.2.2d introduced a penalty for having too many mutations. The
> first few are free but thereafter your regeneration rate is reduced as
> further mutations are added. Beastmen are largely exempted from this but,
> strangely, Chaos Warriors are not.

**It is not in 2.7.5-pre1.** `count_mutations()` has exactly two callers in the
whole source, both of them prerequisite checks inside the selection switch
([mutation.c:374](../../archive/zangband/src/mutation.c#L374) and
[mutation.c:462](../../archive/zangband/src/mutation.c#L462)), and nothing in
`process_world()` scales `regen_amount` by a mutation count. The spoiler
documents a mechanic that Zangband added in 2.2.2d and had removed again by the
version this project ports.

*Why that settles it.* DEC-20 makes the source the authority on algorithm and
the documentation the authority on intent, and here they disagree about
**existence** rather than detail. Building it would not be finishing an
unfinished feature, which is what this milestone is for; it would be
reinstating one its authors reverted. The standing discipline in every
comparable case — a flag with no mechanism, an object with no effect — has been
to decline rather than invent.

*And mutations are not free without it.* Two costs survive in 2.7.5 and are
ported:

- **Food.** Resilience and rock-eating each add 20% to what the character
  burns; the wasting disease takes 50% off, which is not a kindness
  ([dungeon.c:1334](../../archive/zangband/src/dungeon.c#L1334)).
- **The roll itself.** Twelve of the 32 continuous mutations are simply bad —
  puny, moronic, albino, rotting flesh, a silly voice, a blank face, short
  legs, arthritis, bad luck, elemental vulnerability, extra noise, extra fat —
  and nothing lets a character choose. Mutation is a gamble, and that is the
  balance.

**Consequence.** If play shows mutations are too cheap, the penalty is a small
thing to add later and this entry says where it came from. It is not being
skipped for cost.

### DEC-46 — The Chaos Tower is a great city's alone, and it is the only route that lets you choose (DEC-24, PLR-13)

DEC-24 kept the mutation-removal building on the grounds that
`spoilers/mutation.txt` lists it as one of only six ways to lose a mutation. Building
it raised two questions Zangband cannot answer, because Zangband never
implemented it: it names the Mutatalist in `t_info.txt`, routes it through a Lua
hook, and ships no script to fill the hook.

*Great cities only, and 2500 gold.* The figure is set against what it competes
with rather than ported. A potion of New Life removes every mutation at once,
is worth 50000 gold and generates at depth 100 — so a single removal has to
cost enough that the potion stays the thing a player hopes for, and little
enough that a mid-game character with one intolerable mutation has a way out
that is not "find the rarest potion in the game". Restricting it to the largest
town band does the same work from the other side: a character with something
they cannot live with should have to cross the world for it. A Chaos Tower in
every town would make mutations an inconvenience rather than a thing you live
with.

*It is the only route that lets the player choose.* Every other removal path in
the spoiler takes whichever mutation it likes — the potion takes all of them,
"strangely normal" and Polymorph Self take at random. Choosing is the whole
reason to walk to a building and pay for one removal what a potion does
wholesale, and it is what makes the building worth having rather than a slower
potion.

### DEC-47 — Polymorph Self keeps the mutations and drops the race change (PLR-34)

Zangband's `do_poly_self()` does four things at once: it can change the
character's sex, change their race outright, drain stats, and shed and add
mutations. Only the last survives here.

The ruling is DEC-38's, reached the first time this code came up. A Chaos
patron's "Thou needst a new form, mortal!" is the same function, and DEC-38 took
4.2's shapechange over Zangband's permanent race mangling — because 4.2 already
has a shapechange system driven by data, because merging gear into the body has
better mechanical bite than a changed race, and because the change back is
always one keystroke away so a patron cannot strand anyone. The same reasoning
applies to the same code reached through the mutation instead of the patron, and
a port that answered the question differently depending on which caller found it
would be worse than either answer.

What is kept is the gate structure, because that is what the mutation is for:
both the shedding loop and the gaining loop run against `power > randint0(N)`
with `power` the character's level, so a level-40 character can lose several
mutations and gain several more in one invocation and a level-5 character
usually gets nothing at all. Polymorph Self should be frightening late and
merely odd early, and it is.

### DEC-48 — The Midas touch is dropped (PLR-16) — **REVERSED by DEC-52**

**This decision was reversed on 1 September 2026. It is kept in full rather than
deleted, because the reason it gives turned out to be false and that is worth
being able to read.** The mutation is in the game; see DEC-52 below for what
changed and why. Everything from here to the end of this entry is the original
decision as it stood.

Confirmed by project owner: too much effort for something that is not a game
changer.

Recorded as a rejection rather than a deferral, so that it is not picked up
again by someone reading the deferred list as a queue. It sits in
`mutmap.toml` under `reject` rather than `defer` — a distinction that key pair
did not have until this decision needed it — and the conversion report lists it
under REJECTED rather than among the deferred.

*What it would have taken.* Zangband's `alchemy()`
([mutation.c:1110](../../archive/zangband/src/mutation.c#L1110)) turns a chosen
object into gold at a fraction of its value. **4.2 has no effect that destroys
an object for money**, so the work is not one effect handler: it is deciding
what fraction, of what valuation, and what that does to the shops that price
against the same numbers. That is an economy mechanic, and it would exist to
carry one mutation.

*Why the mutation does not earn it.* It is a convenience rather than a power. It
turns loot you were going to sell into gold without the walk back to town —
which the game already lets you do, by walking back to town. Nothing else in
the game wants an object-to-gold effect, so the mechanic would have exactly one
consumer and no prospect of a second.

**Consequence** *(as recorded at the time; superseded by DEC-52, which reversed
this decision and brought the mutation back)*. Twelve mutations still do
nothing, but the split is now eleven deferred and one refused. The activatable "not yet" list drops from eight to
seven; the Midas touch is listed in the power menu as **dropped** and says so
when chosen, because "not yet" is a promise and this is not one the game
intends to keep. The mutation is still gained, described and saved like any
other — a mutation that vanishes from a savefile is worse than one that does
nothing, which is the same reasoning DEC-44 applied to the two charisma
mutations.

### DEC-49 — Seven realms, by content and not by name (PLR-09, PLR-10, PLR-12)

PLR-09 asks for seven realms and names them: Life, Sorcery, Nature, Chaos,
Death, Trump, Arcane. PLR-10 asks that 4.2's realm metadata be retained rather
than replaced. 4.2 has four realms — arcane, divine, nature, shadow. Those two
requirements do not compose on their own, because four plus seven is eleven and
two of the names collide.

*What was chosen.* 4.2's four are mapped onto four of the seven by content:

| 4.2 | becomes |
|---|---|
| `arcane` | **Arcane** |
| `divine` | **Life** |
| `nature` | **Nature** |
| `shadow` | **Death** |

and Sorcery, Chaos and Trump are added. That is exactly seven, keeps every
scrap of 4.2's metadata — the casting stat, verb, spell noun and book noun,
which Zangband held as scattered per-class constants and expressed less well —
and costs sixteen `book:` lines, because realms are not written to the savefile.
Books are matched by tval and sval and the realm is read for display and for
`calc_mana()`, so renaming a record is free.

*The one that is not a clean match, and is recorded as such.* **This game's
Arcane is not Zangband's Arcane.** Zangband's was deliberately the weak
generalist: "it has no ultra-powerful high level spells... all Arcane
spellbooks can be bought in town"
([arcane.txt](https://web.archive.org/web/20220420164306/http://www.zangband.org/spoilers/arcane.txt)).
4.2's arcane realm is the Mage's, with two books of attacks and a dungeon-only
Wizard's Tome of Power. Folding them keeps the stronger reading.

The alternative was two realms sharing one name, which PLR-09's count forbids;
the other alternative was renaming 4.2's arcane to Sorcery, which is worse,
because Sorcery has no direct attack spells at all and 4.2's arcane opens with
Magic Missile. A stronger Arcane is a balance note for BAL-15 to BAL-17 to pick
up. It is not a contradiction.

*What this decision does not do.* It does not give any existing casting class a
realm *choice*. Zangband's Priest picks Life or Death; 4.2's Priest has a fixed
list. Opening that up means adding books to a class's book list, and
`player->spell_flags[]` is indexed by a flat position across every book the
class carries and is written to the savefile by that index — so inserting a book
anywhere but the end shifts every spell a saved character knows one place along.
The game would load and the character sheet would look reasonable. Existing
classes therefore keep their exact progressions, pinned by `player/realm`, and
the choice mechanism is built so that a class can be opened up in data later
with a savefile migration written on purpose.

---

### DEC-50 — All seven realms take Zangband's spells, and old savefiles break (CNT-10, PLR-09, PLR-12)

**Decided by the project owner, 1 September 2026: Reading B.**

CNT-10 says *"Realm spellbooks are imported for whichever realms PLR-09
delivers"*, and PLR-09 delivers seven. DEC-49 mapped four of those seven onto
realms that already carry Angband 4.2's own content — Arcane, Life, Nature and
Death are 4.2's `arcane`, `divine`, `nature` and `shadow` renamed — and PLR-12
requires that existing class progressions not be disturbed. The two cannot both
be taken literally, and the two readings were costed:

- **Reading A** — import Sorcery, Chaos and Trump only. 96 spells, no savefile
  risk, and four of seven realms holding Angband's spells rather than the ones
  a Zangband player remembers.
- **Reading B** — import all seven, replacing 4.2's content in the four mapped
  realms. ~224 spells, and every casting class's savefile invalidated.

**Reading B is taken.** The reason given is that the game is pre-release and
still in development, so a savefile break is affordable now in a way it will not
be later; and CNT-10 read plainly asks for Zangband's spells, which is what the
project is for. Asked directly whether old characters failing to load was
acceptable, the answer was *"if we cannot load the savefile, so be it."*

**What breaks, measured rather than estimated.** `player->spell_flags[]` and
`spell_order[]` are indexed by a flat position across all of a class's books and
written to the savefile by that index
([save.c:785](../../src/save.c#L785)), so replacing a book's spell list changes
what every index means. Eight classes carry a `magic:` block: Mage, Druid,
Priest, Necromancer, Paladin, Rogue, Ranger, Blackguard. Of the 35 characters in
`tests/saves`, **four are casting classes** — one Mage, one Druid, one
Necromancer, one Blackguard. The other 31 are Warriors, Monks, a Mindcrafter and
a Chaos-Warrior, none of which has a spell list to invalidate. The corpus loses
four files, not all of it.

**The break must be loud.** This is the part that is not a matter of taste. A
saved Priest whose book contents changed underneath it will load *successfully*
with the right number of known spells and the wrong spells — the character sheet
looks reasonable and the game plays wrong. That is worse than a refusal, and it
is what the savefile work of 30 August was about. So the import bumps the
`player spells` block to version 2, and the version 1 reader refuses rather than
guesses. An old character then fails to load with a message that says why, which
is the outcome the owner accepted; it is not licence to let one load
incorrectly.

**Not foreclosed by this.** A name-based migration from the v1 block remains
writable later if it is ever wanted — mapping old indices to new by spell name.
Refusing now is a choice about effort, not a door closed.

**This reopens nothing in DEC-49.** The note there that Reading B would weaken
the argument for mapping 4.2's arcane onto Arcane was written before the
decision; the mapping is by *realm identity*, not by whose spells sit in it, and
seven realms remain seven whoever wrote their contents.

---

### DEC-51 — Spell experience is imported per-book, as Zangband has it (CNT-10, BAL-08)

**Decided by the project owner, 1 September 2026: import as derived, which was
the recommendation below.** `sexp = 5 * book²` — 5, 20, 45, 80 — set once per
book and applied to all ~224 imported spells. It follows DEC-50: if the spells
are Zangband's, so is the scheme that rewards them.

The per-book divergence against 4.2's own curve is recorded and *not* corrected
at import. It goes to balance calibration as BAL-13's kind of question rather
than a conversion one, and the two values worth watching in playtest are
**Sorcery book 3** (5.62× 4.2's median for the same spell levels, because the
book spans levels 3 to 25) and **Trump book 2** (0.33×). The reasoning that led
here is kept below, including the figure it corrects.

---

#### How that was arrived at

**Smaller than it was first reported. The framing below corrects an earlier
claim of mine.**

The conversion itself is exact and is not in question (see the Phase 2 rules
recorded in the M9 section). Zangband awards first-cast experience as
`5 * book² * slevel` where `book` is 1 to 4
([cmd5.c:2826](../../archive/zangband/src/cmd5.c#L2826), whose own comment reads
"Experience: 5, 20, 45, or 80 * spell level"), and 4.2 awards
`sexp * slevel` ([player-spell.c:546](../../src/player-spell.c#L546)). The
structures are identical, so `sexp = 5 * book²` — 5, 20, 45, 80 — transfers the
mechanism exactly.

**What was reported first, and was wrong.** This was flagged as an
eight-fold discrepancy, on the grounds that 4.2's `sexp` runs 2 to 10 against
Zangband's 5 to 80. **4.2's `sexp` runs 2 to 300.** The earlier figure came from
looking only at low-level spells, where 4.2's values genuinely are 2 to 10; the
value rises steeply with spell level. Median `sexp` by level band across the 163
spells 4.2 ships:

| Spell level | 1–10 | 11–20 | 21–30 | 31–40 | 41–50 |
|---|---|---|---|---|---|
| median `sexp` | 4 | 10 | 25 | 115 | 200 |

**What the comparison actually shows.** Zangband's derived values against 4.2's
median at the same spell levels, for the Mage's three new realms:

| Realm | Book | Levels | Zangband | 4.2 median | Ratio |
|---|---|---|---|---|---|
| Sorcery | 1 | 1–7 | 5 | 4 | 1.25 |
| Sorcery | 2 | 9–33 | 20 | 16 | 1.25 |
| Sorcery | 3 | 3–25 | 45 | 8 | **5.62** |
| Sorcery | 4 | 10–45 | 80 | 25 | 3.20 |
| Chaos | 1 | 1–15 | 5 | 5 | 1.00 |
| Chaos | 2 | 17–35 | 20 | 20 | 1.00 |
| Chaos | 3 | 11–47 | 45 | 50 | 0.90 |
| Chaos | 4 | 20–49 | 80 | 115 | 0.70 |
| Trump | 1 | 1–17 | 5 | 5 | 1.00 |
| Trump | 2 | 20–42 | 20 | 60 | **0.33** |
| Trump | 3 | 15–45 | 45 | 50 | 0.90 |
| Trump | 4 | 30–49 | 80 | 140 | 0.57 |

So the scales are **broadly comparable and cluster on 1.0**, and the divergence
runs in *both* directions — 0.33 to 5.62. There is no systematic gap to correct.
The cause of the spread is structural: Zangband ties experience to which *book*
a spell is in, and 4.2 ties it to the *spell*, so a cheap spell in a late book
is over-rewarded and an expensive spell in an early book is under-rewarded.
Sorcery book 3 spans levels 3 to 25, which is why it is the worst offender.

**So the question is narrower than "faithful or scaled".** It is whether to
import Zangband's per-book value as it stands, or to derive `sexp` from the
spell's own level against 4.2's curve. The precedent favours neither
automatically:

- **BAL-05** rescales imported armour class by 9/8, because the two games roll
  against it differently — a rescale to make the *mechanism* agree.
- **BAL-13** applies measured global scalars to every monster, 4.2's included —
  a rescale for *balance*, exposed as a tunable constant in `constants.txt`.
- **DEC-40** recovered 4.2's sleepiness scale by observing 434 shared monsters
  rather than assuming a factor — a rescale *derived from evidence*.

None of the three is quite this case: the mechanism already agrees, so BAL-05's
reasoning does not apply, and there is no shared population to derive a mapping
from the way DEC-40 had. The nearest fit is BAL-13's: treat it as a balance dial
rather than a conversion question, import Zangband's values, and note the
per-book divergence for BAL-15 to BAL-17 to pick up during playtest.

**Recommendation, offered rather than taken:** import as derived, record the
table above in the conversion report so the outliers are visible, and leave it
to balance calibration. The two outliers worth watching are Sorcery book 3
(over-rewarding) and Trump book 2 (under-rewarding). This blocks nothing —
whichever way it goes, the converter emits a number and only the number
changes.

---

## Reversals, and what is open

*Nothing is awaiting the project owner.* DEC-50 and DEC-51 were decided on
1 September 2026 and are recorded above, and DEC-52 below reverses DEC-48 —
which is kept above in full rather than deleted, so that the rejection, the
reason it was wrong, and the reversal can be read in order.

### DEC-52 — The Midas touch comes back, and the mechanic is built once (DEC-48, CNT-10, PLR-16)

**Decided by the project owner, 1 September 2026: build the object-to-gold
mechanic and reinstate the mutation.**

DEC-48 dropped the Midas touch, on the project owner's call that it was too
much effort for something that is not a game changer. The recorded reasoning
turned on one specific claim:

> *Nothing else in the game wants an object-to-gold effect, so the mechanic
> would have exactly one consumer and no prospect of a second.*

**That claim is now false.** Importing the Sorcery realm turns up `alchemy()`
with three callers in Zangband, not one:

| Caller | What it is |
|---|---|
| [mutation.c:1112](../../archive/zangband/src/mutation.c#L1112) | the Midas touch — the one DEC-48 refused |
| [cmd5.c:881](../../archive/zangband/src/cmd5.c#L881) | **Sorcery spell 30, *Alchemy***, at level 42 for a Mage |
| [artifact.c:743](../../archive/zangband/src/artifact.c#L743) | a random-artifact activation, priced at 10,000 |

So the mechanic has three consumers, one of which is a realm spell inside the
milestone being built now. DEC-48's decision may still be the right one — "not
a game changer" was a judgement about the mutation and is untouched by this —
but the *argument* for it no longer holds, and that is not something to quietly
route around.

**What was chosen: build it, once, for all three.** The same reasoning FETCH
gets — a mechanic with more than one consumer is designed against all of them
rather than against whichever arrives first. `ALCHEMY` takes no parameters,
because none of Zangband's three callers passes any.

*The numbers are Zangband's, from the source rather than the name.*
[spells3.c:1489](../../archive/zangband/src/spells3.c#L1489):

- **A third of the object's real value.** `object_value_real()`, which is what
  4.2 prices a sale with — not the shop price and not what the character
  believes the object is worth. An unidentified ring of speed pays what a ring
  of speed is worth, which is generous, and is what Zangband does.
- **Divided before the quantity multiplies it.** `price /= 3` comes first, so
  ten objects worth two gold each pay *nothing* rather than six. That is
  integer division doing what it does, and reproducing it matters more than
  tidying it.
- **Capped at 30,000.** One act cannot make a character rich.
- **Artifacts refuse**, via what Zangband guards with
  `can_player_destroy_object()`: one-of-a-kind items are not currency.
- **A worthless object becomes fool's gold** — destroyed, nothing paid. Not a
  way to tidy a pack for free.

*One deliberate divergence.* Zangband multiplies in a signed 32-bit value and
caps afterwards, so a stack valuable enough wraps negative and pays nothing. It
takes a cost above about sixty-five million to reach and Zangband's own data
does not contain one, so the bug is latent there rather than live — but a cap
that can be jumped is not a cap, so the multiply is widened here. Found by a
test written to pin the cap, which then failed on it.

**Consequences, since the plan has been wrong about this count more than once:**

- The Midas touch is **not** dropped, **not** deferred, and **not** among the
  mutations that do nothing. It is a working activatable power.
- **Eleven** mutations do nothing, all of them deferred; **none** is refused.
  The activatable split is **25 working, 7 deferred**.
- The `unavailable:rejected` field and the power menu's *dropped* label survive
  with no user. They are kept rather than removed, because the distinction they
  draw — a power waiting on a mechanism against one considered and turned down —
  is one this game will need again, and `player/mutation` asserts the refused
  count is zero so that a rejection reappearing has to be looked at.

*What this does not change.* DEC-48's judgement that the Midas touch is "not a
game changer" was about the mutation and is untouched; it is a convenience that
saves a walk to town. What changed is the cost side of the trade. The mechanic
is no longer being built to carry one convenience — it carries a realm spell as
well, and an artifact activation Zangband priced at 10,000.

---

### DEC-53 — Chaos's three missing elements, and the radius that cannot grow (CNT-10)

**Decision.** Chaos is imported whole — all thirty-two spells, none deferred —
by mapping three Zangband damage types 4.2 does not have onto types it does, and
by freezing five radii that Zangband grew with the caster's level.

Chaos is the realm the spoiler calls "the very element of unmaking", and its
spells are almost all direct attacks. That makes it the realm most exposed to
the one place Zangband and 4.2 genuinely differ in kind: the list of things a
projection can be. 4.2 has twenty-five elements and thirty-one other
projections, and Zangband's `hurt_types[30]` contains four that have no
counterpart at all.

*The three that Chaos actually uses:*

- **Disintegration** (`GF_DISINTEGRATE`) did two things at once: it damaged
  everything, resisted by nothing, and it deleted walls. 4.2 keeps those apart —
  `KILL_WALL` deletes walls, and `MANA` is the damage nothing resists. So the
  two spells that used it are treated differently by what they cost.
  *Fist of Force* is 8 mana at level 20 and becomes `BALL:FORCE`, keeping the
  damage and losing the walls, because a cheap early spell is not the one that
  should be reshaping the dungeon. It gains stunning, which disintegration did
  not have. *Disintegrate* is 40 mana at level 35 and becomes a two-effect
  chain, `BALL:KILL_WALL` then `BALL:MANA`, keeping both halves at the cost of
  striking monsters with two projections rather than one.
- **Rocket** (`GF_ROCKET`) becomes `SHARD`. Zangband's rocket does great damage
  that little resists and cuts what survives; shards is 4.2's element that cuts.
  Shard resistance now helps against *Magic Rocket*, where almost nothing helped
  before.
- **Nuke** (`GF_NUKE`, poison-radiation) appears only inside *Call the Void* and
  becomes `POIS`, which is the half of it 4.2 has.

`GF_HELL_FIRE` and `GF_HOLY_FIRE` are the fourth and fifth with no counterpart,
and no Chaos spell casts them directly — they reach the player only through
*Call Chaos*, which is separately reduced (below).

*The radius rule.* 4.2's `radius` is a literal in the data file, not an
expression, so a radius that grows with level cannot be carried over at all.
Rather than choose five numbers five ways, one rule is applied everywhere:
**take the value at the level the earliest-entitled class learns the spell**,
which is a Mage for all five. *Flash of Light* 1, *Mana Burst* 2, *Sonic Boom*
4, *Invoke Logrus* 6, *Disintegrate* 3. Every one of them is right at the level
you first cast it and smaller than Zangband's later on, which is the direction
to err in: the spell is never stronger than it was.

*Three further reductions, recorded with the content in `realmmap.toml`:*

- ***Call Chaos*** rolled one of thirty damage types and then one of three
  shapes. Thirty types is not something the data language can hold, so the three
  shapes are kept and given one type each. The roll still decides what happens;
  what it no longer decides is the element.
- ***Call the Void*** punished you for casting it next to a wall — it destroyed
  the area and took 100 to 250 hit points off you. The effect language has no
  test on the terrain around the caster, so the spell can no longer do that.
  This is the spell's defining risk and it is gone; saying so plainly is the
  whole of what can be done about it.
- ***Summon Demon*** gave a one-in-three chance the demon served you. 4.2 has no
  pets and no monster allegiance, so every demon it calls is hostile.

*And one found match rather than a translation.* 4.2's `WONDER` effect handler
is Zangband's *Wonder* table band for band, down to the `plev / 5` added to the
roll and the comment explaining why. Two of twenty-two bands differ in dice.
That is shared ancestry showing through, and the spell is simply `effect:WONDER`.

**What this costs.** Chaos is the first realm imported without a single
deferral, and that is not the same as importing it unchanged. Six of its
thirty-two spells do something measurably different from what Zangband's did,
and the difference is written next to each one in `realmmap.toml` rather than
summarised here and forgotten.

---

### DEC-54 — Trump is deferred whole, and an empty realm is no longer offered (CNT-10, PLR-09, PLR-35)

**Decision.** Trump's thirty-two spells are **not** imported. The realm keeps its
record in `realm.txt` and its place in Zangband's entitlement table, and it has
no books. Separately and immediately: **a realm with no books is no longer
offered at birth**, which is a defect fix that outlives this decision.

*Why the whole realm rather than the summons.* Fifteen of Trump's thirty-two
spells are summons — *Trump Animal*, *Phantasmal Servant*, *Trump Monster*,
*Conjure Elemental*, *Joker Card*, *Trump Spiders*, *Trump Reptiles*, *Trump
Hounds*, *Trump Cyberdemon*, *Trump Undead*, *Trump Dragon*, *Mass Trump*,
*Trump Demon*, *Trump Ancient Dragon*, *Trump Greater Undead* — and in Zangband
every one of them turns on a variable called `pet`. Read
[cmd5.c:2049](../../archive/zangband/src/cmd5.c#L2049) and the shape is the same
fourteen times: succeed and the creature is yours; fail and an angry one appears
in a group. The spell's whole content is which of those two you get.

Angband 4.2 has no monster allegiance. Every monster is hostile, `SUMMON` has no
side to take, and there is no field on a monster to put one in. Imported as-is,
all fifteen would collapse onto the failure branch: *Trump Cyberdemon* at level
40 would be a spell that spends 60 mana to place a hostile Cyberdemon next to
you. That is not a weaker version of the spell. It is the spell inverted, and a
book full of them is a trap rather than a realm.

*Why not import the other seventeen.* Trump would then be four books with
*Five Aces*, the deepest, six-eighths blank, and the blank half is what the
spoiler names as the realm's identity — "an equally impressive selection of
summoning spells". The seventeen that work are real and good, and they are
mostly teleportation, which the game already has. A realm that is half a hole
and whose remaining half duplicates Sorcery's is worse than a realm that is
honestly absent. Sorcery's four deferrals are four out of thirty-two; this would
be fifteen.

**What would unblock it.** Monster allegiance: a side on `struct monster`, an AI
that respects it, a `SUMMON` that can take one, and the pet-death and
pet-management behaviour that comes with it. That is a milestone of its own, not
a translation choice, and it is what PLR-35's *Shuffle* and the Phantasmal
Servant both sit behind. Nothing else about Trump is hard — *Shuffle* is a
120-entry random table that `RANDOM` and `SELECT` can express, and *Living
Trump* is a `GAIN_MUTATION` away.

**The defect this exposed, which is not about Trump.** Zangband's entitlement
table is imported whole and correctly says a Mage may take Trump. This game
gains a realm's content one commit at a time, so between commits there is a
window where a class is entitled to a realm with nothing in it — and the birth
step offered it. Choose it and the character is made, the realm is fixed, and
the spell menu is empty for the rest of that character's life with no way back.
It was live for Sorcery between 3.54.2 and 3.55.0 and nobody noticed.

`player_realm_choices()` now skips a realm the class has no book in. The
entitlement table is untouched — it is the record of what Zangband permits and
should stay that way — and the guard sits between it and the player.
`player/realm` asserts both halves: every realm offered has a book behind it,
and every realm the class has a book in is offered somewhere. The second half is
what stops the guard from being satisfied by offering nothing at all.

#### DEC-53, addendum — a Chaos spell that fails does something else (3.59.0)

Recorded here rather than as a decision of its own, because it is the same
question DEC-53 answers — what Chaos costs to bring across — asked about the
failure path instead of the spells.

Zangband's spoiler promises this and the code delivers it: a failed Chaos spell
rolls `randint1(100) < spell` against the spell's place in its realm, and on a
hit runs `wild_magic()` ([cmd5.c:452](../../archive/zangband/src/cmd5.c#L452)).
The roll inside is `randint0(spell) + randint0(9)`, so a deeper spell reaches
further down a table that gets worse as it goes — a short teleport at the top,
the Ancient and Foul Curse at the bottom. *Magic Missile* at realm index 0 can
never backfire; *Call the Void* at 31 nearly always does.

The bands are kept boundary for boundary. Three of the forty entries have no
4.2 counterpart and are substituted, and nothing else is:

- Zangband's six **bizarre summon groups** do not exist here, so both summoning
  bands call ordinary monsters. Eight of them, which is Zangband's count.
- **`summon_cyber()`** calls a Cyberdemon; the nearest thing here is a greater
  demon.
- **`wall_breaker()`** smashes walls near the caster, which becomes a
  `KILL_WALL` sphere.

Everything else is an effect the game already had, several of them because
earlier milestones built them: `GAIN_MUTATION` from M8, `ANCIENT_CURSE` from M3,
and `EARTHQUAKE`, `DISENCHANT`, `GRANITE`, `TELEPORT`, the light and dark, and
the door and trap touches from 4.2 itself.

*The scaling is the part worth a test rather than a paragraph.* A spell's `sidx`
counts across the whole class, so Chaos's first spell is 0 for a Chaos-Warrior
and 62 for a Mage. Scaling by `sidx` would look right for a Chaos-Warrior and
make every Chaos spell a Mage owns backfire as though it were the deepest in the
game. `spell_realm_index()` takes the realm's offset off first, and
`player/realm` walks all 218 spells of three classes asserting each realm
restarts at zero.

**Death's failure path is not built with this one.** Zangband punishes a miscast
Death spell too — `(sval + 1)d6` hit points, and the Necronomicon shakes your
sanity — and it is buildable today, because `shadow book` is the Death realm and
the Necromancer already carries four of them. It is deliberately left: it would
change the balance of an existing class *before* that class's spell content is
replaced, which is the half-state the M9 ordering exists to avoid. It belongs in
the Death realm's own commit.

#### DEC-49, addendum — the fold went Zangband's way (3.61.0)

DEC-49 recorded that Zangband's Arcane and Angband's arcane are not the same
realm, that having exactly seven realms forces them together, and that folding
them "keeps the stronger of the two". That was the state while Arcane still held
Angband's content. **DEC-50 replaced it**, so the sentence now reads the other
way: this game's Arcane is Zangband's Arcane, and the fold cost Angband's side
of it — two books of attack spells and the *Wizard's Tome of Power*.

Recorded rather than edited into DEC-49, because what DEC-49 decided (seven
realms, mapped by content) is unchanged; only its consequence flipped, and it
flipped because a later decision was taken, which is the kind of thing a
decision record should show rather than tidy away.

*What it costs.* In thirty-two Arcane spells there is one ball, one bolt and one
beam. A Mage's damage now comes from its **second** realm, which is Zangband's
own arrangement -- a Mage there takes two, and Arcane plus Chaos is the classic
pair. A Mage who takes Arcane and Sorcery has chosen two realms that barely
fight, and Zangband would have let them do exactly that.

*What it buys.* All four Arcane books are sold in town, for 100 to 2500 gold.
That is the other half of the spoiler's sentence and the realm's whole bargain:
no other realm can be bought outright, and every other realm keeps its two best
books in the dungeon. The town/dungeon flag is not cosmetic -- `store.c` stocks
every town book permanently and never a dungeon one -- so emitting Arcane under
the generic two-town rule made half the realm unbuyable and quietly removed the
reason to choose it. `player/realm` now pins the four books' costs and
allocation depths by number, because the converter check cannot tell that the
generator was asked for the wrong ladder.

---

### DEC-55 — A class Zangband never had takes the figures of the one it matches (CNT-10, DEC-50)

**Decision.** Three of Angband 4.2's nine casting classes post-date Zangband and
have no row in `magic_info[]`: the **Druid** (Nature), the **Necromancer** and
the **Blackguard** (both Death). DEC-50 replaces those realms' content anyway,
so each borrows the figures of the Zangband class it matches on the three magic
constants both games inherited from the same ancestor — `spell_first`,
`spell_weight`, and the casting stat.

This was recorded as an obstacle "wanting deciding before Nature or Death is
attempted". It turns out to be a derivation rather than a decision, which is why
it is settled here rather than referred on.

*Why the key is trustworthy.* Six of the nine match a Zangband class **exactly**,
and match the one with the same name:

| 4.2 class | first / weight / stat | Zangband |
|---|---|---|
| Mage | 1 / 300 / INT | Mage |
| Priest | 1 / 350 / WIS | Priest |
| Paladin | 1 / 400 / WIS | Paladin |
| Rogue | 5 / 350 / INT | Rogue |
| Ranger | 3 / 400 / INT | Ranger |
| Chaos-Warrior | 2 / 400 / INT | Chaos-Warrior |

Six of six agreeing is not a coincidence; these are constants 4.2 inherited from
the Angband that Zangband forked. That is the same standard the `magic_info[]`
slicing was held to — stated twice in independent places and agreeing — and it
is what makes the key evidence rather than a guess.

*The three that do not match by name:*

- **Druid → Zangband's Priest.** 1 / 350 / WIS, and among the six Zangband
  classes that carry Nature it is the **only** one with that key. Determined,
  not chosen.
- **Necromancer → Zangband's Mage.** 1 / 300 / INT matches Mage *and* High-Mage
  exactly. The tie goes to the **generalist**: the High-Mage's better figures are
  the price of being allowed a single realm, and 4.2's Necromancer pays no such
  price — it is a five-book caster with a full realm, which is a Mage's bargain.
- **Blackguard → Zangband's Paladin.** 1 / 600 / INT matches nothing: Zangband's
  weights run 300 to 400 and stop. So the rule extends by one step — exact
  `spell_first`, then nearest `spell_weight` — which among Death's carriers with
  `spell_first` of 1 gives the Paladin at 400. Worth noting because it reads
  backwards at first: `spell_weight` is the armour weight a caster tolerates
  before losing mana, so 600 makes the Blackguard the *least* penalised for
  armour, not the worst caster. The Paladin is 4.2's other armoured
  fighter-caster, so the key and the character agree.

**The rule, once, so it does not have to be re-argued:** *exact match on
`spell_first`, `spell_weight` and casting stat where one exists; failing that,
exact `spell_first` and nearest `spell_weight`; ties to the generalist over the
specialist.*

**What this is not.** It is not a balance judgement dressed as a derivation. The
figures are data, regenerated by `zconv realms` from a donor named in one place,
so a different donor is a one-word change and a re-run. If the Necromancer plays
wrong at the High-Mage's numbers instead of the Mage's, that is a calibration
finding and cheap to act on -- which is the reason for putting the donor in the
data rather than in the emitted file.


#### DEC-36, addendum — what the Monk's casting turned up (3.65.0, PLR-03, PLR-08)

Two things the field names actively mislead about, and one defect in a class
already shipped.

**Chosen versus random is not `spell_type`.** Zangband's `magic_info[]` carries a
`spell_type` field that looks exactly like the switch — 0 for the Monk, 1 for the
Priest — and **nothing reads it**. What decides is the class's own `spell_book`
constant: `do_cmd_study()` branches on `mp_ptr->spell_book != TV_LIFE_BOOK`
([cmd5.c:339](../../archive/zangband/src/cmd5.c#L339)), taking a selected spell
if so and a random one if not. That field is *never reassigned from the chosen
realm* — `REALM1_BOOK` is a separate macro — so the Monk's constant stays
`TV_LIFE_BOOK` whichever realm it picks, and **a Monk of Nature learns at random
exactly as a Priest does.** So the Monk does *not* get `CHOOSE_SPELLS`, which is
the opposite of what "it is a caster, give it the caster flag" would have done.

The rule is trustworthy on the same standard as DEC-55's key: applied to the six
classes both games have, it agrees with Angband 4.2's own convention six times
out of six — Priest and Paladin random, Mage, Rogue and Ranger chosen.

**And it found a defect.** The Chaos-Warrior's constant is `TV_SORCERY_BOOK`, so
it chooses its spells, and 3.58.0 gave it books without the flag. It has been
learning Chaos at random since — fixed here. The Mindcrafter's constant is
`TV_LIFE_BOOK` and it carries `CHOOSE_SPELLS`, which is wrong by the same rule
and *inert*, because the flag is only read by the study command and a
Mindcrafter has no books to study; left alone and noted rather than changed.

**The martial and the magical meet in the armour.** A Monk is the only class in
the game that is both `MARTIAL_ARTS` and a caster, and Zangband weighs *the same
six slots* twice: at `100 + level * 4` for the martial penalty
([xtra1.c:2466](../../archive/zangband/src/xtra1.c#L2466)) and at `spell_weight`
for the mana penalty. The Monk's `spell_weight` is **300 — the Mage's figure, not
the Priest's 350** — so a Monk in heavy armour loses its unarmed bonuses *and*
its spell points together. Nothing needed building for that: 4.2 already weighs
the same slots for `monk_armour` (M7 built it to Zangband's threshold exactly)
and for `cumber_armor`. It is asserted rather than assumed, because a class where
one half quietly disables the other is the failure mode worth testing for.

*Three things checked and found not to apply.* Zangband's **glove** penalty is
gated on `spell_book == TV_SORCERY_BOOK`, so a Monk never had one — and 4.2 has
no glove penalty at all, so there was nothing to reproduce either way. None of
the twenty-nine `CLASS_MONK` sites in Zangband touches mana or spell failure;
every one is martial or cosmetic. And `item_tester_tval = mp_ptr->spell_book` in
`do_cmd_study()` means a Zangband Monk of Nature cannot select its own books at
all — a bug in the original, not carried across: in this game books are matched
by realm through `player_object_to_book()`, so a Monk of Nature reads nature
books and chants verses.

**One consequence worth stating rather than fixing.** The casting stat comes from
the *realm* here and from the *class* in Zangband (PLR-10, DEC-49 chose 4.2's
per-realm metadata deliberately). Zangband's Monk casts everything on WIS; here a
Monk of Life or Nature casts on WIS and a Monk of **Death on INT**, which the
Monk has at −1. So Death is the weak choice of its three rather than a free one.
That is the established design showing through a new class, not a new decision.

**And one gap found in a class shipped last release.** `calc_mana()` gives
`CLASS_HIGH_MAGE` a flat `msp += msp / 4`
([xtra1.c:1768](../../archive/zangband/src/xtra1.c#L1768)) on top of the better
levels and costs its `magic_info[]` row already carries. Our High-Mage does not
have it, and 4.2's `class.txt` has no field for a mana multiplier, so it wants
code rather than data. Not done here; recorded so it is not found twice.

---

### DEC-56 — A power paid for in blood is not also penalised for it (PLR-02, PLR-16, BAL-08)

**Decided by the project owner, 2 September 2026: no.**

`player_power_chance()` charged `5 * (cost - csp)` when a character was short
of mana for a racial or mutation power, which is how spells work
([player-spell.c:621](../../src/player-spell.c#L621)). It is removed.

**The reason it was wrong is that its condition was already spoken for.** The
penalty tested `csp < cost`; `player_use_power()` uses
`use_hp = (p->csp < power->cost)` to decide the price comes out of hit points.
Character for character, those are the same people. So the surcharge fell on
exactly those already paying blood and on nobody else — charged twice for one
shortfall — while the comment beside the blood price said it existed *"so a
Draconian Warrior could never once breathe"* without it, and the penalty then
made that Warrior fail 95 times in 100 anyway. The two halves of the same
feature were pulling against each other.

**Measured, before and after.** A Draconian's breath: cost 25, power level 15,
base failure 12.

| Level | Full pool | Empty pool, before | Empty pool, after |
|---|---|---|---|
| 15 | 7% | 95% | 7% |
| 25 | 6% | 95% | 6% |
| 30 | 6% | 87% | 6% |
| 40 | 6% | 57% | 6% |
| 50 | 6% | 27% | 6% |

Worth recording that the old behaviour was not a permanent 95: the level term
ate into the shortfall, so the power arrived around level 30 rather than never.
An earlier note of mine said a Warrior "still cannot breathe" — that was drawn
from the single level-15 figure a test asserted and generalised too far.

**Spells keep their penalty, and that is not an inconsistency.** A spell is not
paid for in blood. It is paid for in unconsciousness: `player_over_exert()`
faints the caster for `5 * shortfall + 1` turns and takes a point of
constitution half the time, after a *"Attempt it anyway?"* confirmation. Blood
is a price and fainting is a risk; neither needs a surcharge on top, and the
power path has no faint.

**What it costs.** Any character can now use a power at its true failure rate
with an empty pool, paying `randint1(cost - cost/2) + cost/2` hit points — 13
to 25 for a cost of 25. Two classes have no mana at all, the Warrior and the
Mindcrafter, and they are the ones this was blocking. The blood price and the
"You have not the strength left." refusal below `chp < cost` are unchanged, so
the power is still rationed by how much punishment the character can take.

`player/wild`'s *a-cheat-makes-a-power-fire* now asserts
`eq(without_mana, with_mana)` — equality rather than the old inequality,
because it catches the penalty returning in any size.

---

### DEC-57 — Every entitlement Zangband gives is carried over (CNT-10, PLR-08, DEC-50)

**Decided by the project owner, 2 September 2026:** *"If the entitlements are in
Zangband then we should carry them over and complete the task by including them
here in our implementation."* Fidelity to the tables, against a real balance
change.

**The gap, and that it was nobody's decision.** Zangband's
`realm_choices1/2[]` were transcribed verbatim into `class.txt` from the start
— `player/realm`'s `class-realm-entitlements-match-zangband` has asserted that
since 3.53.0, and it never failed. What was missing was the *books*. Five classes
could choose realms they had no content for:

| Class | Entitled but bookless |
|---|---|
| Mage | Life, Nature, Death |
| Priest | Arcane, Nature, Death |
| Rogue | Death |
| Ranger | Arcane, Death |
| Paladin | Death |

Ten class-realm pairs, forty books, **305 spells**.

This was an artefact of the order the realms arrived in, not a judgement. Sorcery
and Chaos were **appended** to the classes entitled to them, so those got full
coverage. The four mapped realms were **replaced** in place under DEC-50 — and
replacement only touches classes that *already carry* books of that realm, which
in Angband 4.2 meant Life for the Priest and Paladin, Arcane for the Mage and
Rogue, Nature for the Druid and Ranger, Death for the Necromancer and Blackguard.
Nothing ever emitted a mapped realm into a class that had not previously had it.

The tell is the asymmetry. The Warrior-Mage and the High-Mage arrived after all
six realms existed and were emitted with the full set, twenty-four books each.
The Mage and the Priest are entitled to exactly the same realms in Zangband and
had twelve. Two classes with one entitlement and two different amounts of
content is not a design; it is a rollout showing through.

**DEC-50 does not cover it.** That decision is about *replacing* the four mapped
realms' spell content and the savefile cost of doing so. It says nothing about
which classes carry which realm, and the word "entitlement" does not appear in
it.

**The consequence, plainly: a Mage that can study Death is not Angband's Mage.**
Six realms are open to it now instead of three, and among them Death — drains,
nether, genocide, *Hellfire* — and Life, with healing and *Holy
Invulnerability*. That is Zangband's Mage, which is a far broader class than
4.2's, and the same goes for the Priest, which can now take Arcane. It is a
larger change to how the game plays than any single realm import was, and it was
taken deliberately for fidelity rather than for balance. BAL-14's dials remain
the way to answer it if playtest says the classes are now too strong; the
alternative — keeping a reduction nobody chose — would have been worse.

**What was checked before doing it.**

- *The books go on the end.* Every new realm is appended, never inserted, so
  every spell index that already existed still means what it meant.
  `every-book-kept-its-place` reads the whole thing as ordered runs and the
  Mage's Chaos books are still at flat index 64.
- *The savefile.* Appending leaves indices alone but the fingerprint covers the
  whole list, so it changes: a saved caster **with spells recorded is refused**
  rather than silently rebound, which is the fence doing what 3.52.0 built it
  for, and one with none loads unaffected. The corpus needed no change — all
  thirty-five files already fail a step earlier on the prayer-book kind DEC-50
  deleted, so `game/saves` is unmoved at 38/38 and `EXPECTED-FAILURES` is
  untouched.
- *The chosen-versus-random gate*, which DEC-36's addendum found for the Monk,
  **does not move for anyone.** It is keyed on the class's own `spell_book`
  constant, and 4.2 keys it on the class flag `PF_CHOOSE_SPELLS`; neither
  consults the realm. So a Priest that gains Arcane still learns at random, and
  a Mage that gains Life *chooses* its prayers. The same Life book behaves
  differently in two hands, which is Zangband's behaviour and worth knowing at
  the table.
- *No deferral becomes reachable.* All seventeen realm-spell deferrals are
  per-spell mechanism reasons and none names a class, so widening who carries a
  realm widens the deferrals with it: eighty-seven effectless spells became a
  hundred and twenty.
- *Seven spells are out of reach on their own figures* — Death's last seven for
  the Rogue and the Ranger, Arcane's last one for the Ranger, all above level 50.
  That is `usable()` doing Zangband's arithmetic, the same as the Rogue's Sorcery
  before it.

**And one visible consequence beyond the spell lists.** A Mage's pack now sorts
a nature book and a prayer book as *readable*, because it can read them. 4.2 puts
readable books first and then sorts by decreasing tval, so the magic book — first
when it was the only one a Mage could use — comes last of the three.
`player/calc-inventory` caught it, and the expected order was derived from the
rule before the test was re-run rather than copied from the failure.
