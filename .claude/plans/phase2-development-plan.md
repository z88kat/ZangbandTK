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
  was built. **Done** in 3.49.0 — and in Zangband's order, where the mutation
  *replaces* the favour rather than arriving beside it.

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
- **Folded in from M8, by the project owner's decision:** three mutations whose powers were
  deferred for want of an effect. They are built here rather than earlier because two of
  them are cheap either way and the third is only cheap *here*.
  - **Telekinesis** needs a `FETCH` effect, and this is the reason all three wait. Zangband's
    `fetch()` is 110 lines ([spells3.c:1139](../../archive/zangband/src/spells3.c#L1139)) and
    handles weight, line of sight and object piles; 4.2 has no equivalent and never did. It
    has **three consumers, not one**: the mutation, Sorcery's *Telekinesis* spell, and the
    Trump realm, which calls the same function
    ([cmd5.c:864](../../archive/zangband/src/cmd5.c#L864) and
    [cmd5.c:2046](../../archive/zangband/src/cmd5.c#L2046)). Designing it once against all
    three is the whole point of folding it in; built for the mutation alone it would be
    retrofitted twice.
  - **Swap position** needs an effect that exchanges the player and a monster.
    `monster_swap()` already exists in 4.2 (`mon-util.c:644`) and handles the player, so this
    is roughly fifteen lines over a primitive the game runs on every monster turn.
  - **Sterilize** needs an effect that sets `cave->num_repro` to `repro_monster_max`, plus
    the 17–34 hit points it costs. The counter is live in 4.2 (read at `mon-move.c:1077`).
    Cheap, and the recorded reservation stands: it reaches into monster generation to carry
    one mutation, which is a judgement rather than an obstacle.

  Neither swap nor sterilize unlocks anything but itself, and neither is realm work — they
  ride along because M9 is where the mutation-power gap is being closed, not because the
  realms need them.

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

Every requirement defined in the four Phase 1 documents appears exactly once below.
There are **109**; the table used to account for 91 of them and to say in a footnote
that seven were unassigned, which was wrong in both directions. Rebuilt rather than
patched, and the arithmetic is checkable: the milestone rows sum to 104, and the five
that are not scheduled are listed under it with a reason each.

| Milestone | Requirements | Count |
|---|---|---:|
| M0 | BAL-11, BAL-12 | 2 |
| M1 | BAL-01…BAL-07, BAL-13, BAL-14 | 9 |
| M2 | BAL-09, BAL-10, CNT-01…CNT-04, CNT-06…CNT-09, CNT-11 | 11 |
| M3 | CNT-13…CNT-16 | 4 |
| M4 | WLD-01…WLD-09 (incl. WLD-04a, WLD-04b, WLD-08a), WLD-23, WLD-24 | 14 |
| M5 | WLD-10…WLD-18 (incl. WLD-11a, WLD-16a–16d; less WLD-13), WLD-25, CNT-05, CNT-12, PLR-40, PLR-41 | 18 |
| M6 | WLD-19…WLD-22 | 4 |
| M7 | PLR-01…PLR-07 | 7 |
| M8 | PLR-13…PLR-21, PLR-34…PLR-38 (+ the Chaos Tower, the last row of WLD-16c) | 14 |

**M8 is complete.** All fourteen requirements, in five phases:

| Phase | What landed | Version |
|---|---|---|
| 1 | The model: 96 mutations generated out of `tables.c`, the weighted roll, the nine cancelling pairs, race affinity, Beastman birth and per-level, the savefile block (PLR-13, PLR-36, PLR-37, PLR-38) | 3.45.0 |
| 2 | The 32 continuous mutations as player properties, through `calc_bonuses()`, and the character sheet (PLR-15, PLR-17) | 3.46.0 |
| 3 | 24 of the 32 activatable mutations in the power list beside racial powers (PLR-16) | 3.47.0 |
| 4 | 22 of the 27 random mutations firing on their own timer, and all 5 melee mutations in the attack round (PLR-14, PLR-35) | 3.48.0 |
| 5 | The acquisition and removal paths, the DEC-38 patron carry-over, the Chaos Tower, the potion of New Life (PLR-14, PLR-34, DEC-24) | 3.49.0 |
| 6 | What the milestone was declared complete without: the wizard grant menu, the character sheet's mutations page, the `cheat_powers` option, and the three flags and one food penalty the converter had been skipping (PLR-13, PLR-15, PLR-17) | 3.49.4–3.50.0 |

**M8 was declared complete one phase early.** PLR-17 asks that mutations be "visible in the
character sheet, including their effects", and at 3.49.0 they reached
`write_character_dump()` and nothing on screen — a player could see them only by writing a
file and reading it. The page landed in 3.49.5, reported from play rather than caught here.
Two further gaps closed in 3.50.0: three mutations were not taking away the flags Zangband
has them take away, because the converter read `SET_FLAG` and not the clearing form; and
six descriptions named some of their effects and not others, the worst of them hiding a
vulnerability to electricity behind four points of intelligence and wisdom.

PLR-18 to PLR-21 were done earlier — virtues, their selection, their writers and their two
consumers. That was the milestone's gate: DEC-39 kept the feature on condition that
something read it, and two things do.

**Twelve mutations do nothing** — eleven deferred and one refused (DEC-48) — recorded
per-mutation in `tools/zconv/mutmap.toml` and listed in `docs/mutations.rst`: eight
activatable and four random. Every one needs machinery 4.2 has not got — pets, an incorporeal player state, an
object-to-gold conversion, a pseudo-identification bit, an effect that reads both the hit
point and spell point pools. Two more continuous mutations are inert because they moved
only charisma, which 4.2 removed in 4.2.0.

`mutmap.toml` carries **thirteen** `defer` keys, and the thirteenth is a mislabel worth
correcting rather than counting: the chaos gift has no effect chain because it needs none.
It carries the `PATRON` flag, `player_apply_mutations()` applies flags whatever a
mutation's kind, and `patron_owes_reward()` reads it — so it works through the machinery
PLR-05 built. Counting it as deferred overstated the gap by one in three previous reports.

**What M8 found that the documentation did not say.** The spoiler gives the headline of
each mutation and the headline is generally the good half: hyper-strength is "+4 STR" there
and +4 STR, -1 INT, -1 WIS in the code. All five melee mutations state their dice the wrong
way round — a scorpion tail is written "3d7" and rolls 7d3 — because `natural_attack()`
fills in two variables and passes them in the other order. Three mutations have
prerequisites no Zangband document mentions. The race affinities are not uniform. And the
regeneration penalty the spoiler warns about had been taken out of Zangband before 2.7.5
(DEC-45).
| M9 | PLR-08…PLR-12, CNT-10 (+ PLR-03's two realm classes, and three M8 mutation powers) | 6 |
| M10 | PLR-22…PLR-33 | 12 |
| M11 | BAL-15…BAL-17 | 3 |
| | **Scheduled** | **104** |

And the five that are not:

| Requirement | Why it is not in a milestone |
|---|---|
| BAL-08 | A standing rule, not a deliverable: every imported number has its consuming formula checked in both codebases. It applies to M1, M2, M5 and M11 alike. |
| CNT-20 | Built and shipped, never scheduled. The `BLESSING` race flag and the white deer — ZangbandTK's own, so it belongs to no import milestone. |
| WLD-26 | Built and shipped, never scheduled. Per-chunk monster capacity, done as M4 pre-work because the wilderness could not be built without it. |
| CNT-17 | Dropped by DEC-32. Sanity loss is not being implemented; it is not Amber's. |
| WLD-13 | Superseded by DEC-26. A town is not a persistent chunk, so there is nothing to hold. |

**109 = 104 scheduled + 1 standing + 2 built-unscheduled + 2 closed.** There is no
PLR-39; the numbering skips it.

Corrections this table carried until now, kept here because each was invisible
while the count was wrong:

- **WLD-04a, WLD-04b and WLD-08a appeared nowhere in this document.** All three are
  built — object decay in open country, uniques remembered where ordinary monsters are
  re-rolled, and danger as a function of law. They were inside the `WLD-01…WLD-09` range
  numerically and named nowhere, which is how three shipped requirements went four
  milestones without a mention.
- **PLR-40 and PLR-41 were listed as needing scheduling.** They were built in M5. They
  are M5's, not future work.
- **WLD-16c is counted once, in M5, but is not wholly M5's.** It is a table of eight
  building services rather than a single deliverable: six shipped in M5, the quest giver
  went to M6 as WLD-16d, and the Chaos Tower waits for M8 because it removes mutations
  and there are none yet. Counted where the bulk of it landed, with the remainder noted
  on M8's row — a requirement that spans milestones cannot be counted twice without
  breaking the arithmetic, and cannot be counted once without a note.

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
