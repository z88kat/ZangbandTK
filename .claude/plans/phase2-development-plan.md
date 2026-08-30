# Phase 2 — Development Plan

**Status:** draft · **Phase:** 2 · **Inputs:** the four Phase 1 requirement documents and
[decisions.md](decisions.md)

Sequences all 86 Phase 1 requirements into twelve milestones. Phase 3 (the Tcl/Tk front
end) follows this plan; see [§5](#5-a-note-on-phase-3-timing) for an argument about
starting part of it earlier.

---

## 1. Sequencing principles

1. **Reach something playable early, and stay playable.** Every milestone from M1 ends with
   a game that runs. The lethality scalar alone (BAL-13) makes the game feel different on
   day one, before a single monster is imported.
2. **Cheap and visible before large and structural.** Content and balance first; they prove
   the conversion tooling on low stakes and deliver most of the visible character.
3. **The world gates the rest.** Town monsters, dungeon themes and wilderness quests all
   need somewhere to be.
4. **Committed risk goes last, not never.** Realms (M9) and pets (M10) are the two largest
   risks and two most characteristic features. DEC-19 commits to both; scheduling them
   after the game is playable is risk management, not hedging.
5. **Documentation ships with each milestone**, per DEC-17. A milestone is not done until
   its manual chapter is written.

### Working from Zangband's source (DEC-20)

Clean-room is dropped, so the original implementation is available throughout. The rule is
**port where the algorithm is the value; reimplement where 4.2's architecture differs**, and
keep the copyright headers on anything ported.

Where it changes the work materially:

| Milestone | What the source gives us |
|---|---|
| **M4** | The whole reason this milestone gets easier. Fractal terrain, the height/population/law decision tree, block caching, and road/river routing in [wild1.c](../../archive/zangband/src/wild1.c)–[wild3.c](../../archive/zangband/src/wild3.c). Take the *algorithms*; leave the data structures, since W-1 puts the wilderness on 4.2's chunks. |
| **M5** | Building placement scoring in parameter space, town layout by size band, and dungeon character selection ([bldg.c](../../archive/zangband/src/bldg.c), [wild2.c](../../archive/zangband/src/wild2.c)). |
| **M6** | Quest generation, placement constraints and trigger handling ([quest.c](../../archive/zangband/src/quest.c), 2,692 lines). |
| **M3** | Exact cascade probabilities for the Ancient and Foul Curse, and the random-ability tables. |
| **M7** | Chaos patron reward and punishment tables — hard to reconstruct from prose. |
| **M8** | The 96 mutation effects as implemented ([mutation.c](../../archive/zangband/src/mutation.c), 2,056 lines), where the spoiler gives the description and the source gives the numbers. |
| **M10** | Pet AI and command handling — though note this is spread across Zangband's monster code rather than collected, so it is a reading exercise more than a port. |

M4's risk drops most. It remains the largest milestone, but "adapt a working generator onto
a different world model" is a materially different problem from "invent a world generator".

Two things this does **not** change: the balance decisions (DEC-08 to DEC-10 are about
numbers, and BAL-08 still requires checking the consuming formula), and the requirement
documents themselves, which define what is being built rather than how.

### Standing constraints

- **BAL-08 applies throughout.** No numeric field is converted before the code that consumes
  it has been checked in both codebases. This rule was written because ignoring it produced
  a 20× error.
- **Do not foreclose PLR-22.** Everything touching monster handling before M10 must leave
  room for monsters to have an allegiance. Pets arrive late; the assumption that every
  monster is hostile must not be deepened in the meantime.
- **DEC-11 means design for clarity.** No contortions to preserve upstream mergeability.

---

## 2. Milestones

### M0 — Foundations
*Unblocks everything. No gameplay change.*

- Project identity: name, version, and the DEC-07 savefile break.
- Confirm the 4.2 build and test suite are green on ARM64 as a baseline.
- **Conversion tooling** (BAL-11, BAL-12): parser for the 2.8.1-era `*_info.txt` formats,
  emitter for 4.2's data files, override file, and the review report that is its real
  output. Must handle the `& Name~` normalisation from content §1.
- Documentation scaffolding in [docs/](../../docs/) per DEC-17.
- **Read the remaining official documents** (DEC-16). Four documents produced three
  requirements; thirty remain. Feed corrections back into the Phase 1 docs *before*
  building against them.

**Exit:** builds clean, tests pass, converter runs end-to-end on one data file and produces
a reviewable report. Phase 1 docs updated from the documentation pass.

---

### M1 — Balance foundation
*First playable ZangbandTK.*

- BAL-01 to BAL-07 — scale conversions: armour class ×9/8, dice→average hit points,
  `sleepiness` mapping, experience formula handling.
- BAL-13, BAL-14 — the lethality scalar (×0.73 hit points, ×0.50 armour class) as named
  constants in `constants.txt`.
- Resolve balance Q1: recover the 2.8.1→4.2 `sleepiness` mapping from the 434 shared
  monsters.

**Exit:** 4.2's own content plays at Zangband lethality; both scalars tunable without a
rebuild. Manual chapter: how balance differs from both ancestors.

> This is the first checkpoint worth playing for a while. The game is still vanilla in
> content but should already feel faster and more dangerous. If it does not, BAL-13's
> constants are wrong and that is far cheaper to discover now than after M2.

---

### M2 — Bestiary and treasure
*The content that makes it Zangband.*

- BAL-09, BAL-10 — curve calibration for imported monsters; unmapped flag reporting.
- CNT-01 to CNT-04 — the monster import, Amber and Mythos unique sets, flag resolution
  across 118 distinct flag tokens. **CNT-02 is closed by the `AMBERITE` flag**: the twelve
  arrived with the import, but nothing connected them until the flag gave them a shared
  kind, the `S_AMBERITES` summon and the blood curse a dying one lays on its killer.
- CNT-06 to CNT-09 — artifacts and ego items, translated into 4.2's `object_property`,
  `brand`, `slay` and `curse` structures.
- **The M2 flag group is closed.** `AMBERITE`, `CAN_SPEAK`, `RES_TELE`, `REFLECTING` and
  the three auras are implemented, and with them the ego flags `REFLECT`, `SH_FIRE` and
  `SH_ELEC` — the same three mechanisms seen from the wearer's side, which is why they were
  built together. Nothing tagged M2 remains in either flagmap.
  > *That sentence used to go on to say what was left, and named three flags.* It was
  > wrong within a day — `SHAPECHANGER` shipped, and it had never counted the six
  > object flags in `objflagmap.toml`'s own `[implement]` block, which is a second
  > section in a second file. Counting flags belongs in the flagmaps, which the
  > converter reads and cannot be wrong about; a plan that restates them drifts.
- **CNT-11 is done** (3.40.0) — 82 object kinds imported by `zconv objects`, with 19
  deferred, 16 refused as artifact bases and 10 recognised as renames of objects 4.2
  already has. It needed three more object properties (`STRANGE_LUCK`, `PSI_CRIT`,
  `NO_MAGIC`) and corrected the §1 object count in
  [phase1-content-and-flavour.md](phase1-content-and-flavour.md), which had been measured
  by name.
- Theme-first curation throughout, per DEC-19.

**Exit:** Amber, Mythos and Chaos content in play at calibrated stats; conversion report
reviewed and its invented numbers signed off. Manual chapters: monsters, objects.

---

### M3 — Effects and vaults

- CNT-15 — the Ancient and Foul Curse, including the cascade (the cascade *is* the mechanic).
- CNT-16 — random object abilities at generation time.
- CNT-13, CNT-14 — monster pit themes extended; curated Zangband vaults.

**Exit:** a recognisably Zangband-flavoured game on a 4.2 base, with no world yet. Second
substantial play checkpoint.

---

### M4 — Wilderness core
*Largest structural milestone. Expect it to want splitting once started. Risk substantially
reduced by DEC-20 — this is now an adaptation of a working generator, not an invention.*

- **Pre-work:** measure `struct chunk`'s memory footprint against the live-block target
  (world Q4). This shapes everything after it — a chunk carrying noise and scent heatmaps
  sized for a dungeon level may be too heavy at ninety live blocks.
- WLD-01 to WLD-06 — block-as-chunk representation, seed-derived generation, modified-block
  persistence, bounded cache, and the `chunk_find_name()` linear-scan replacement.
- WLD-07 to WLD-09 — height/population/law parameter space, sea, lakes, rivers, roads;
  terrain expressed in `terrain.txt`.
- WLD-23, WLD-24 — world coordinates on the player, and travel transitions generalised
  beyond stairs.
- Sized at roughly a quarter of Zangband's linear dimension, per DEC-19, via WLD-02.

**Exit:** the player can walk a generated world that saves and restores correctly.

---

### M5 — Places
*Towns, dungeons, buildings — the world becomes worth walking.*

- WLD-10 to WLD-14 — multiple towns and dungeon entrances, size bands, inhabitant types,
  fixed starting town, per-dungeon depth range and character. Includes **WLD-11a**, towns
  differing in which stores they hold; WLD-13 is superseded by DEC-26 and drops out.
- WLD-15 to WLD-18 — buildings placed by population/magic/law suitability; stores extending
  4.2's store system; services as terrain with attached actions.
- WLD-25 — the overhead world map.
- CNT-05 — town and wilderness monsters.
- CNT-12 — per-dungeon object themes. **Verify first** (content Q4): confirm `obj_theme`'s
  four-way weighting can drive 4.2's allocation system.

**Exit:** several distinct towns with working buildings, several dungeons that feel
different from each other. Manual chapters: the wilderness, towns and services.

---

### M6 — Quests

- WLD-19 to WLD-22 — six quest types, four-state lifecycle, six triggers, world placement,
  persistence and a player-visible quest log.

**Exit:** quests can be taken, tracked, completed and failed. Manual chapter: quests.

> **Complete.** All four requirements are delivered and all six quest types are offered and
> completable, the last three released as 3.26.0. The manual chapter is written
> ([quests.rst](../../docs/quests.rst)) and every type is covered by
> [src/tests/player/quest.c](../../src/tests/player/quest.c). Nothing in M6 is outstanding.

> Phase 2's world work is complete here. Third major play checkpoint, and the point at
> which the game stops resembling Angband-with-extras.

---

### M7 — Races and classes

- PLR-01 — 21 new races with Zangband's stats, skills, hit dice and `expfact`.
- PLR-02 — racial activatable powers (new mechanism; 4.2 has none).
- PLR-03 to PLR-06 — Warrior-Mage, Chaos-Warrior with patrons, Monk with unarmed
  progression, Mindcrafter with psionics, High-Mage.
- PLR-07 — retain Druid, Necromancer, Blackguard.

**Exit:** all races and classes playable, less the two deferred below. Manual chapters:
character creation, races, classes.

> **Two of PLR-03's five classes are deferred to M9.** The Monk, Mindcrafter and
> Chaos-Warrior are done (DEC-36, DEC-37, DEC-38), each chosen because its identity needs
> nothing from another milestone. The **Warrior-Mage** and the **High-Mage** are the two
> defined by which magic realms they may choose, so they cannot be built before PLR-08 and
> PLR-09 exist. They move to M9 and are listed there; M7 closes without them rather than
> staying open across two milestones. Nine races landed rather than twenty-one, per DEC-35's
> curation — PLR-01 is met as amended, not partially.

---

### M8 — Mutations and virtues

- PLR-13 to PLR-17 — three mutation groups, runtime acquisition and loss, represented as
  player properties, exposed in a power list and the character sheet.
- PLR-34 to PLR-38 — the documented acquisition and removal paths, Beastmen gaining
  mutations by design, the seven mutually cancelling groups, and race-weighted probability.
  These came out of the M0 documentation pass and were missing from this plan; PLR-36 and
  PLR-38 tie directly back to M7's races. PLR-34 no longer carries the Eldritch Horror
  path — CNT-17 is closed (DEC-32, confirmed by the project owner).
- PLR-18 to PLR-21 — 8 virtues per character drawn from a pool of 18, selected at birth,
  tracked, displayed, persisted, **and consumed by at least one system**. PLR-21 is a gate:
  if nothing consumes virtues, cut them rather than ship inert numbers. **DEC-39 commits to
  keeping virtues and leaves the consumer open** — the patron ladder and the inn dream are
  the recorded candidates. Note that Zangband shipped virtues inert, so there is nothing to
  port: whatever consumer lands is new design under DEC-30.
- **The Chaos Tower** (WLD-16c, DEC-24) — the mutation-removal building service, the seventh
  service and the only one not delivered in M5. It has no handler in 2.7.5's `bldg.c`, so it
  is written rather than ported, and it is one of PLR-35's removal paths.
- Use `spoilers/mutation.txt` (DEC-16) to select, rather than guessing from flag names.
- **Carried over from M7 (DEC-38):** the Chaos-Warrior's patron rewards have a
  one-in-six branch that grants a mutation, left as a deliberate gap when PLR-05
  was built. Fill it in when PLR-13 exists.

**Exit:** mutations acquirable and visible; virtues doing something. Manual chapters:
mutations, virtues.

---

### M9 — Magic realms
*Committed by DEC-19. Structural.*

- PLR-08 to PLR-12 — realm selection at birth, seven realms, 4.2's realm metadata retained
  and extended, savefile and display, and **4.2's existing class progressions re-expressed
  as realms** so retained classes work within the system rather than beside it.
- CNT-10 — realm spellbooks, gated on which realms land.
- **Carried over from M7:** the remainder of PLR-03 — the **Warrior-Mage** and the
  **High-Mage**, the two classes defined by which realms they may choose. They were deferred
  out of M7 because neither can be built before PLR-08 and PLR-09 exist.
- Life realm content comes from [archive/zangband/](../../archive/zangband/); its spoiler is
  the one unarchivable document (DEC-16).

**Exit:** a Mage's realm choice defines the character, and every class in PLR-03 is
playable. Manual chapter: the magic system.

> PLR-12 is the trap here. Half-implementing this leaves the game with two magic systems.

---

### M10 — Pets
*Committed by DEC-19. The invariant change.*

- PLR-22 — monsters carry an allegiance; hostility stops being an invariant.
- PLR-23, PLR-24 — AI accounts for friendly monsters; player attacks, spells and
  projections distinguish targets, with confirmation before harming a pet.
- PLR-25 to PLR-28 — nine command modes with distance behaviour, persistence across levels
  and saves, visual distinction, and summoning/charming as sources.

**Exit:** pets commandable, persistent and distinguishable. Manual chapter: pets.

> Budget generously. Every place 4.2 assumes "monster ⇒ enemy" is a potential defect, and
> they are not enumerable by search. The standing constraint in §1 exists to keep this from
> getting worse between now and here.

---

### M11 — Nightmare mode

- BAL-15 to BAL-17 — irreversible birth option, the full modifier set, composing with
  BAL-13 rather than replacing it (hit points become `base × 0.73 × 2`).
- Must follow M10: the pet-decay behaviour needs the allegiance model.

**Exit:** nightmare mode selectable and unfair as designed. Manual chapter: nightmare mode.

---

## 3. Requirement coverage

| Milestone | Requirements | Count |
|---|---|---:|
| M0 | BAL-11, BAL-12 (+ BAL-08 standing) | 2 |
| M1 | BAL-01…BAL-07, BAL-13, BAL-14 | 9 |
| M2 | BAL-09, BAL-10, CNT-01…CNT-04, CNT-06…CNT-09, CNT-11 | 11 |
| M3 | CNT-13…CNT-16 | 4 |
| M4 | WLD-01…WLD-09, WLD-23, WLD-24 | 11 |
| M5 | WLD-10…WLD-18 (incl. WLD-11a, WLD-16a–16d; less WLD-13), WLD-25, CNT-05, CNT-12 | 16 |
| M6 | WLD-19…WLD-22 | 4 |
| M7 | PLR-01…PLR-07 (less the two classes deferred to M9) | 7 |
| M8 | PLR-13…PLR-21, PLR-34…PLR-38 (+ the Chaos Tower, WLD-16c) | 14 |
| M9 | PLR-08…PLR-12, CNT-10 | 6 |
| M10 | PLR-22…PLR-28 | 7 |
| M11 | BAL-15…BAL-17 | 3 |
| | **Total** | **94** |

BAL-08 is a standing rule rather than a milestone deliverable, accounting for the 95th.

> **Seven requirements remain unassigned**, all of them from the M0 documentation pass, which
> this table was never updated for. **PLR-29 to PLR-33** are pet requirements and belong with
> M10 — PLR-30 and PLR-31 in particular are the reason pets are not overpowered, so M10 must
> not be sized without them. **PLR-40** (knowledge is a thing that can be lost) and
> **PLR-41** (a night at the inn shows the sleeper something) were both built in M5 and need
> recording as done rather than scheduling — as do **CNT-20** (not everything is there to be
> fought) and **WLD-26** (the monster array is sized per chunk), which this table has never
> mentioned at all. Assigning these is a separate pass and is not attempted here.

---

## 4. What can run in parallel

The chain M0 → M1 → M2 is strictly sequential; the converter must work before content
imports, and the scalars must be right before content is calibrated against them.

After M3 the plan forks. **M4–M6 (world)** and **M7–M8 (player)** have no hard dependency
on each other and could proceed together given the capacity — with two caveats: CNT-05 and
CNT-12 sit in M5 but are content work, and M8's mutations interact with M7's races.

**M9, M10 and M11 are strictly last**, in that order. M11 depends on M10; both depend on a
stable game to modify.

---

## 5. A note on Phase 3 timing

The plan above completes all gameplay before any Tcl/Tk work, following the phase structure
in [Idea.md](Idea.md). One argument for bringing a minimal slice forward:

M5's overhead world map (WLD-25) and M4's terrain generation are both far easier to
evaluate visually than through a terminal, and the tile and sound assets in
`archive/Tk/image` and `archive/Tk/sounds1-4` already exist. A bare `main-tcl.c` that only
renders the map — no menus, no UI — would make the largest and riskiest milestone
observable while it is being built.

Not a recommendation to restructure the phases, and it costs real time up front. Raising it
because M4 is where the project is most likely to get stuck, and being able to *see* a
generated world is worth a lot when debugging one.

---

## Open

- M4's chunk footprint measurement (world Q4) should happen before M4 is estimated; it may
  change the design.
- Milestone sizing is deliberately absent. M4 and M10 are the two most likely to be
  underestimated, and M2's flag translation is larger than its requirement count suggests.
- The DEC-16 documentation pass in M0 may add requirements. Three came from four documents;
  thirty remain unread.
