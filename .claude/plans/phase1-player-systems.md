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

18 virtues ([defines.h:4923](../../archive/zangband/src/defines.h#L4923)): Compassion,
Honour, Justice, Sacrifice, Knowledge, Faith, Enlightenment, Enchantment, Chance, Nature,
Harmony, Vitality, Unlife, Patience, Temperance, Diligence, Valour, Individualism.

Only 412 lines, but the accounting is invasive: virtues move in response to kills, spell
use, item use and other actions scattered across the codebase. The cost is in the call
sites, not the module.

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
"Chaos deities give you gifts" mutation; being sanity-blasted by an Eldritch Horror
(CNT-17); and being a Beastman (PLR-36). Rationale: mutations are meant to arrive through
chaos exposure, not from a generic random source — the delivery mechanism is the flavour.

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

### Virtues

**PLR-18 — The 18 virtues are tracked per character** (§2.5).

**PLR-19 — Virtue values change in response to player actions** — kills, spell use, item
use, quest outcomes.

**PLR-20 — Virtues are displayed to the player** and persist in the savefile.

**PLR-21 — At least one system consumes virtues.** Rationale: tracked-but-inert numbers are
not a feature. Zangband ties virtues to Chaos-Warrior patron behaviour and some artifact and
spell outcomes; if none of those consumers land, PLR-18 to PLR-20 should be cut rather than
shipped as decoration.

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
