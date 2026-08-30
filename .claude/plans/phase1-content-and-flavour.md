# Phase 1 Requirements — Content & Flavour

**Status:** draft · **Phase:** 1 (requirements) · **Feeds:** Phase 2 development plan

Covers monsters, artifacts, ego items, objects, vaults and themed levels — DEC-04's third
feature family. This is the cheapest family per unit of visible change, because 4.2
expresses nearly all of it in data files.

How imported content gets its *numbers* is governed by
[phase1-balance-calibration.md](phase1-balance-calibration.md), not here. This document
governs *which* content and *what it feels like*.

---

## 1. What exists where

Counts are of distinct named entries. "Zangband-only" means present in Zangband 2.7.5-pre1
and absent from Angband 2.8.1; the final column further excludes anything 4.2.6 has since
added under the same name.

| | 2.8.1 | Zangband | 4.2.6 | Zangband-only, absent from 4.2 |
|---|---:|---:|---:|---:|
| Monsters | 537 | 884 | 624 | **387** |
| Artifacts | 113 | 129 | 138 | **51** |
| Ego items | 80 | 99 | 79 | **17** |
| Objects | 359 | 473 | 330 | **135** |
| Vaults | 11 | 125 | 161 | not compared |

Roughly **590 pieces of content** with no counterpart in 4.2. That is the import job.

> **Name-matching caveat.** 2.8.1 and Zangband write object names as `& Long Sword~`; 4.2
> writes `Long Sword~`. The object row above normalises the `&` prefix and `~` suffix before
> comparing — without that, the figure inflates from 135 to 166. Any conversion tooling must
> normalise the same way, and the monster and artifact rows should be re-checked for
> equivalent formatting differences before the counts are treated as final.
>
> **And that is not sufficient, as CNT-11 found.** Normalising the markers makes two names
> comparable; it does not make them the same object. Zangband renamed 26 of the kinds it
> inherited from 2.8.1 without changing what they are — Ring of Skill for Ring of Accuracy,
> Enchant Weapon Deadliness for Enchant Weapon To-Dam, Levitation for Feather Falling — and
> 4.2 still ships ten of the non-spellbook ones under the older name. It also re-used four
> slots for a *different* object, which the same comparison misses in the other direction.
> An object kind's identity in these files is its `(tval, sval)` slot, and comparing slots
> is what the converter does. By that measure the object row's 135 is 82 imported, 28
> spellbooks for CNT-10, 19 deferred for want of a mechanism, 16 artifact bases and 10
> renames.
>
> **The monster, artifact and ego rows have now had the same scrutiny, and each needed a
> different notion of identity** — see `tools/zconv/renames.toml`. Ego indices are 98.9%
> stable across the two versions, so the index is identity and the one divergence was a
> rename: `of Levitation` is Angband's `of Slow Descent`, which 4.2 still ships. Artifact
> indices are 69% stable, and all 35 divergences turned out to be Zangband replacing a
> Tolkien artifact with one of its own in the same numbered slot — replacements, not
> renames, and correctly imported. Monster indices are 0.5% stable: Zangband renumbered
> the whole bestiary, so identity had to come from the *description*, which is
> hand-written prose. 33 monsters keep a 2.8.1 monster's paragraph word for word; 31 are
> Zangband writing a deeper monster on inherited text or 2.8.1 boilerplate shared by
> several monsters at once, and two were renames — `hobo` and `raving lunatic`, which
> Angband still ships as the boil-covered wretch and the village idiot.
>
> Three duplicates in total, now excluded: two monsters and one ego. The artifact row is
> clean.

### 1.1 The object delta is not mostly spellbooks

*This section originally read "The object delta is mostly spellbooks". CNT-11 measured it
and it is not: of the 135, only 28 are realm spellbooks. The rest — 107 slots — is the
larger half, and is what CNT-11 imported. The claim below stands for the spellbooks
themselves and for the gate on them; the "dominated by" reading did not survive contact
with the data.*

The realm spellbooks among them — *Armageddon Tome*, *Black
Channels*, *Black Mass*, *Blessings of the Grail*, *Book of the Unicorn*, *Call of the
Wild*, *Cantrips for Beginners* — are a direct consequence of Zangband's seven-realm
magic system, and it means **that part of the row is gated on PLR-08/PLR-09** in
[phase1-player-systems.md](phase1-player-systems.md). The realm system is committed
(DEC-19), so the gate is on *which* realms land and when — books for a realm that does not
exist have nothing to contain. Sequencing puts both in M9.

### 1.2 Vaults are the one place 4.2 is ahead

Zangband expanded vaults dramatically over 2.8.1 — 11 to 125. But 4.2 has **161** in
[vault.txt](../../lib/gamedata/vault.txt), plus 415 room templates in
[room_template.txt](../../lib/gamedata/room_template.txt). Vanilla has done this work
already and done more of it. Zangband's vaults are worth mining for the distinctive ones,
not importing wholesale.

---

## 2. The flavour, specifically

Zangband's identity is three literary overlays on Angband's Tolkien base. This is what
"the spirit of Zangband" concretely means in content terms.

**Amber (Roger Zelazny).** The largest overlay: 34 Amberite references in the bestiary plus
the princes of Amber as uniques. Twelve carry Zangband's `AMBERITE` flag, and they are the
set: **Oberon, Benedict, Corwin, Eric, Caine, Gerard, Julian, Bleys, Fiona, Brand, Dworkin
and Rinaldo**. It reaches into artifacts (*Grayswandir*, *Frakir*) and ego types
(*Pattern Weapon*, *Trump Weapon*), and into the magic system as the Trump realm. Amber is
not decoration in Zangband; it is load-bearing.

> *This list previously named Random and omitted Oberon, Dworkin and Rinaldo.* It had been
> written from the novels rather than from `r_info.txt`, and **Random is not in Zangband's
> bestiary at all** — checked, not assumed. The roster above is the twelve the flag is
> actually on, which is the same mistake DEC-38 caught in the patron list and the same fix:
> a roster is a fact, so it gets looked up.

**Cthulhu Mythos (Lovecraft).** Nyarlathotep, Hastur, Shub-Niggurath, Azathoth, Yog-Sothoth,
Tsathoggua as deep uniques.

**Chaos.** Cuts across families — the Chaos realm, the Chaos-Warrior class and its patrons,
the `(Chaotic)` ego type, and the raw-chaos mutations in
[phase1-player-systems.md](phase1-player-systems.md) §2.4.

> **Worth knowing before distribution.** The Cthulhu Mythos is public domain. Amber is not —
> Zelazny's estate retains copyright, and Zangband used the material anyway in an era with
> different norms. Names and titles alone are weak copyright subjects, but characters can be
> protected, and this project is intended for distribution. Flagging it as a factor for you
> to weigh, not as a blocker; the Amber content can equally be renamed while keeping the
> mechanics if that is ever preferred.

### 2.1 Themed levels

Zangband weights object generation per dungeon through `obj_theme`
([types.h:206](../../archive/zangband/src/types.h#L206)), four bytes — treasure, combat,
magic, tools — attached to each `dun_type`. A dungeon that yields weapons and one that
yields scrolls feel materially different, and this is the whole mechanism.

4.2's nearest equivalent is [pit.txt](../../lib/gamedata/pit.txt), 40 themed monster pits
and nests (Orc, Troll, dragons by element, Demons, Undead, Spellcasters, Archers, Naga…).
That covers *monster* theming well; it does not cover object theming, which is what
`obj_theme` provides and what makes WLD-14's distinct dungeons distinct.

---

## 3. Decisions recorded

**C-1 — Content is imported as data, not code.** Everything in §1 lands in 4.2's existing
`monster.txt`, `artifact.txt`, `ego_item.txt`, `object.txt` and `vault.txt`. New *mechanics*
that content depends on are requirements in the other two families, not here.

**C-2 — 4.2's content is kept alongside Zangband's.** Import is additive. Rationale:
DEC-11 removes any need to minimise divergence, and 4.2's bestiary and vault collection are
good. Where both have an entry under the same name, 4.2's wins (DEC-10).

**C-3 — Vaults are curated, not bulk-imported.** Rationale: §1.2 — 4.2 already has more
vaults than Zangband did, so only genuinely distinctive Zangband vaults earn their place.

---

## 4. Requirements

### Bestiary

**CNT-01 — The 387 Zangband-only monsters are imported**, with statistics assigned per
BAL-09 and the global lethality scalar of BAL-13 applied.

**CNT-02 — The Amber uniques are present as a coherent set** — the princes named in §2,
with the relationships and relative power that make them recognisable rather than a scatter
of similar uniques.

> **Met.** The twelve arrived with M2, at their own depths and their own strengths, which
> covered *relative power*. What they did not have until now was anything connecting them:
> no shared kind, no group summon, nothing they did as a family. On the requirement's own
> words they were precisely "a scatter of similar uniques" — thirty-four Amber references in
> the bestiary and no Amber in the code.
>
> The `AMBERITE` flag closes that. They are a kind the lore names, the `S_AMBERITES` summon
> calls its own, and a dying one lays a blood curse on whoever killed it. Guarded by
> [monster/amberite](../../src/tests/monster/amberite.c), which fails if the set is not
> exactly twelve.

**CNT-03 — The Mythos uniques are present as deep-level encounters** (§2).

**CNT-04 — Monster flags with no 4.2 equivalent are resolved, not dropped.** Per BAL-10,
each is either mapped to a 4.2 mechanism, implemented, or explicitly rejected with a reason.
Zangband's bestiary uses 118 distinct flag tokens.

**CNT-05 — Town and wilderness monsters are imported** to populate WLD-11's six inhabitant
types. This row is gated on the world existing.

**CNT-20 — Not every creature is there to be fought.** A monster may carry a blessing
instead of a threat: walking into it heals the toucher and it bounds away. Neither Angband's
nor Zangband's — this is ZangbandTK's own, and it fits DEC-30's aim, since a numinous beast
that appears, gives something and is gone is Amber's furniture rather than a dungeon
crawler's.

> **Built.** The `BLESSING` race flag, and a **white deer** in
> [monster.zangbandtk.txt](../../lib/gamedata/monster.zangbandtk.txt) — a third bestiary
> file, since a ZangbandTK original belongs neither in Angband's `monster.txt` nor in the
> generated `monster.zangband.txt`, and the loader's own comment argues for keeping
> provenance visible.
>
> *Once per beast, and that is the whole of the balance.* A full heal for nothing is worth
> having; one that can be had again by following the beast and touching it a second time is
> a character who never buys a potion again. The beast remembers, in `mflag`, which is
> written to the savefile with the rest of the monster — so reloading does not wipe the
> memory either. A second touch only sends it bounding off again.
>
> *Before the fear check*, so a character too frightened to swing can still be blessed —
> which is when they most want it, and better than being told they are too afraid to touch a
> deer. It is not killed and not removed: it goes on living in the world, with nothing more
> to give.
>
> *Ten grids, not five.* The teleport effect picks the grid whose distance best approximates
> what is asked and varies it by up to a quarter either way, so asking for the five it is
> meant to clear lands short about half the time. Measured at ten, the worst of thirty bounds
> is nine.
>
> *Open for tuning:* `rarity:4` at `depth:1`. A full heal per beast is a lucky find at that
> rarity and would become a strategy if they were common; the knob is in the data file.
>
> **And the Unicorn of Amber**, which is what the deer turned out to be for. She carries the
> same `BLESSING` flag and is `UNIQUE`, and *that* is what makes her blessing the greater
> one — no second flag, because being unique is what the difference actually is: there are
> deer, and there is the Unicorn, and one of them is not a kind of thing. She undoes
> everything the town healer sells at once and for nothing: wounds, poison, cuts, stunning,
> blindness, confusion, fear, drained stats, and levels lost to life-draining. Once.
>
> Worth noting against DEC-30, since it is the first content in this project written *from*
> the novels rather than filtered out of Zangband. She is Amber's emblem, she is "she"
> throughout the books, and she is always leaving — which is the mechanic. The general flag
> came first and the character fitted it; that is the right order, and it means the next one
> costs a data record.

### Artifacts and ego items

**CNT-06 — The 51 Zangband-only artifacts are imported**, including the Amber artifacts of
§2.

**CNT-07 — The 18 Zangband-only ego types are imported:** `(Chaotic)`, `(Pattern Weapon)`,
`(Trump Weapon)`, `(Ghoul Touch)`, `(Vampiric)`, `of Electricity` and the remainder.

**CNT-08 — Ego and artifact powers are expressed in 4.2's property system.** 4.2 represents
these through [object_property.txt](../../lib/gamedata/object_property.txt),
[brand.txt](../../lib/gamedata/brand.txt), [slay.txt](../../lib/gamedata/slay.txt) and
[curse.txt](../../lib/gamedata/curse.txt) — structures Zangband did not have. Translation is
required; verbatim flag copying will not work.

**CNT-09 — Zangband powers with no 4.2 property equivalent are implemented as new
properties**, extending the data files rather than special-casing in code.

### Objects

**CNT-10 — Realm spellbooks are imported for whichever realms PLR-09 delivers.** Explicitly
gated: books for realms that do not exist must not be imported.

**CNT-11 — Non-spellbook Zangband-only objects are imported**, subject to the name
normalisation caveat in §1. ✅ **Met** (3.40.0). 82 kinds imported by
`zconv objects`; 19 deferred with a recorded reason, 16 refused as artifact bases and 10
recognised as Zangband's renames of objects 4.2 already ships. Three new object
properties — `STRANGE_LUCK`, `PSI_CRIT`, `NO_MAGIC` — were built for it. The §1 caveat is
amended above: name normalisation is necessary and not sufficient.

### Effects found in the official documentation

Both requirements below come from Zangband's spoilers (DEC-16) rather than its data files,
and were absent from this document's first draft — a useful demonstration of why DEC-16
treats the manuals as a primary source.

**CNT-15 — The Ancient and Foul Curse is implemented.** The Curse of Topi Ylinen, from
[tycurse.txt](https://web.archive.org/web/20220527225941/http://zangband.org/spoilers/tycurse.txt),
is a *cascading* effect rather than a single one: an initial result — aggravate monsters,
summon several strong monsters or groups, summon a monster, lose 1/16 of experience,
paralysis (with a free-action saving throw) — carries a 1-in-6 chance of triggering the next
effect in the chain, which may itself cascade. After the chain resolves there is a further
1-in-3 chance of starting the whole process again, terminating only on paralysis or a
cyberdemon summon. Rationale: the cascade *is* the mechanic; implemented as a flat random
effect it becomes unremarkable.

**CNT-17 — ~~Eldritch Horrors blast sanity on sight~~. Dropped by DEC-32.** The Mythos
path is closed: a mechanic whose purpose is to make the Lovecraft content matter pulls
against DEC-30, which calls that content drift. The inn's nightmare, which Zangband reached
through this, is kept and built without it. What follows is the original entry, left because
the reasoning it records is still the argument against the decision.

From `docs/monster.txt` (DEC-16), and
missing from every earlier draft of this document. Certain monsters are *"so fearsome to
look upon"* that merely seeing them may temporarily drain intelligence or wisdom, or inflict
fear, confusion or amnesia. Per `spoilers/mutation.txt` a sanity blast can also **grant a
mutation** (PLR-34).

Rationale: this is the mechanic that makes the Mythos content of §2 *mean* something. Import
Nyarlathotep and Azathoth without it and they are large monsters with evocative names;
with it, the Cthulhu material has a mechanical identity of its own, distinct from Tolkien's
and Amber's. It is also the clearest single instance of DEC-02's "iconic feature" test.

Note the coupling: CNT-17 is a *content* mechanic that writes into PLR-13's mutation system
and needs a per-monster flag. It should be scheduled with the Mythos monsters of CNT-03, not
deferred to the player-systems family.

**CNT-16 — Objects can receive random abilities on generation.** From
[randabil.txt](https://web.archive.org/web/20220527225941/http://zangband.org/spoilers/randabil.txt):
artifacts, ego items and *ordinary* items may be granted an ability drawn at random from
Extra Sustain, Extra Resistance or Extra Power lists, either guaranteed or by chance. This
is a generation-time mechanism distinct from the fixed properties of CNT-08, and applies to
plain items, not only enchanted ones.

### Levels and theming

**CNT-12 — Dungeons carry an object generation theme.** Reproduces `obj_theme`'s four-way
weighting (§2.1). Required for WLD-14's per-dungeon character to be perceptible.

**CNT-13 — 4.2's monster pit and nest themes are retained and extended** with Zangband's
monster groupings.

**CNT-14 — A curated selection of Zangband vaults is imported**, implementing C-3.

---

## 5. Risks

1. **This family is gated on the other two.** Spellbooks need realms (PLR-09), town monsters
   need towns (WLD-11), dungeon themes need multiple dungeons (WLD-14). Content imported
   ahead of its mechanism is untestable.
2. **Flag translation is where the work actually is.** The counts in §1 make this look like
   data entry. It is not: 118 monster flag tokens and an entire object property model
   separate the two formats, and BAL-08 requires each be checked against its consuming code.
3. **Name matching is unreliable** (§1 caveat). The object count moved by 23% under
   normalisation, and CNT-11 later found normalisation alone still wrong: comparing by
   `(tval, sval)` slot rather than by name moved it again, in both directions. Monsters and
   artifacts have not had the same scrutiny.
4. **Volume invites shallow review.** 590 entries is more than anyone will carefully
   read. BAL-11's conversion report is the only realistic control.

---

## Open questions

1. ~~How much of the 389-monster import is wanted?~~ **Settled by DEC-19: theme first.**
   Everything carrying Amber, Mythos or Chaos identity; the generic tail only where it fills
   a gap.
2. ~~Is the Amber material kept as-is, renamed, or reduced?~~ **Settled by DEC-19: kept
   as-is.** Load-bearing to the spirit, mechanically free, renameable later if it ever
   matters.
3. **Which Zangband vaults are distinctive enough to import?** CNT-14 needs a selection
   criterion. Suggest importing only vaults that carry the Amber, Mythos or Chaos themes,
   since generic vaults are already well covered by 4.2's 161.
4. **Does `obj_theme` survive contact with 4.2's object generation?** CNT-12 assumes the
   four-way weighting can be applied to 4.2's allocation system. Not yet verified — and
   per BAL-08, it must be before CNT-12 is treated as settled.
