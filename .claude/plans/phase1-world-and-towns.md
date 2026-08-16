# Phase 1 Requirements — World & Towns

**Status:** draft · **Phase:** 1 (requirements) · **Feeds:** Phase 2 development plan

Covers the wilderness overworld, multiple towns, buildings, and quests — DEC-04's first
feature family, and the one that drove DEC-11's hard fork. Balance figures for anything
here come from [phase1-balance-calibration.md](phase1-balance-calibration.md); project-wide
decisions from [decisions.md](decisions.md).

This is the largest and most architecturally invasive family. Nothing else in the project
restructures as much of 4.2.

---

## 1. Code volume being drawn from

| Zangband source | Lines | Purpose |
|---|---:|---|
| [wild1.c](../../archive/zangband/src/wild1.c) | 3,577 | Wilderness generation, place selection, fractal terrain |
| [wild2.c](../../archive/zangband/src/wild2.c) | 3,059 | Town/city/dungeon drawing |
| [wild3.c](../../archive/zangband/src/wild3.c) | 2,409 | Wilderness block caching and display |
| [quest.c](../../archive/zangband/src/quest.c) | 2,692 | Quest generation, triggers, state |
| [bldg.c](../../archive/zangband/src/bldg.c) | 2,016 | Building interiors and services |
| [fields.c](../../archive/zangband/src/fields.c) | 1,814 | Field system — see §2.5 |
| **Total** | **~15,600** | |

Plus data: 232 wilderness block types
([w_info.txt](../../archive/zangband/lib/edit/w_info.txt)) and 147 field types
([t_info.txt](../../archive/zangband/lib/edit/t_info.txt)).

For scale, this family alone is comparable in size to a significant fraction of 4.2's
generation subsystem. It is not a weekend port, and estimates in Phase 2 should be built
from this figure rather than from feature counts.

---

## 2. The two world models

### 2.1 Zangband

A single procedurally generated overworld, defined in
[wild.h](../../archive/zangband/src/wild.h) and
[defines.h:159](../../archive/zangband/src/defines.h#L159):

- **129 × 129 blocks** of **16 × 16 grids** = a **2,064 × 2,064 grid** world.
- Terrain assigned by a **decision tree over three parameters** — height, population and
  law (`DT_HGT`, `DT_POP`, `DT_LAW`) — with the 232 `w_info` block types each claiming a
  region of that 256³ parameter space.
- Fractal terrain generation (`frac_block`), a sea covering **1/4** of the map, **4** lakes,
  and rivers.
- **20 towns** and **20 dungeon entrances**, separated by minimum distances
  (`MIN_DIST_TOWN` 10, `MIN_DIST_DUNGEON` 8), joined by **roads** (`ROAD_DIST` 30).
- Only **9 × 9 blocks** are live at a time (`WILD_VIEW`), backed by an explicit cache
  (`WILD_CACHE`).
- Each `place_type` stores a **`u32b seed`** — places are regenerated deterministically
  from the seed rather than persisted in full.

Towns come in four kinds (`TOWN_OLD` the vanilla town, `TOWN_FRACT` procedural,
`TOWN_QUEST`, `TOWN_DUNGEON`), sized by population into small / town / city / castle, and
populated by one of six inhabitant types — villager, elves, dwarf, lizard, monster,
abandoned.

### 2.2 Angband 4.2

One level at a time, as a `struct chunk` ([cave.h:181](../../src/cave.h#L181)) holding
squares, monsters, objects, noise and scent heatmaps. Dimensions from
[constants.txt](../../lib/gamedata/constants.txt): dungeon **66 × 198**, town **22 × 66**.
A single town, laid out by `town_gen_layout()` in
[gen-cave.c:2515](../../src/gen-cave.c#L2515), with **8 stores**.

### 2.3 Where they already meet

Two pieces of luck worth exploiting:

**4.2 already has named, persistent chunks.**
[gen-chunk.c](../../src/gen-chunk.c) maintains `chunk_list` with `chunk_list_add`,
`chunk_list_remove` and `chunk_find_name`, and its own header says: *"This file maintains a
list of saved chunks of world which can be reloaded at any time. The initial example of
this is the town, which is saved immediately after generation and restored when the player
returns there."* Multiple towns is exactly the generalisation this was built for.

**The town dimensions are identical.** Zangband's `TOWN_WID` 66 / `TOWN_HGT` 22 and 4.2's
`town-wid` 66 / `town-hgt` 22 are the same numbers, both inherited from 2.8.1. A Zangband
town fits a 4.2 town chunk exactly, with no rescaling.

### 2.4 Where they do not meet

The wilderness is **2,064 × 2,064 = 4.26 million grids**, against a 4.2 dungeon level of
66 × 198 = 13,068. That is **~326 dungeon levels' worth of world**. It cannot be one chunk,
and 4.2 has no concept of a world larger than the level the player is standing on.

This is the single hardest problem in the project, and the reason DEC-11 abandoned upstream
mergeability.

### 2.5 The fields system is a trap, and mostly redundant

Zangband implements buildings through its **field** mechanism
([fields.c](../../archive/zangband/src/fields.c), 147 types in `t_info.txt`). But
`t_info.txt`'s own header says fields describe *"whether the field is a trap, door,
building, magic wall etc."* — the same mechanism carries traps and doors.

4.2 already has a dedicated trap system ([trap.txt](../../lib/gamedata/trap.txt),
`src/trap.c`) and a terrain system ([terrain.txt](../../lib/gamedata/terrain.txt)).
Importing fields wholesale would introduce a second, parallel implementation of things 4.2
already does well, for the sake of the building subset.

### 2.6 Quests

Zangband has six quest types (`BOUNTY`, `DUNGEON`, `WILD`, `MESSAGE`, `FIND_ITEM`,
`FIND_PLACE`), six triggers (`KILL_MONST`, `KILL_UNIQUE`, `KILL_WINNER`, `WILD_ENTER`,
`KNOW_ARTIFACT`, `FIND_SHOP`) and a four-state lifecycle (untaken → taken → completed →
finished), all in [defines.h:180](../../archive/zangband/src/defines.h#L180).

4.2 has **two** quests — Sauron and Morgoth — in
[quest.txt](../../lib/gamedata/quest.txt). Its header is unusually encouraging: *"It would
also only require fairly small game code changes to allow other quests to be added as a
result of game events."* The existing structure is a reasonable base to extend rather than
replace.

---

## 3. Decisions recorded

**W-1 — The wilderness is built on 4.2's chunk abstraction, not a parallel world model.**
Each wilderness block becomes a named chunk generated on demand from a per-block seed and
cached in `chunk_list`. Rationale: reuses 4.2's existing generation, save and restore
machinery rather than reimplementing Zangband's `rg_list`/region system alongside it. Under
DEC-11 we are free to reshape `chunk_list` itself to suit.

**W-2 — Seed-derived generation, not stored terrain.** Following Zangband's `place_type.seed`
approach, wilderness blocks are reproducible from a world seed plus block coordinates. Only
blocks the player has *changed* need persisting. Rationale: 4.26M grids cannot be held in
memory or written to a savefile whole.

**W-3 — The fields system is not imported.** Buildings are implemented on 4.2's terrain and
store mechanisms; Zangband's traps and doors are ignored in favour of 4.2's existing
equivalents. Only the building subset of `t_info.txt` is mined, for its building list and
behaviour, not its implementation. Rationale: §2.5 — avoids a second parallel trap system
and removes ~1,814 lines plus integration risk from scope.

**W-4 — Quests extend 4.2's quest structure rather than replacing it.** Rationale: §2.6 —
the existing structure and its data file are a workable base, and upstream's own comment
indicates the extension points were anticipated.

---

## 4. Requirements

### Wilderness representation

**WLD-01 — The world is a grid of wilderness blocks, each a `struct chunk`.** Block
dimensions follow Zangband's 16 × 16 grids unless playtesting shows otherwise.

**WLD-02 — World dimensions are data constants in `constants.txt`, not compile-time
defines.** Zangband's 129 × 129 is the reference value, not a mandate. Rationale: §2.4 — the
wilderness is the project's largest performance and generation risk, and the ability to
shrink it for testing without a rebuild is worth more than fidelity to a constant.

**WLD-03 — Wilderness blocks are generated deterministically from `(world_seed, block_x,
block_y)`.** Regenerating a block the player has not modified must produce identical
terrain. Implements W-2.

**WLD-04 — Only blocks modified by the player are persisted; the rest are regenerated.**
The savefile stores the world seed, the modified-block set, and place metadata.

**WLD-05 — A bounded cache holds live blocks, evicting by distance from the player.**
Zangband's `WILD_VIEW` of 9 × 9 blocks is the reference working set.

**WLD-06 — `chunk_find_name()`'s linear scan must be replaced before the wilderness lands.**
[gen-chunk.c:114](../../src/gen-chunk.c#L114) walks `chunk_list` comparing strings. That is
fine for 4.2's single saved town and O(n) per lookup with hundreds of cached wilderness
blocks. Rationale: a real defect introduced by the change of scale, not a pre-existing bug —
4.2 is correct for the workload it has.

### Terrain generation

**WLD-07 — Terrain type is selected by position in a height/population/law parameter
space.** Reproduces Zangband's decision-tree model (§2.1), with the 232 `w_info` block types
as the content source. Under DEC-20 the selection and fractal generation algorithms are
ported from [wild1.c](../../archive/zangband/src/wild1.c) rather than reimplemented; only
the structures they write into are ours, per W-1.

**WLD-08 — The world includes sea, lakes, rivers and roads connecting towns.** Zangband's
proportions — sea 1/4 of the map, 4 lakes, roads within `ROAD_DIST` 30 — are the reference
values and belong in `constants.txt` per WLD-02.

**WLD-09 — Wilderness terrain features are expressed in 4.2's `terrain.txt`**, extending it
where Zangband has features 4.2 lacks. No parallel feature system.

### Towns and places

**WLD-10 — The world contains multiple towns and multiple dungeon entrances.** Zangband's
20 and 20 are reference values, in `constants.txt` per WLD-02, with minimum separation
constraints.

**WLD-11 — Towns vary by size and inhabitant type.** Zangband's four size bands (small,
town, city, castle, keyed on population) and six inhabitant types (villager, elves, dwarf,
lizard, monster, abandoned) are the reference taxonomy.

**WLD-12 — A designated starting town has a fixed store set.** Zangband's `START_STORE_NUM`
is 7; 4.2 ships 8 stores. Rationale: the opening experience should not depend on procedural
luck.

**WLD-13 — Each town is a named persistent chunk.** Uses the mechanism `chunk_list` already
provides for 4.2's single town (§2.3).

**WLD-14 — Dungeon entrances carry their own depth range and character.** Zangband's
`dun_type` ([types.h:1690](../../archive/zangband/src/types.h#L1690)) carries `min_level`,
`max_level`, an object theme, a habitat flag set, room types available, floor and liquid
terrain, and a per-dungeon `recall_depth`. This is what makes dungeons distinct rather than
interchangeable, and is required for WLD-10 to be meaningful.

### Buildings

**WLD-15 — Buildings are placed by suitability in the same parameter space as terrain.**
Zangband's `wild_building_type` scores each building on population, magic and law, plus a
rarity. Rationale: this is what makes a lawful city and a frontier village feel different,
and it is a small mechanism with a large payoff.

**WLD-16 — Buildings are one of general, store, or service.** Zangband's `BT_GENERAL` /
`BT_STORE` / `BT_BUILD` split.

**WLD-16a — Building quality is a generic attribute, not 113 hand-authored entries.**
Zangband's `t_info.txt` lists 113 building types, but these collapse to **40 distinct
concepts** carrying a quality ladder: *Weapon Smiths → Advanced → Expert → Deep → Arcane →
Unique* (6), *Potion Store → Expensive → Deep → Rare → Custom* (5), *Jeweler → Copper →
Silver → Gold → Rare* (5), and so on. Roughly 73 of the 113 entries are tier variants rather
than distinct ideas.

Implement instead as **building type × quality level**, so the ladder is generated rather
than enumerated. Rationale: identical gameplay for a fraction of the data, and the tiers are
not decoration — they are what WLD-15's population/magic/law scoring *selects between*. A
large lawful magical city drawing *Arcane Weapon Smiths* where a frontier village draws
plain *Weapon Smiths* is the entire payoff of that mechanism. Removing the ladder would keep
the machinery and lose its purpose; hand-authoring it would pay 113 entries for a two-axis
idea.

**WLD-16b — The 40 concepts divide by implementation cost, and should be scheduled by
bucket:**

| Bucket | Count | Work |
|---|---:|---|
| Already in 4.2 | ~8 | Free. General Store, Armoury, Weapon, Book, Alchemy, Magic, Black Market and Home all map directly onto [store.txt](../../lib/gamedata/store.txt). |
| New shops | ~17 | Data. Jeweler, Fletcher, Swordsman, Shieldsman, Axeman, Milliner, Statue Store, Figurine Store, Clothes Store, Ammo Supplies, Supplies Store, Warrior Hall, Heavy Armoury, Scroll Store, Potion Store, Magic Store, Rare Book Store. |
| New **services** | **8, not ~15** | Code, one at a time. See WLD-16c. |

> **Correction.** An earlier draft of this table listed ~15 service buildings by name. That
> was wrong: it counted every exotic *name* in `t_info.txt` as a service. Checking
> [bldg.c](../../archive/zangband/src/bldg.c) shows only **eight** have implemented
> behaviour. Library, Map Maker, Mutatalist, Zymurgist, Castle, Bazaar, Flea Market and
> Grocer appear only in `wild2.c` (placement and drawing) and `zborg*.c` (the borg's
> knowledge of building types) — never in `bldg.c`. They are shop variants or empty shells,
> not services. Zangband 2.7.5 routed unimplemented building behaviour through a Lua hook
> (`FIELD_ACT_BUILD_ACT2` in `process_build_hook`) that the shipped scripts never filled —
> the entire `lib/script/` tree is 509 lines and contains no building code.

**WLD-16c — Service buildings are selected on mechanical impact, not name.** The eight with
implemented behaviour, assessed:

| Service | What it actually does | Verdict |
|---|---|---|
| **Magetower** | Teleport network linking towns (`collect_magetower_links`, `record_aura`) | **Keep.** Fast travel is a genuine logistics system once WLD-10 gives us several towns and WLD-02 a large world. Arguably the highest-value building in the game. |
| **Enchant** (Magesmith) | `enchant_item()` — buy to-hit, to-dam, to-AC | **Keep.** Real build decisions and a gold sink. |
| **Recharge** | `building_recharge()` — restore wand and staff charges | **Keep.** Utility plus gold sink. |
| **Healer** | `building_healer()` — cure and restore | **Keep.** Utility. |
| **Quest giver** | `build_has_quest()`, `build_cmd_quest()` | **Keep — not optional.** WLD-19 to WLD-22 need somewhere to take quests from. |
| **Inn** | `inn_rest()`, plus the nightmare vision in `have_nightmare()` | **Keep.** Resting itself duplicates 4.2's, but the nightmare vision is a real effect, and an inn gives a town somewhere to *be* rather than only somewhere to shop. |
| **Weaponmaster** | `compare_weapons()` — hit and critical probability tables | **Cut.** 4.2's object descriptions already show damage, blows and probabilities, and show them better. Redundant on a modern base. |
| **Casino** | Four gambling minigames — in-between, craps, wheel, dice slots (~270 lines) | **Cut.** A money faucet and sink with no other mechanical consequence. Nothing else in the game reads its outcome. |

> **Correction — a ninth service, found in the documentation after DEC-24 was taken.**
> `spoilers/mutation.txt` lists *"using the services of the Chaos Tower"* as one of only six
> ways to remove a mutation (PLR-35). That is a real gameplay function, and it explains what
> the Mutatalist in `t_info.txt` was for. It has **no handler in 2.7.5's `bldg.c`** — the
> feature was documented but its implementation is absent from the last release, presumably
> lost to the unfinished Lua migration.
>
> This does not overturn DEC-24, whose test is mechanical impact: a mutation-removal service
> passes that test comfortably. **Keep it**, but note it must be *written* rather than
> ported — the documentation describes the behaviour, and no source implements it. Schedule
> with M8 (mutations) rather than M5, since it is useless before PLR-13 exists.

**WLD-16d — Quest-giving is a property any building may carry, not a building type.** The
"quest giver" in WLD-16c is a *capability*, not a distinct building. Zangband already worked
this way: `build_has_quest()` resolves through `lookup_quest_building(build_ptr)` against
whichever building the player is standing in
([bldg.c:303](../../archive/zangband/src/bldg.c#L303)), so any building can host a quest.

Rationale: keeping this property-based means quest sources can be extended later without
touching the quest system — the Inn as a quest hub, a Temple offering pilgrimages, a
Magetower commissioning retrieval — where a hardcoded quest-giver building type would need
reworking each time. It costs nothing now and forecloses nothing.

*Noted intent:* the Inn is the natural first candidate beyond the default quest source, and
pairs well with WLD-11's town inhabitant types — who is drinking in a frontier village inn
differs from a lawful city's. Not scheduled; recorded so M5 and M6 do not design it out.

**WLD-17 — Stores reuse 4.2's store system.** [store.txt](../../lib/gamedata/store.txt) and
`src/store.c` are extended with new store types rather than replaced. Implements W-3.

**WLD-18 — Service buildings are implemented as 4.2 terrain with an attached action**, not
as fields. Implements W-3.

### Quests

**WLD-19 — The quest system supports Zangband's six quest types**, per §2.6.

**WLD-20 — Quests have an explicit lifecycle and event triggers.** Four states, six trigger
kinds (§2.6). Rationale: 4.2's model — zeroing a level field on completion — cannot express
a taken-but-incomplete quest, which every Zangband quest type requires.

**WLD-21 — Quests are placed in the world, not only in dungeons.** `QUEST_TYPE_WILD` and
`FIND_PLACE` need world coordinates; `TOWN_QUEST` places are part of the world layout.

**WLD-22 — Quest state persists across saves and is presented to the player.**
`QUEST_FLAG_KNOWN` implies a player-visible quest log.

### Cross-cutting

**WLD-23 — Player position gains world coordinates alongside level coordinates.** 4.2 tracks
`player->depth` and a grid within the current chunk; a wilderness needs block coordinates
too. This touches the savefile — permitted under DEC-07.

**WLD-24 — Level transitions generalise from stairs to world travel.** Entering a dungeon
from the world, leaving to the world, and moving between wilderness blocks are all
transitions 4.2's stair-based model does not currently express.

**WLD-25 — An overhead world map display is required.** Zangband's `w_info` carries an `M:`
field naming the feature to mimic on the overhead map, implying a zoomed-out world view
distinct from the local view.

---

## 5. Risks

1. **Performance is unproven.** 4.26M grids, on-demand generation and a cache eviction
   policy are all new load on a codebase built for one 13k-grid level. WLD-02 exists so the
   world can be shrunk if it proves necessary.
2. **`chunk` may be too heavy per block.** Each carries noise and scent heatmaps, monster
   groups and object arrays sized for a dungeon level. Ninety live wilderness blocks paying
   that overhead may not be acceptable; W-1 may need a lighter variant for wilderness use.
   Not yet measured.
3. **Savefile size** depends entirely on how many blocks the player modifies. WLD-04 bounds
   it in principle; nothing bounds a player who chops down every tree.
4. **This family blocks the others.** Player systems (pets, quests-as-progression) and
   content (themed dungeons, town monsters) both assume a world exists. Sequencing in Phase
   2 should reflect that.

---

## Open questions

1. ~~Does the wilderness need to be Zangband's size?~~ **Settled by DEC-19:** start at
   roughly a quarter of Zangband's linear dimension, tunable via WLD-02, and grow it once
   travel, caching and generation are proven.
2. ~~Are 20 towns and 20 dungeons wanted?~~ **Settled by DEC-19:** fewer and denser. A
   smaller world with distinct places beats a large one with interchangeable ones.
3. ~~How much of the building catalogue is worth having?~~ **Settled.** The "116 buildings"
   are 40 concepts plus a quality ladder; **WLD-16a** generates the ladder rather than
   enumerating it, and **WLD-16c** keeps five services (Magetower, Enchant, Recharge,
   Healer, Quest giver), treats the Inn as optional, and cuts Weaponmaster and Casino.
   Resolved from [bldg.c](../../archive/zangband/src/bldg.c) rather than `docs/town.txt` —
   the source showed only eight services were ever implemented, which the manual's shop
   list does not reveal.
4. ~~Does `struct chunk` need a lightweight wilderness variant?~~ **Measured. No — but one
   allocation must become per-chunk.** See §6.

---

## 6. Measured: the chunk memory footprint

M4 pre-work, answering open question 4. Measured on the real structures rather
than estimated, with a probe compiled against the game's own headers.

| | Bytes |
|---|---:|
| `struct chunk` itself | 144 |
| `struct square` | 40 |
| square info bitflags | 3 |
| noise + scent heatmap, per grid | 4 |
| **Total per grid** | **47** |
| `struct monster` | 424 |

Per-grid cost is modest. A 16 × 16 wilderness block needs **11.8 KB** of grid
data and **1.6 KB** of fixed overhead. Ninety live blocks would be around 1.2 MB
— entirely acceptable.

### The problem is one fixed allocation

`cave_new()` allocates the monster array at `z_info->level_monster_max`
(1024) regardless of the chunk's size:

```c
c->monsters = mem_zalloc(z_info->level_monster_max * sizeof(struct monster));
```

That is **424 KB per chunk**, fixed. For a 16 × 16 wilderness block:

| | 16 × 16 block |
|---|---:|
| Grid data | 11.8 KB |
| Fixed overhead | 1.6 KB |
| **Monster array** | **424.0 KB** |
| **Total** | **437.4 KB** |

**97% of a wilderness block would be an empty monster array sized for a full
dungeon level** — one that cannot possibly be filled, since the block has only
256 grids. At 81 live blocks (Zangband's 9 × 9 view) that is **35 MB**, of which
34 MB is empty.

### Decision: keep `struct chunk`, make the monster array per-chunk

W-1 stands — no lightweight variant is needed, and the divergence one would
cause is not worth it. What is needed is for the monster array to be sized from
the chunk's own area, capped at `level_monster_max` so dungeon levels are
unaffected.

**This is wider than it looks.** `z_info->level_monster_max` is used as the
array bound in eight places outside `cave_new` — `gen-chunk.c`, `generate.c`,
`game-world.c`, `mon-group.c`, `load.c` — so shrinking the allocation alone
would let existing loops run off the end. The change is:

1. Add a capacity field to `struct chunk`.
2. Size it from the chunk's area at allocation, capped at `level_monster_max`.
3. Replace the eight external uses of the global with the per-chunk value.

**WLD-26 — The monster array is sized per chunk, not per game.** A wilderness
block must not carry a dungeon level's monster capacity. Every bound that
currently reads `z_info->level_monster_max` against a specific chunk must read
that chunk's own capacity instead.

---

## 7. Observed from ZangbandTK screenshots

Recorded from the original running, not from source or documentation. Noted for
M5; no action taken yet.

### Towns are walled and moated, with gates

Zangband's towns are **enclosed** — a stone wall with a water moat outside it —
but not sealed the way Angband's town is. A road leaves through a gap in the
wall and crosses the moat, running out into open country.

This sits between the two models considered so far. The town is not a separate
level reached by stairs, and it is not open ground either: it is an enclosed
place *embedded in the continuous surface*, with a defined way in and out. A
player walking the road leaves the town without any transition, but cannot
simply stroll over the wall.

> **Correction.** An earlier reading of a single screenshot concluded the town
> had no wall at all. It does. The road out was visible; the wall was off the
> edge of that view.

### Town gates are locked doors

The gap in the town wall is not an opening but a **door**, and it can be locked:
attempting to pass one gives *"You failed to unlock the door."*

That is a better mechanic than it first appears. Leaving town becomes a small
act rather than an automatic one, a character with poor disarming skill can be
briefly penned in, and the wall means something — a walled town whose gate
always stands open is scenery.

It also fits 4.2 without any new machinery: doors, locks, unlocking, bashing and
the skill checks behind them all already exist. WLD-10's town wall needs a
`FEAT_CLOSED` in it and nothing more.

### Buildings are entered, and hold named people

The town hall is a building the player enters, presenting a named NPC and a
menu:

```
Uldrik (Human)                              Mayor

q) Request quest

ESC) Exit building
```

Three things follow from this:

- **WLD-16d is confirmed by the original.** Quest-giving is attached to a
  building, and the building presents it as one option among several — exactly
  the property-not-type model that requirement asks for.
- **Buildings have occupants with names and roles**, not just services. That is
  cheap flavour with real payoff, and it is how a town hall differs from a shop
  in feel rather than only in function.
- **The interaction is a menu, not a shop screen.** 4.2's store UI is built
  around buying and selling; a building offering "request quest" needs a
  different presentation. WLD-18 says services are terrain with an attached
  action, and this is what that action looks like.

### Town layout differs from Angband's

Buildings are scattered at varying sizes rather than laid out in Angband's
regular rows, and the road is the organising feature. `town_gen_layout()` in
`gen-cave.c` is therefore less reusable for Zangband-style towns than WLD-13
assumed — it places stores along streets on a grid, which is not this.

### The terrain set is wider than WLD-09 assumed

Zangband's `f_info.txt` carries **30 wilderness features** against the 9 added so
far. Seen in play: *"The jungle is impassable."*

| | Zangband | ZangbandZK |
|---|---|---|
| Ground | sand, dirt, wet mud, dry mud, pebbles, patch of grass, long grass, snow | grass, dirt, sand, mud |
| Vegetation | tree, pine tree, dead tree, snow-covered tree, submerged tree, bush, dead bush, **jungle** | tree |
| Water | shallow, deep, very deep | shallow, deep |
| Rock | rock face, snow-covered rock face, rock, stone fence, solidified lava | mountainside |
| Wet ground | swamp, thick swamp | *(mud)* |
| Hazard | shallow lava, deep lava | — |

Three things follow.

**Vegetation is not one thing.** Zangband distinguishes woodland you can push
through from **jungle, which blocks movement outright** — flagged `BLOCK`, and
shown to the player as an impassable wall of green. That is a terrain type doing
real work: it shapes where a traveller can go, which is what makes an overworld
a place to navigate rather than a field to cross. One `FEAT_TREE` cannot express
it.

**Climate varies.** Snow-covered rock, snow-covered trees and plain snow imply
the world has cold regions, which the current height/population/law parameter
space has no axis for. Either a fourth parameter, or latitude derived from
position — Zangband's own approach is worth checking before choosing.

**Terrain carries hazards.** Shallow and deep lava sit in the same list as
water. WLD-08's sea, lakes and rivers are only part of what the surface holds.

None of this is hard — each is a `terrain.txt` entry and a line in
`wild_terrain_feat()`. But WLD-09 currently reads as though the nine features
added were the job, and they are closer to a third of it.

### Still to compare

The project owner intends a manual comparison pass against the original. Areas
where screenshots have already shown more than the requirements capture:

- The full building roster and what each one's menu offers
- Town wall, gate and moat generation
- Townsfolk: who wanders a town, and whether they differ by town type
- The overworld's own furniture — dungeon entrances, roads, rivers as features
