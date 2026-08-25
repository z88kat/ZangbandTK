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

**WLD-04 — The world remembers what the player changed, and forgets it over time.** The unit
of persistence is the *change*, not the block: the savefile stores the world seed and a list
of what has been left behind, and everything else regenerates.

> Amended from "only blocks modified by the player are persisted". Storing whole blocks was
> the wrong unit — a block is 256 grids and a change is usually one of them — and, more
> importantly, *permanent* persistence is the wrong model for an overworld. See WLD-04a.

**WLD-04a — What is left in the wilderness decays.** Drop a sword in open country, walk
away, and the longer you leave it the likelier it is that someone has found it. Come back to
a monster you wounded and it has had time to heal.

*Why this is the right model and not merely a nice one.* Three things fall out of it that
permanent persistence does not give:

- **It is more believable.** A world in which everything you drop stays exactly where you
  dropped it forever is a world with nobody else in it.
- **It bounds the store without an arbitrary cap.** Things decay out of the list, so the
  list does not grow without limit and the savefile does not either. A fixed cap would have
  to evict *something*, and evicting by decay is both cheaper and better-motivated than
  evicting by age or distance.
- **It is cheaper than the alternative.** Less has to be stored, and less has to be stored
  accurately.

*Decay is scaled by the land.* How fast something disappears depends on the block's
population — the same parameter terrain and monster density already come from. A sword left
outside a city gate is gone by morning; one dropped in a waste may lie there for a long
while. This costs nothing, since the parameter is already carried on every block.

*Angband already does the monster half of this.* `restore_monsters()`
([mon-move.c:2007](../../src/mon-move.c)) regenerates monsters and wears off their timed
effects in proportion to elapsed turns, on returning to a frozen persistent level. The
mechanism is 4.2's own; what is new is applying it to the surface.

**WLD-04b — Ordinary monsters are re-rolled rather than remembered; uniques are
remembered.** A named monster you wounded and left must still exist, wherever it has got to
— that is what makes a unique a unique. Everything else in open country is scenery, and
scenery that regenerates is indistinguishable from scenery that recovered and wandered off,
which is what the player would expect to have happened anyway.

This is the cheap half of WLD-04a and it is deliberate rather than a shortcut: it keeps the
store small, it needs no monster serialisation, and it produces the behaviour the model asks
for.

**WLD-05 — ~~A bounded cache holds live blocks, evicting by distance from the player~~.
Withdrawn: the premise no longer holds, as with WLD-06.**

Written when each block was to be its own `struct chunk`, generated and cached individually.
The surface redesign (§7) made blocks the unit of *generation* rather than of level: the
window builds its terrain directly from block data, and no per-block chunk is ever
constructed. The cache was implemented, and then orphaned by the redesign without anyone
noticing — `wild_cache_get()` and `wild_block_chunk()` had no callers outside their own
file.

Recorded rather than quietly deleted, for the same reason as WLD-06: the requirement was
right for the design that prompted it, and correcting that design removed it. Zangband's
`WILD_VIEW` of 9 × 9 blocks survives as the *window* size, which is what it was really
describing.

**WLD-06 — ~~`chunk_find_name()`'s linear scan must be replaced~~. Withdrawn: the premise
no longer holds.**

This requirement was written when each wilderness block was to be its own level, cached in
`chunk_list` — hundreds of entries, each lookup a linear walk comparing strings. The design
has since changed: the wilderness is one continuous surface (§7), blocks are the unit of
generation rather than of level, and they live in `wild.c`'s own bounded cache. The
wilderness contributes roughly one entry to `chunk_list`, not hundreds.

What remains is 4.2's original scan over stored levels, which accumulate only under
persistent levels and only one per level visited. It is called from level preparation and
savefile loading — never per turn — so at a few hundred entries on a cold path there is no
problem to fix.

Recorded rather than quietly dropped, because the reasoning is worth keeping: the defect was
real for the design that prompted it, and correcting that design removed it. Optimising it
now would be work against a problem that no longer exists, which DEC-18 asks us not to do.

### Terrain generation

**WLD-07 — Terrain type is selected by position in a height/population/law parameter
space.** Reproduces Zangband's decision-tree model (§2.1), with the 232 `w_info` block types
as the content source. Under DEC-20 the selection and fractal generation algorithms are
ported from [wild1.c](../../archive/zangband/src/wild1.c) rather than reimplemented; only
the structures they write into are ours, per W-1.

**WLD-08 — The world includes sea, lakes, rivers and roads connecting towns.** Zangband's
proportions — sea 1/4 of the map, 4 lakes, roads within `ROAD_DIST` 30 — are the reference
values and belong in `constants.txt` per WLD-02.

> **Done in M5.** Sea, lakes and rivers first; roads once WLD-10 gave them something to join.
> A road is *routed*, not drawn: Dijkstra over the block map with a cost per terrain, so it
> follows the valleys, avoids swamp, goes round mountains and keeps to the land unless a
> causeway is the only way across. The network is a spanning tree over the towns — so there is
> always a road out of the starting village that reaches every other town — plus a direct road
> between any two towns within `ROAD_DIST` of each other, plus a spur from every dungeon mouth
> to its nearest town.
>
> *Every dungeon, not just every town.* Measured before the spurs existed: six of thirteen
> dungeon mouths happened to sit on a road and the rest were 11 to 62 blocks from one, which
> is up to a thousand grids of open country to search with nothing to follow. Siting cannot
> fix that — a dungeon stands in the country it belongs in, and the deep ones belong far from
> anywhere — so the road goes to it.
>
> *A road is two grids wide.* One grid was the first attempt and is not visible: where a
> one-grid road turns a right angle in the block the player is standing in, the corner is a
> single square of floor at right angles to the way they are going, and it reads as a road
> that stopped. Reported from play as a road that "appears to end at the beach" after a long
> walk. Three was the second attempt and read as a motorway — also reported. Two makes the
> corner a two-by-two block of paving, which is enough to see a turn as a turn. Measured:
> 2.9 per cent of blocks carry a road, which the width does not change, since routing is
> what decides it.
>
> *Rivers and lakes are ported from Zangband* under DEC-20:
> [create_rivers()](../../archive/zangband/src/wild1.c#L2205) scatters sources evenly, sorts
> them by height, and joins each to its nearest neighbour with a recursively-halved crooked
> line, striking off any point below sea level so rivers end at the coast instead of fanning
> into deltas. [create_lakes()](../../archive/zangband/src/wild1.c#L2344) drops a small
> plasma fractal at a random spot and abandons the attempt if any of it would land in the
> sea — attempts rather than results, so a world with a lot of coast gets fewer lakes.
>
> *How water is drawn is ours, and the first attempt was wrong.* Zangband ran a per-block
> plasma fractal whose corners were weighted by the neighbouring blocks' water flags; we have
> no per-block scratch buffer, since grids come from a hash of their own position (W-1). The
> first attempt interpolated the flags as a field between block centres, which sounds
> equivalent and is not: a linear ramp from full to nothing over sixteen grids leaves a broad
> near-threshold band, so the result was a fourteen-grid channel with open water speckled
> through the fields either side. Reading the flagged blocks as a **path** — each water block
> joined to its water neighbours, wetness by distance to the nearest segment — gives a river
> of controlled width with ragged banks, and nothing outside the blocks that carry it.

**WLD-08a — Danger in the wilderness is a function of law, and towns are placed to
guarantee a survivable doorstep.** Zangband computed a block's monster depth as
`MAX(1, (256 - law) / 4 - 5)` ([wild1.c:3418](../../archive/zangband/src/wild1.c#L3418)) and
required `law > 230` of a town site
([wild1.c:3328](../../archive/zangband/src/wild1.c#L3328)). The two are one mechanism: since
law is laid down by a fractal and therefore varies smoothly, a town in lawful country sits
in a wide patch of lawful country, and the danger gradient falls out rather than being
imposed.

*Measured.* With the danger formula in and the placement rule left out, a first-level
character walking three blocks from the gate met monsters of dungeon depth **20 to 53**.
With town siting scored on the mean danger within six blocks, the same measurement across
eight worlds gave a town law of **206 to 247** and a mean surrounding danger of **1 to 6**,
with danger climbing beyond it. A fixed `law > 230` threshold does *not* work here and was
tried first: requiring a whole thirty-block footprint above a cutoff fails on most worlds,
and relaxing the cutoff until something passes lands wherever the ladder stops.

**WLD-09 — Wilderness terrain features are expressed in 4.2's `terrain.txt`**, extending it
where Zangband has features 4.2 lacks. No parallel feature system.

### Towns and places

**WLD-10 — The world contains multiple towns and multiple dungeon entrances.** Zangband's
20 and 20 are reference values, in `constants.txt` per WLD-02, with minimum separation
constraints.

**WLD-11 — Towns vary by size and inhabitant type.** Zangband's four size bands (small,
town, city, castle, keyed on population) and six inhabitant types (villager, elves, dwarf,
lizard, monster, abandoned) are the reference taxonomy.

**WLD-11a — Towns differ in which stores they hold.** Not every town has a Black Market,
and a frontier village should not carry the same eight shops as a city. Which stores stand
in a town follows from its size band and its position in the parameter space — population
argues for more of them, law for the respectable ones, lawlessness for the Black Market —
so this is WLD-15's scoring applied to 4.2's store list rather than to a building
catalogue.

*Built through `town_gen_layout()`, not around it* (DEC-27). 4.2's town generator already
walks `z_info->store_max` and places each store along a street; what it lacks is any reason
to place fewer, or different ones. That is the change: a per-town store set, chosen once
when the town is placed, that the generator loops over instead of over every store in the
game.

*Deferred to M5, and deliberately.* It needs several towns to mean anything — with one town
the only visible effect would be a character whose home happens to lack a shop, which is
WLD-12 saying no. Recorded here so that M5 builds it as variation between towns rather than
as randomness within one.

**WLD-12 — A designated starting town has a fixed store set.** Zangband's `START_STORE_NUM`
is 7; 4.2 ships 8 stores. Rationale: the opening experience should not depend on procedural
luck. This is the exception WLD-11a is measured against: everywhere else varies, home does
not.

**WLD-13 — ~~Each town is a named persistent chunk~~. Superseded by DEC-26.** A town is not
a level, so `chunk_list` has nothing to hold. Its layout regenerates from the world seed and
its own position; what the player *changes* in it persists by the same mechanism as anything
else on the surface (WLD-04), rather than by a mechanism of the town's own.

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

> **Done in M5**, together with WLD-16a, which is the thing it selects between.
>
> *Magic is a fourth fractal on the block map.* Population and law were there; magic was
> not, and adding it is what made the scoring worth having rather than a formality — see
> DEC-34 for the measurement. Free to carry: the world map is never written to a savefile,
> it regenerates from the seed (WLD-03), so a fourth byte per block costs nothing but the
> fractal.
>
> *Which axis matters depends on the trade.* A magic shop or a bookseller climbs on magic;
> arms and armour on population and order with magic counting for the enchanted top of that
> ladder; a general store on population alone, there being no arcane bread. The band counts,
> and the block's own seed breaks ties, so two equally favoured towns need not match.

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

> **Done in M5.** Three rungs above plain — Advanced, Expert, Arcane — in
> [quality.txt](../../lib/gamedata/quality.txt), each carrying a level bonus and extra shelf
> slots, applying to any trade. Adding a fourth rung is adding a record; nothing in the code
> counts them. 113 hand-authored entries replaced by 3 records and a score.
>
> *What a tier does, and the part that did not work first time.* It raises the level the
> goods are generated at, which is what `apply_magic()` works from — measured, the top rung
> carries about three times the plusses of a plain shop. It does **not** thereby sell deeper
> goods, and that was the trap: which kind a shop stocks comes from `store.txt` and nothing
> in that list depends on level, so with only the level raised an arcane shop's shelves came
> out no deeper than a plain one's. A long word on the sign and nothing behind it. The tier
> now also makes that many extra draws from the shop's own stock list and keeps the deepest,
> which takes the widest-ranging shop in the game from a mean stock level of 9 to 12.
>
> *Your house is exempt, and that exemption is load-bearing.* Every town keeps a home
> (WLD-11a gives every town a general store and a home whatever else it draws), and they are
> all the same house: the restock below skips `FEAT_HOME`. Asked from play, which is a fair
> sign the machinery reads the other way. The alternative — a house per town — strands a
> character's spare gear in whichever village they were standing in when they outgrew it,
> days of walking away, with nothing in the game to say which village. Guarded by
> there-is-one-home-in-the-world.
>
> *One shop per trade, not one per town.* There is a single `struct store` per trade in the
> whole game, so every town showed the same shelves — true and invisible while there was one
> town. A shop is now restocked when the player takes their custom to a different town, at
> that town's tier, keyed on the town rather than on opening the door so that walking out and
> back in cannot re-roll the shelves. Which town a shop's stock belongs to is saved (stores
> block version 2), or reloading would restock the shop the player was standing in.
>
> *Measured shape of the ladder:* 70 / 18 / 8 / 2 per cent across 2,527 shops in 40 worlds.
> Thresholds taken from centiles of the measured score rather than guessed; the guess was out
> by a factor of ten at the top. See DEC-34.

**WLD-16b — The 40 concepts divide by implementation cost, and should be scheduled by
bucket:**

| Bucket | Count | Work |
|---|---:|---|
| Already in 4.2 | ~8 | Free. General Store, Armoury, Weapon, Book, Alchemy, Magic, Black Market and Home all map directly onto [store.txt](../../lib/gamedata/store.txt). |
| New shops | ~~~17~~ **0** | **None, by DEC-33.** Checked one at a time against 4.2's stock lists, every one of the seventeen sells a narrower selection of goods those eight already carry -- or needs object types 4.2 does not have. The variation passes to WLD-16a's quality ladder. |
| New **services** | **8, not ~15** | Code, one at a time. See WLD-16c. **Done in M5**, less the quest giver (M6) and the Chaos Tower (M8). |

> **Resolved, and this is what resolving it looked like.** The seventeen were
> counted from names, and names are what deceived: *Swordsman*, *Axeman*,
> *Shieldsman*, *Milliner*, *Clothes Store*, *Heavy Armoury* and *Warrior Hall*
> are all subsets of 4.2's Armoury and Weaponsmith, which between them already
> stock swords, polearms, hafted, shields, helms, crowns, gloves, boots, cloaks
> and all three weights of body armour. The *Fletcher* and *Ammo Supplies* are a
> subset of one shelf of the Weaponsmith, which already carries `bow`, `shot`,
> `arrow` and `bolt`. The *Potion*, *Scroll* and *Magic* stores are subsets of
> the Alchemist and the Magic shop; the *Rare Book Store* is what the Black
> Market is for, since it stocks anything.
>
> Two failed for a different reason. The *Jeweler* has nothing mundane to sell:
> every ring and amulet in 4.2 is a significant magic item, so a jeweller is a
> second Black Market, and DEC-29 made the Black Market the reason to travel.
> The *Statue* and *Figurine* stores need `TV_STATUE` and `TV_FIGURINE`, which
> 4.2 has not got -- inventing them is content invention, not porting.
>
> None of which says Zangband was wrong to have them. It was built on 2.8.1,
> whose Armoury and Weaponsmith were far thinner, and subdividing them bought
> real choice at the time. 4.2 spent twenty-five years merging that choice back
> into fewer, better-stocked shops, and this project sits on 4.2 (DEC-27).


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
| **Inn** | `inn_rest()`, plus the nightmare vision in `have_nightmare()` | **Keep.** Resting itself duplicates 4.2's, but the nightmare vision is a real effect, and an inn gives a town somewhere to *be* rather than only somewhere to shop. *The bed is built (M5); the nightmare is not — see below.* |
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

> **A service can be promised and not built, and once was.** Worth recording because the
> failure was invisible from the town's data: `wild_town_services()` granted the town a
> magetower, the savefile said it had one, the world map said so, and there was no magetower on
> the ground. Reported from play twice, in two different towns.
>
> The cause was not placement. Services go on a lot off one of the streets, before the shops,
> and the ruin pass that follows skips any lot that already has a building on it — by asking
> `feat_is_shop()`, and a service is not a shop. So the generator built the magetower and then
> built a ruin over it. About one service in ten went missing that way, and one in six in a
> village, where the ruins have the most lots to themselves.
>
> Four rounds of work went into the wrong half of the problem first — more lots, an earlier
> pass, a systematic sweep instead of sixty random guesses — because "placement is failing" was
> inferred from the service being absent rather than measured. Absent has two causes. When the
> failure to find a lot was finally instrumented it never fired once, in any band: the building
> always went up. Guarded by `every-service-held-is-built`, which walks every band because the
> *village* is the worst case, not the great city — and by the first version of that test
> passing on a lucky seed range while the bug was still live.

> **The inn's nightmare is outstanding, and is no longer blocked.** The bed is
> built and works: it sells a night's sleep, only after dark, and wakes the
> player at dawn.
>
> Zangband reached its sanity blast through that bed, so the two arrived together
> and looked like one feature. `have_nightmare()`
> ([bldg.c](../../archive/zangband/src/bldg.c#L22)) picks a monster from the
> deepest part of the bestiary, works a *power* from its hit dice — plus fifty if
> unique, minus fifty if it comes in packs — and rolls a saving throw. Pass, and
> *"X chases you through your dreams"*. Fail, and *"You behold the [horrific]
> visage of X!"*, the monster's `ELDRITCH_HORROR` lore is marked learned, and a
> race-dependent switch does the damage.
>
> That second half is **CNT-17**, and DEC-32 drops it: the Mythos path is closed.
> So the nightmare is to be built from what 4.2 already has — its timed effects
> will carry a bad night, and the world map will carry a true one — and not on
> top of an insanity system. Zangband's shape is still reusable; its payload is
> not.
>
> Sketched directions and the constraint are recorded in
> [decisions.md](decisions.md) under the outstanding follow-ups. The constraint,
> briefly: no insanity, no amnesia, no mutation trigger. A design needing those
> is CNT-17 wearing a hat.

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

> **Built (M6).** `quest_givers` on `struct wild_town`, a bit per service, and
> `wild_gives_quests()` asks it. `ui_enter_service()` runs the work before the building's own
> business, so walking into an inn that is hiring offers the job before it offers a bed —
> and nothing in that path knows what an inn is. Moving the property to the magetower in
> `wild_town_quest_givers()` is one line, and the magetower starts commissioning retrievals.
>
> *The inn carries it, and the reason is not arbitrary.* It is where people who have been
> somewhere else are sitting — and a town that has fallen keeps no services, so the work
> dries up exactly where you would expect it to without a rule saying so. Measured: 46 towns
> in 96 have work to offer, so it is worth walking to and not everywhere.
>
> *The quest list gained room.* `quest.txt` sizes the array, and everything in it is a quest
> the game is *won* by. Work taken from a building goes in a slot past those —
> `wild:quest-slots` in `constants.txt`, eight of them — and the slot is freed when the job
> is handed back. Guarded by work-never-overwrites-the-endgame, since the two live in one
> array and a bounty written over Oberon would either end the game for killing three orcs or
> quietly make the ending unreachable.
>
> *What it gives is a bounty*, which is the cheapest of WLD-19's six types and the only one
> needing nothing the game has not got — so it is the one that proves the WLD-20 lifecycle
> end to end: taken, carried, completed by killing, reported, paid. The other five are
> WLD-19. **Work is handed back at any hiring building, and that is the design rather than a
> shortcut.** It began as one -- recording the giver means another savefile field -- but the
> owner settled it the better way: whoever wanted the job done happens to be drinking in this
> inn too. The alternative is a character who finished a job in the wrong half of the world
> walking back across it to be paid, which is not an interesting journey, and a coincidence
> of that size is what the novels run on.

**WLD-17 — Stores reuse 4.2's store system.** [store.txt](../../lib/gamedata/store.txt) and
`src/store.c` are extended with new store types rather than replaced. Implements W-3.

> **Met, with nothing to add.** DEC-33 found no new store types worth having, so
> the instruction stands and has nothing to instruct: the eight 4.2 ships are the
> eight, and WLD-16a's quality ladder varies them through the same file. The
> requirement was written to forbid replacing 4.2's store system with Zangband's;
> that has not happened and will not.

**WLD-18 — Service buildings are implemented as 4.2 terrain with an attached action**, not
as fields. Implements W-3.

### Quests

**WLD-19 — The quest system supports Zangband's six quest types**, per §2.6.

> **Three of six built (M6).** `enum quest_type` holds all six; the errands written are
> the bounty (kill so many, anywhere), the delivery (carry word to a named town) and the
> find-place (go and look at somewhere nobody here has been). The other three — a kill at a
> named depth of a named dungeon, clearing open country, and fetching a particular object —
> are values in the same enumeration and checks in the same two places; what is missing is
> the errands, not the machinery.
>
> *The kill check now asks what kind of quest it is looking at*, which it did not have to
> while every quest was about killing. A delivery that named a monster would otherwise have
> been completed by killing one, which is the sort of thing that works for months and then
> reads as nonsense in play. Guarded by a-kill-finishes-only-killing-work.

**WLD-20 — Quests have an explicit lifecycle and event triggers.** Four states, six trigger
kinds (§2.6). Rationale: 4.2's model — zeroing a level field on completion — cannot express
a taken-but-incomplete quest, which every Zangband quest type requires.

> **The four states are in (M6, in progress).** `QUEST_UNTAKEN` → `TAKEN` → `COMPLETE` →
> `FINISHED`, on `struct quest`, saved (quests block version 2; version 1 savefiles read
> back as `TAKEN`, or `FINISHED` where the level was zeroed, which is exactly what version 1
> could express). The level field now means only the depth the quest is at, and `is_quest()`
> asks the state instead — a quest holds its level until it is finished.
>
> *One existing rule had to change, and it is the one worth recording.* 4.2 declares the
> player a winner when no quest has a level left. That is the same as "all of them are done"
> only while every quest in the game comes from `quest.txt` and lasts the whole game. Once a
> quest can be taken from somebody in a town and handed back an hour later, "nothing
> outstanding" is an ordinary afternoon — and the character would be told they had won the
> game for delivering a parcel. Winning now counts **fixed** quests only: the ones from
> `quest.txt`, which exist from birth and have nobody to report to. Guarded by
> winning-counts-only-fixed-quests.
>
> *And a job can be given up*, which the four states did not originally allow for. M6's exit
> asks that a quest can be taken, tracked, completed **and failed**, and nothing could fail:
> a bounty on something twenty levels too deep held a slot until the character died. Handing
> it back over the counter is the failing. Every carried job is listed rather than only the
> first, because being able to give up only the oldest is not a choice. Guarded by
> a-job-can-be-given-up, which checks the slot is usable again afterwards.
>
> *Still to come in M6:* three of the six types — a kill at a named dungeon depth, clearing
> open country, and fetching a particular object — and the simplification that lets work be
> reported at any hiring building rather than the one that gave it.

> **The endgame is undecided, and M6 should not decide it by accident.** The two fixed
> quests are Sauron and Morgoth — Tolkien, and therefore exactly the drift DEC-30 calls a
> defect. Zangband replaced Morgoth with the Serpent of Chaos; an Amber game presumably ends
> at the Courts of Chaos, which is already a dungeon here. Nothing in the machinery above
> assumes which: `fixed` marks "the quests the game ends on" without saying what they are,
> so the content decision can be taken later without reworking the lifecycle. It does need
> taking before M6 closes, since the final quest is the most important quest in the game.

**WLD-21 — Quests are placed in the world, not only in dungeons.** `QUEST_TYPE_WILD` and
`FIND_PLACE` need world coordinates; `TOWN_QUEST` places are part of the world layout.

> **Built (M6).** A quest carries the dungeon it is in and the town it points at. The
> dungeon matters because a depth is no longer a place — the Courts of Chaos run 75 to 110
> and the Abyss 90 to 127, so the ending could have been reached in the wrong one. The town
> matters because a delivery and a find-place are finished by *being somewhere*, which
> `quest_check()` can never notice: it only ever sees a monster die. So arrival is a trigger
> of its own, hooked into the post-move handler, and it is idempotent — it runs on every
> step taken inside a town and completing a quest moves it off `QUEST_TAKEN`, so the second
> step finds nothing and says nothing.

**WLD-22 — Quest state persists across saves and is presented to the player.**
`QUEST_FLAG_KNOWN` implies a player-visible quest log.

> **Built (M6).** `J` shows what the character has taken on, how far along each is, and
> where the travelling ones point. Persisted in the quests block, version 3: the state and
> whether it is fixed at version 2, and the kind, the town it points at and its name at
> version 3 — the name because work taken from a building is named when it is taken and so
> has nowhere else to live.
>
> *The fixed quests are deliberately not listed.* A character is on those from birth without
> being told, and putting "kill the Serpent of Chaos" at the top of a first-level
> character's list gives away the ending and is not news.

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

> **Read this section through DEC-27.** Much of what follows is a description of
> *Angband 2.8.1's town wearing Zangband's paint*, because that is what Zangband
> was built on. The observations are accurate; the conclusion "therefore build
> this" does not follow, and where it was drawn below it has been struck through.
> Angband 4.2's town is a considered redesign and it is the one we keep. What we
> take from Zangband is what the town is *for* — that it stands in a wilderness,
> that there are several of them, that they differ, and that the buildings in
> them do things.

### ~~Towns are walled and moated, with gates~~ — observed, not adopted

**Not being built** (DEC-27). Walls, moats and gates around a rectangular grid of
buildings are what Angband 2.8.1's town looked like; Zangband inherited the shape
and dressed it. Angband 4.2 replaced that town wholesale with the starburst
clearing, and 4.2's is the one we keep. What survives from the observation is
only that the town sits in the country and has a way out of it — which the 4.2
town gets for free once the rock its clearing was cut from is stripped away.

The observation itself, kept because it is accurate and because a later feature
may want it:

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

### ~~Town gates are locked doors~~ — observed, not adopted

**Not being built** (DEC-27), since there is no wall for a gate to sit in. The
mechanic is still a good one and costs nothing to keep on the shelf: if some
later place *should* be sealed — a walled dungeon town, a keep, the approach to
somewhere that ought to be earned — this is how to do it.

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

### ~~Town layout differs from Angband's~~ — and that is fine

Buildings are scattered at varying sizes rather than laid out in Angband's
regular rows, and the road is the organising feature.

The conclusion drawn from this — that `town_gen_layout()` is therefore not
reusable — was wrong twice over. It compared Zangband against **2.8.1's** grid of
rows, which 4.2 no longer has: 4.2's town scatters buildings along streets that
radiate from a crossroads, which is much closer to what the screenshot shows than
the thing being compared against. And under DEC-27 the comparison is not the
point anyway. `town_gen_layout()` is the town generator, and what M5 adds to it
is variation between towns — size, and which stores stand in them — not a
different layout model.

### The terrain set is wider than WLD-09 assumed

Zangband's `f_info.txt` carries **30 wilderness features** against the 9 added so
far. Seen in play: *"The jungle is impassable."*

| | Zangband | ZangbandTK |
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
