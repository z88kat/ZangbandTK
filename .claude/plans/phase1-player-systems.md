# Phase 1 Requirements — Player Systems

**Status:** draft · **Phase:** 1 (requirements) · **Feeds:** Phase 2 development plan

Covers races, classes, magic realms, mutations, virtues and pets — DEC-04's second feature
family. Balance figures come from
[phase1-balance-calibration.md](phase1-balance-calibration.md); project-wide decisions from
[decisions.md](decisions.md). Assumes the world from
[phase1-world-and-towns.md](phase1-world-and-towns.md) exists.

---

## 1. Code volume being drawn from

| Zangband source | Lines | Purpose |
|---|---:|---|
| [mutation.c](../../archive/zangband/src/mutation.c) | 2,056 | 96 mutations: gain, lose, activate, describe |
| [racial.c](../../archive/zangband/src/racial.c) | 761 | Per-race activatable powers |
| [avatar.c](../../archive/zangband/src/avatar.c) | 412 | 18 virtues and their accounting |
| pet handling | — | No single file; threaded through `cave.c`, `melee2.c`, `monster2.c`, `mspells2.c`, `spells1-3.c`, `cmd4.c`, `cmd5.c` |

The pet system having no home of its own is the important entry in that table. It is not a
module to port; it is a property that pervades monster handling.

---

## 2. Where the two versions stand

### 2.1 Races — mostly content

| | Zangband | 4.2.6 |
|---|---:|---:|
| Count | 31 | 11 |

Shared: Human, Half-Elf, Elf, Hobbit, Gnome, Dwarf, Half-Orc, Half-Troll, High-Elf, Kobold.
4.2-only: Dunadan. **Zangband-only: 21** — Amberite, Barbarian, Half-Ogre, Half-Giant,
Half-Titan, Cyclops, Yeek, Klackon, Nibelung, Dark-Elf, Draconian, Mindflayer, Imp, Golem,
Skeleton, Zombie, Vampire, Spectre, Sprite, Beastman, Ghoul.

4.2 defines races in [p_race.txt](../../lib/gamedata/p_race.txt), so most of this is data
entry. The exceptions are races with active powers, which is what
[racial.c](../../archive/zangband/src/racial.c) exists for.

### 2.2 Classes — content plus code

| | Zangband | 4.2.6 |
|---|---:|---:|
| Count | 11 | 9 |

Shared: Warrior, Mage, Priest, Rogue, Ranger, Paladin.
**Zangband-only: 5** — Warrior-Mage, Chaos-Warrior, Monk, Mindcrafter, High-Mage.
4.2-only: Druid, Necromancer, Blackguard.

The Zangband-only classes are not merely stat blocks. Chaos-Warrior has patron rewards,
Monk has martial arts progression and unarmed combat, Mindcrafter has an entirely separate
psionic power list. Each carries real code.

Class data, verified against the original's own character creation screen:

| Class | STR | INT | WIS | DEX | CON | CHR | Hit die | Experience |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Warrior | +5 | -2 | -2 | +2 | +2 | -1 | 9 | 0% |
| Mage | -5 | +3 | 0 | +1 | -2 | +1 | 0 | 30% |
| Priest | -1 | -3 | +3 | -1 | 0 | +2 | 2 | 20% |
| Rogue | +2 | +1 | -2 | +3 | +1 | -1 | 6 | 25% |
| Ranger | +2 | +2 | 0 | +1 | +1 | +1 | 4 | 30% |
| Paladin | +3 | -3 | +1 | 0 | +2 | +2 | 6 | 35% |
| **Warrior-Mage** | +2 | +2 | 0 | +1 | 0 | +1 | 4 | 50% |
| **Chaos-Warrior** | +2 | +1 | 0 | +1 | +2 | -2 | 6 | 35% |
| **Monk** | +2 | -1 | +1 | +3 | +2 | +1 | 6 | 40% |
| **Mindcrafter** | -1 | 0 | +3 | -1 | -1 | +2 | 2 | 25% |
| **High-Mage** | -5 | +4 | 0 | 0 | -2 | +1 | 0 | 30% |

> **These figures were wrong until checked against the original.** `player_class` places
> sixteen skill fields between the stat adjustments and the hit die, where `player_race`
> places eight, and the first extraction read the same offsets for both — producing hit dice
> and experience factors taken from the middle of the skill block. The stat columns were
> right throughout, being first in the struct.
>
> A useful reminder that BAL-08's rule — check the consuming code, do not infer from
> position — applies to reading Zangband's tables as much as to converting its data. The
> race figures in PLR-01 come from a struct whose layout was separately confirmed, and the
> Amberite row was checked on screen.

Note that CHR appears in both tables. 4.2 removed it (P-5's companion rejection), so those
columns are recorded for fidelity and will be dropped on import.

### 2.3 Realms — the structural difference

This is the one genuine architectural fork in this family.

| | Zangband | 4.2.6 |
|---|---|---|
| Realms | 7 — Life, Sorcery, Nature, Chaos, Death, Trump, Arcane | 4 — arcane, divine, nature, shadow |
| Player choice | **Yes** — casting classes choose realms at birth | **No** |
| What a realm is | A distinct spell list the character selects into | Metadata on a class's fixed spell list: casting stat, verb, spell noun, book noun |

Sources: [defines.h:887](../../archive/zangband/src/defines.h#L887) and
[realm.txt](../../lib/gamedata/realm.txt).

In Zangband a Mage picks two realms from six, and that choice — not the class alone —
defines the character. In 4.2 a class has one fixed spell progression and the realm is
descriptive. Adopting Zangband's model means reworking how spells attach to characters, not
adding spell content.

Observed at the original's character creation:

- **Realm choice is a birth step of its own**, after race and class, presented as a fourth
  menu column alongside them.
- **Which realms a class may take is restricted.** A Paladin is offered Life or Death only,
  not the full seven. So realm availability is a property of the class, not a free choice —
  which is what makes the combination meaningful rather than merely wide.
- **The game explains the realms' character** at the point of choosing: *"Life and Sorcery
  are protective, Chaos and Death are destructive. Nature has both defensive and offensive
  spells."*

That Paladin offers a realm choice at all is worth noting for **PLR-12**: the realm system
reaches classes Angband and Zangband share, not only the five that are new. A Paladin in 4.2
has a fixed divine progression; in Zangband they pick a side. Re-expressing 4.2's classes as
realms is therefore not a tidying-up exercise — it changes shared classes too.

### 2.4 Mutations — no analogue in 4.2

96 flags in three groups
([defines.h:462](../../archive/zangband/src/defines.h#L462)):

- **MUT1 (32) — activatable powers.** Spit acid, breathe fire, hypnotic gaze, telekinesis,
  teleport, mind blast, vampirism, blink, eat rock, earthquake, Midas touch, banish, laser
  eye…
- **MUT2 (32) — behavioural and random.** Berserk rage, cowardice, random teleport,
  hallucination, scorpion tail, horns, beak, attract demons, produce mana, speed flux, eat
  light, raw chaos…
- **MUT3 (32) — permanent physical.** Hyper-strength, puny, hyper-intelligence, moronic,
  resilient, albino, flesh rot, extra eyes, magic resistance, infravision, extra legs, fire
  body, iron skin, wings, fearless, regeneration, ESP, arthritis, good and bad luck…

4.2 has nothing equivalent. The nearest structures are
[shape.txt](../../lib/gamedata/shape.txt) (9 shapechanges — temporary and wholesale, not
cumulative) and the curse system (attached to objects, not the player).

The useful hook is [player_property.txt](../../lib/gamedata/player_property.txt), which
already carries 17 `type:player` entries expressing exactly this kind of thing in data —
"Extra Shots", "Blessed Fighter", "Relentless [30]". The representation fits; what is
missing is runtime acquisition, since player properties are currently static grants from
race and class.

### 2.5 Virtues — no analogue in 4.2

A pool of 18 virtues ([defines.h:4923](../../archive/zangband/src/defines.h#L4923)):
Compassion, Honour, Justice, Sacrifice, Knowledge, Faith, Enlightenment, Mysticism, Chance,
Nature, Harmony, Vitality, Unlife, Patience, Temperance, Diligence, Valour, Individualism.

Slot 8 is *Mysticism*, which is the string a player saw
([avatar.c:30](../../archive/zangband/src/avatar.c#L30)); its constant is `V_ENCHANT`
([defines.h:4930](../../archive/zangband/src/defines.h#L4930)). This document previously
listed the constant name.

**A character carries eight of the eighteen, not all of them.** `MAX_PLAYER_VIRTUES` is 8
against `MAX_VIRTUE` 18 ([defines.h:4942](../../archive/zangband/src/defines.h#L4942)), and
`get_virtues()` fills the eight slots by class, then race, then each realm, deduplicates
them, and pads whatever is left with weighted-random draws. A Chaos-Warrior gets Chance and
Individualism; a Monk gets Faith, Harmony, Temperance and Patience; an Amberite gets Honour;
a Yeek gets Sacrifice. Which virtues a character has is part of who they are.

Only 412 lines, but the accounting is invasive: virtues move in response to kills, spell
use, item use and other actions scattered across the codebase — **168 `chg_virtue()` call
sites across 18 files**. The cost is in the call sites, not the module.

> **Zangband never read them.** Outside `avatar.c` nothing in 2.7.5-pre1 branches on a
> virtue value: 168 writers, zero gameplay readers, and the knowledge-menu display commented
> out. See DEC-39, which records the decision to keep virtues anyway and what that commits
> us to.

### 2.6 Pets — the highest-risk item in the project

Zangband has a full companion system: nine player commands (stay close, follow me, seek and
destroy, allow space, stay away, open doors, take items, info, dismiss), distance bands per
mode, and an `RF6_FRIENDLY` flag with an `is_pet()` predicate
([defines.h:742](../../archive/zangband/src/defines.h#L742)).

**Angband 4.2 has no concept of a non-hostile monster.** A search of the entire `src/` tree
for "friendly" returns two hits: a comment in `main-sdl.c` and a shopkeeper's greeting in
`ui-store.c`. Every monster is an enemy by construction, and that assumption is baked into
targeting, AI goal selection, projection and spell effects, summoning, combat resolution
and display.

Adding pets is therefore not a feature addition. It is a change to an invariant that the
monster subsystem is built on.

---

## 3. Decisions recorded

**P-1 — Races and classes are added as data first, code second.** The 21 new races and 5
new classes land in `p_race.txt` and `class.txt`, with code written only for those that
genuinely need it (racial powers, Monk unarmed combat, Chaos-Warrior patrons, Mindcrafter
powers). Rationale: §2.1 and §2.2 — most of the value is content that 4.2's data files
already express.

**P-2 — 4.2's own classes are kept.** Druid, Necromancer and Blackguard stay. Rationale:
they are good classes, DEC-11 removes any need to minimise divergence, and deleting working
content serves no one.

**P-3 — Mutations are represented as runtime-acquired player properties.** Extends
`player_property.txt` rather than introducing a parallel flag system. Rationale: §2.4 — the
representation already exists and is data-driven; only the acquisition path is new.

**P-5 — Gender selection is not carried over.** Confirmed by project owner.
Zangband asks the player to choose a gender at birth, as Angband 2.8.1 did;
Angband 4.2 removed it. We follow 4.2.

Rationale: it carries no mechanical weight in Zangband either — it selects
pronouns and a few flavour strings — so reintroducing it would mean touching
character creation, the savefile and every description path that refers to the
player, for nothing that changes play. It belongs with `CHR`, which 4.2 also
removed and which is already recorded as rejected: both are part of the
2.8.1-era character model that Angband has since moved away from, and DEC-18
holds that where we keep 4.2's approach over Zangband's it is usually the
better engineering rather than a compromise.

**P-4 — Pets are scoped as a monster-subsystem change, not a feature.** Phase 2 must plan
it as such, with the hostility invariant addressed explicitly before any pet-granting
content is written. Rationale: §2.6.

---

## 4. Requirements

### Races and classes

**PLR-01 — The 21 Zangband-only races are available**, with stats, skills, hit dice and
`expfact` taken from Zangband (DEC-08's finding that `expfact` is meaningfully different for
the exotic races — Half-Titan 255, Draconian 250, Amberite 225 — makes these values part of
the race's design, not drift).

> **Nine of the twenty-one are in (M7), curated rather than imported — see DEC-35.** Stats,
> hit dice and four of the seven skills transfer verbatim, which was measured against the ten
> races the two games share rather than assumed. Experience factors are Zangband's, which is
> the point of the requirement: 4.2 flattened nearly every race to 120 and Zangband used the
> number as its balance dial.
>
> Guarded by every-race-is-playable, which exists because a race file that will not parse
> takes nineteen unrelated suites down with it and says only "Cannot initialize player races".

**PLR-02 — Races may grant activatable powers.** Reproduces
[racial.c](../../archive/zangband/src/racial.c). 4.2 has no per-race activatable ability
mechanism; this is new.

**PLR-03 — The 5 Zangband-only classes are available:** Warrior-Mage, Chaos-Warrior, Monk,
Mindcrafter, High-Mage.

**PLR-04 — Monk unarmed combat is implemented** as a distinct progression, not as a
weaponless penalty.

**PLR-05 — Chaos-Warrior patrons are implemented**, including their reward and punishment
events. This is the class's entire identity.

**PLR-06 — Mindcrafter psionic powers are implemented** as a power list independent of the
realm system.

**PLR-07 — 4.2's Druid, Necromancer and Blackguard are retained.** Implements P-2.

### Magic realms

**PLR-08 — Casting classes select magic realms at birth.** Reproduces Zangband's model
(§2.3); the number of realms selectable is per-class.

**PLR-09 — Seven realms exist:** Life, Sorcery, Nature, Chaos, Death, Trump, Arcane.

**PLR-10 — 4.2's realm metadata is retained and extended, not replaced.** Casting stat,
verb, spell noun and book noun are useful per-realm properties that Zangband expressed less
cleanly.

**PLR-11 — Realm choice is recorded in the savefile and shown in character display.**

**PLR-12 — 4.2's existing class spell progressions are re-expressed as realms** so that
retained classes (PLR-07) work within the new system rather than beside it. Rationale:
two parallel magic systems would be worse than either.

### Mutations

**PLR-13 — The three mutation groups are implemented:** activatable, behavioural/random,
and permanent physical (§2.4).

**PLR-14 — Mutations are acquired and lost at runtime** through chaos effects, specific
monsters and items.

**PLR-15 — Mutations are represented as player properties**, implementing P-3, with runtime
grant and revoke.

**PLR-16 — Active mutations appear in a player-invocable power list**, alongside racial
powers from PLR-02.

**PLR-17 — Mutations are visible in the character sheet**, including their effects.

**PLR-34 — Mutations are acquired through the documented paths**, from
`spoilers/mutation.txt` (DEC-16): the Chaos *Polymorph Self* spell; the Polymorph Self
mutation itself; failing a Chaos spell; failing a Death spell from the Necronomicon; being
hit by Chaos or Toxic Waste without chaos resistance; a Chaos Patron's level-up reward; the
"Chaos deities give you gifts" mutation; and being a Beastman (PLR-36). Rationale: mutations
are meant to arrive through chaos exposure, not from a generic random source — the delivery
mechanism is the flavour.

> **One documented path is deliberately absent.** Zangband also granted mutations on being
> sanity-blasted by an Eldritch Horror. That path went with CNT-17, dropped by DEC-32 and
> confirmed closed by the project owner: sanity is not being implemented, and it is not part
> of the Amber chronicles. The remaining eight paths are all chaos exposure of one kind or
> another, which is the flavour this requirement is actually about.
>
> **And a second is closed by an earlier decision, which this requirement did not record.**
> "Hit by Chaos *or Toxic Waste*" is two paths, and only the Chaos half exists. Toxic waste
> is Zangband's `GF_NUKE`, and 4.2 has no such element: the conversion maps `BA_NUKE` to
> `BA_POIS` and `BR_NUKE` to `BR_POIS` in [spellmap.toml](../../tools/zconv/spellmap.toml),
> so every monster that breathed toxic waste in Zangband breathes poison here and there is
> no toxic waste left to be hit by. Zangband's own handler mutates the player one time in
> five when the poison lands, and of those one in four is a full Polymorph Self
> ([spells1.c:3215](../../archive/zangband/src/spells1.c#L3215)).
>
> Giving poison that behaviour instead was rejected: poison is one of 4.2's commonest damage
> types, carried by dozens of monsters from the first dungeon level, and hanging a mutation
> chance on it would mutate low-level characters constantly — which is the opposite of the
> rarity this requirement is about. The path is closed, not deferred: it needs an element
> the game does not have and should not gain for this.
>
> So **six of the eight paths are live**, two wait for the magic realms (Chaos *Polymorph
> Self*, and a failed Chaos or Death spell), and this one is closed.

**PLR-35 — Mutations are removed only through the documented paths:** a potion of New Life,
the Trump *Shuffle* spell, acquiring a cancelling mutation (PLR-37), the mutation-removal
building service, Polymorph Self, and the "strangely normal" mutation. Rationale: scarcity
of removal is what makes mutations consequential.

**PLR-36 — Beastmen gain mutations by design:** one at character creation, then a 20% chance
per level gained. This is the race's identity and links PLR-01 to PLR-13.

**PLR-37 — Certain mutations cancel one another**, with seven documented groups: computer
brain / moronic; puny / superhuman; iron skin / (scales, rotting flesh, warts); rotting flesh
/ regeneration; cowardice / fearlessness; limberness / arthritis; beak / trunk. Gaining one
removes its opposite.

**PLR-38 — Mutation probability is weighted by race.** Vampires gain hypnotic gaze more
readily, Imps horns, Mindflayers tentacles, Yeeks shriek, Beastmen polymorph self. Cheap to
implement and a meaningful differentiator between the new races of PLR-01.

**PLR-40 — Knowledge is a thing that can be lost.** A consumable exists that takes
everything the character knows: the map underfoot, the world map and every place on it,
the monster memory, what every flavoured item is, and every spell learned. Not from
Zangband — it is new, and it earns its place on DEC-30's terms: the first Amber novel
opens on a man with no memory who knows only that there is a place called Amber and that
he belongs to it. That is the one thing this leaves behind.

> **Built.** A mushroom, `Lotus` in [object.txt](../../lib/gamedata/object.txt), which
> looks like every other unidentified mushroom until it has been eaten once. It sets a
> five-turn timed effect that does nothing at all; the forgetting happens when the fuse
> runs out, in the `TMD_LOTUS` case of `process_world()`.
>
> *The delay is the design.* An item that took your memory the instant you ate it would be
> an ordinary bad potion. One that takes it five turns later — "you feel a little dizzy..."
> and then, five turns on, "Where am I?" — is a mistake you have time to understand and no
> time to undo.
>
> *What it will not take.* No experience, no levels, no items, and not the starting village.
> Everything it takes is recoverable by playing, and nothing is recoverable quickly, which
> makes the cost hours rather than a run. The exception for home is not kindness: WLD-12
> makes the starting village always known, the magetower travels only to places the player
> has found, and a character who has forgotten every place including the one they began in
> has a blank map, no fast travel and nowhere to aim for. That is a lost save, not a
> setback. The nine blocks around home stay on the map too, or the village would be a name
> with no ground under it.
>
> *Five kinds of knowledge in five places*, since Angband has no single switch for it:
> `square_forget()` per grid, `wild_forget_knowledge()`, `wipe_monster_lore()` per race,
> `kind->aware` per flavoured kind, and `PY_SPELL_LEARNED` per spell. Guarded by
> the-lotus-forgets-the-world, which checks all five rather than trusting one call, because
> the failure mode here is quietly forgetting to forget something and nothing in play would
> say which.

**PLR-41 — A night at the inn shows the sleeper something.** Zangband's inn carried
`have_nightmare()`, which is the half of DEC-32 that survived: the dream stays, the sanity
blast does not. Built from 4.2's own effects, with the constraint that it must not
reintroduce CNT-17 by the back door — no insanity, no amnesia, no mutation trigger.

> **Built.** Three kinds of night, weighted by the law of the town slept in, because a flat
> roll would make every inn in the world the same inn when there is a parameter space here
> saying how settled a place is.
>
> | | frontier town (law 155) | lawful city (law 254) |
> |---|---:|---:|
> | a true dream | 9% | 25% |
> | a dark dream | 25% | 0% |
>
> Law below about 155 never comes up: a town that far gone has fallen (`wild_town_folk`) and
> keeps no services at all, so there is no inn to sleep in.
>
> *A true dream* puts the nearest place the character has not found onto the world map. It
> marks the block **seen**, not the town **visited** — the magetower travels only to places
> the player has stood in, so a dream tells you where to walk and does not carry you.
> Marking it visited would turn a night's sleep into free passage to anywhere in the world.
> Nearest rather than random, because a vision of the far side of the world is a curiosity
> and one of somewhere three days away is a plan; repeated nights therefore open the map
> outwards, which is the order it would have been explored in anyway.
>
> *A dark dream* draws from monsters the player has actually met — the deepest of three
> draws — which is a change from Zangband's "deepest thing in the bestiary" and a better
> one: a dream about something never encountered is a table lookup. It also scales itself,
> and a new character who has met nothing dreams of nothing. On a made save the dream is
> only remembered; on a failed one the sleeper wakes afraid or confused, both 4.2 timed
> effects and neither of them CNT-17.
>
> This is the mirror of PLR-40, and deliberately: the lotus takes places off the map and the
> dream puts one on. Tir-na Nog'th is a city seen only by moonlight and is already a dungeon
> here, so a night's sleep showing you somewhere is the setting's own furniture.

### Virtues

**PLR-18 — Each character tracks 8 virtues, drawn from a pool of 18** (§2.5), selected at
birth by class, then race, then realm, deduplicated and padded with weighted-random draws.
The selection mechanism is part of the feature, not an implementation detail: which virtues
a character has distinguishes it as much as the values do. Corrected by DEC-39 — this
requirement previously said all 18 were tracked, which the source does not support.
>
> ✅ **Met.** `virtues_select()` in [player-virtue.c](../../src/player-virtue.c), with the
> class, race and realm tables in `class.txt`, `p_race.txt` and `realm.txt` rather than in a
> switch. Zangband's weighted pad is reproduced from its case-range widths.

**PLR-19 — Virtue values change in response to player actions** — kills, spell use, item
use, quest outcomes. ✅ **Met**, at nine hook sites covering all eighteen virtues. **DEC-43**
records why nine and not Zangband's 169: the rule is that every virtue a character can be
given must be one their play can move, and the writers are sized to that.

**PLR-20 — Virtues are displayed to the player** and persist in the savefile. ✅ **Met.**
A `[Virtues]` section in the character sheet, on Zangband's own thirteen-band ladder; and a
savefile block written and read *by count*, in player block version 3, with version 2 kept
as a loader so characters made before this still open.

**PLR-21 — At least one system reads a virtue value and behaves differently because of it.**
✅ **Met, twice.** A Lord of Chaos reaches for the bottom of its reward ladder less often for
a character strong in Chance and Individualism and more often for one strong in Harmony and
Temperance (`patron_roll_slot()`); and the inn dream runs truer for Enlightenment and
Knowledge, darker for Unlife and Chance (`player_night_dream()`). Both sit on top of
mechanisms that already existed rather than needing new plumbing. **The gate is closed and
virtues are kept.**
Rationale: tracked-but-inert numbers are not a feature. **If nothing consumes virtues,
PLR-18 to PLR-20 are cut rather than shipped as decoration** — the gate is on any consumer
at all, not on a particular one.

> This requirement previously named three consumers — Chaos-Warrior patron behaviour,
> artifact outcomes and spell outcomes — and made the cut conditional on all three failing.
> All three were checked against the source and **none of them read virtues**; Zangband had
> no consumers whatsoever. Restated in the general form by DEC-39, which also records the
> decision to keep virtues and the two candidate consumers under consideration (the patron
> ladder and the inn dream). Whatever is built is new design under DEC-30, not a port.

### Pets

**PLR-22 — Monsters carry an allegiance, and hostility is no longer an invariant.** The
foundational change identified in §2.6. Everything else in this section depends on it.

**PLR-23 — Monster AI accounts for friendly monsters** in goal selection, movement and
target choice.

**PLR-24 — Player attacks, spells and projections distinguish friendly from hostile
targets**, including confirmation before harming a pet.

**PLR-25 — Pets are commanded through the nine Zangband modes** (§2.6), with per-mode
distance behaviour.

**PLR-26 — Pets persist across level changes and saves**, following the player where the
mode implies it.

**PLR-27 — Pets are visually distinguishable** in both ASCII and graphical display. Carries
into Phase 3.

**PLR-28 — Summoning and charming produce pets**, giving PLR-22 through PLR-27 a source.
The documented paths are: realm spells that summon or charm, Mindcrafter domination,
Chaos patron gifts, thrown magical figurines, and wands of charm monster. Several are gated
on M9 and on PLR-05/PLR-06.

### Pets — mechanics recovered from the official documentation

The following came from `docs/monster.txt` (DEC-16) and were absent from this document's
first draft. They are not optional detail: two of them are the reason pets are not
overpowered.

**PLR-29 — Allegiance has three states, not two: hostile, friendly, and pet.** Zangband
distinguishes them, and they behave differently:

| State | Hurts player | Commandable | Mana upkeep | Obtained by |
|---|---|---|---|---|
| Hostile | yes | — | — | default |
| Friendly | no, but does not necessarily help | **no** | **none** | found in the dungeon; summoned by a friendly |
| Pet | no | yes (PLR-25) | **yes** (PLR-30) | charm, summon, domination, figurine, patron gift |

This refines PLR-22: a boolean "friendly" flag is insufficient.

**PLR-30 — Pets cost mana upkeep proportional to the sum of their levels.** The first
monster or few are free; beyond that, maintaining control reduces the player's *mana
regeneration rate*, scaling with total pet levels. Rationale: this is the entire balancing
mechanism for pets. Without it, a summoner build trivialises the game — the documentation
warns explicitly about pets that summon more pets.

**PLR-31 — Experience is awarded only for the killing blow.** A pet that kills a monster
earns the player nothing. Rationale: the second half of pet balance, and it interacts
directly with DEC-08 — pets do not become an experience shortcut.

**PLR-32 — Summoned monsters inherit their summoner's allegiance.** A pet's summons are
pets; a friendly monster's summons are friendly. Compounds with PLR-30's upkeep.

**PLR-33 — Annoying a pet or friendly monster turns it hostile.** Area effects that catch
them, aggravation, and similar. Rationale: the documented counterweight to pets, and the
reason PLR-24's confirmation prompt matters.

---

## 5. Risks

1. **Pets are a cross-cutting change to a core invariant** (§2.6). Every place 4.2 assumes
   "monster ⇒ enemy" is a potential defect site, and they are not enumerable by grep. This
   is the most likely source of subtle, long-lived bugs in the project.
2. **The realm rework touches every casting class**, including the three 4.2 classes we are
   keeping (PLR-12). Partial implementation leaves the game with two magic systems.
3. **Mutation count is deceptive.** 96 mutations is 96 distinct effects to implement and
   balance, several of which (polymorph, raw chaos, Midas touch) have wide blast radius.
4. **Virtues risk being decoration.** PLR-21 exists to force the question early.
5. **This family multiplies with content.** Every new race and class interacts with every
   new mutation and realm. Test surface grows faster than the feature list suggests.

---

## Open questions

1. **Are all 21 races and 5 classes wanted?** DEC-02's curated approach invites a subset.
   Yeek and Beastman carry Zangband's character; several others are close variations on
   races already present.
2. ~~Does the realm rework earn its cost?~~ **Settled by DEC-19: committed, scheduled
   late.** The full seven-realm system with player choice is part of the concept, not a
   candidate for the cheaper fixed-progression alternative. PLR-08 to PLR-12 stand as
   written. Sequencing only.
3. ~~Should pets be deferred?~~ **Settled by DEC-19: committed, scheduled late.** Pets are
   wanted. PLR-22 to PLR-28 stand as written. The scheduling consequence is that anything
   touching monster handling before the pet milestone must not foreclose the allegiance
   model of PLR-22 — see the note under DEC-19.
4. **Which mutations are must-haves?** If the count needs cutting, the activatable MUT1 set
   is the most visible to players and the behavioural MUT2 set is the most characterful;
   MUT3's stat modifiers are the most replaceable.
