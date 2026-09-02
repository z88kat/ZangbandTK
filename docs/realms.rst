The Realms of Magic
===================

.. note::

   **Under construction.** You choose your realms at birth, your character sheet
   records them, and **Sorcery and Chaos are playable** — sixty-four workings in
   eight books between them, and the realm choice decides which books you can
   open. **Trump is deferred**, and you will not be offered it: a realm with no
   books behind it is no longer something you can pick.

Spellcasters in ZangbandTK choose realms of magic at birth, and that choice
defines the character as much as the class does. Angband asks you what you are;
Zangband asks you what you *study*, and this game asks both.

The seven
---------

.. list-table::
   :header-rows: 1
   :widths: 12 12 14 62

   * - Realm
     - Stat
     - You…
     - What it is
   * - **Arcane**
     - INT
     - cast spells
     - The wizard's realm: bolts, bindings, detections and the deep power at
       the end of it.
   * - **Life**
     - WIS
     - recite prayers
     - Healing, blessing, protection, and the banishing of foul things.
   * - **Nature**
     - WIS
     - chant verses
     - Both defensive and offensive; the realm that talks to animals and to
       weather.
   * - **Death**
     - INT
     - perform rituals
     - Necromancy. Draining, cursing, and commerce with what should stay
       buried.
   * - **Sorcery**
     - INT
     - invoke workings
     - The meta-realm: detection, identification, protection, escape and
       advantage. *No direct attacks at all.*
   * - **Chaos**
     - INT
     - unleash invocations
     - Destruction, and the realm that changes the caster as much as the
       target.
   * - **Trump**
     - INT
     - read cards
     - A deck that reaches places and people. Teleportation, summoning, and
       the Amber realm in everything but name.

Four of these are Angband's own, renamed to the names this game uses
everywhere else: *arcane* keeps its name, *divine* becomes **Life**, *shadow*
becomes **Death**, and *nature* keeps its name. Their casting stat, verb and
nouns are Angband's and are unchanged — Zangband kept the same information in
scattered per-class constants, and Angband's way of holding it is better.

.. warning::

   **Arcane is Zangband's Arcane, and that is a change from Angband.** The two
   games disagreed about this realm more than any other: Zangband's Arcane was
   deliberately the weak generalist, every book buyable in town and no
   high-level power at all, while Angband's arcane realm is the Mage's realm,
   with two books of attacks and a *Wizard's Tome of Power* found only in the
   dungeon. Having exactly seven realms means folding them together, and the
   fold went Zangband's way (DEC-49, DEC-50).

   So **a Mage no longer learns its attacks from Arcane.** In thirty-two
   Arcane spells there is one ball, one bolt and one beam. That is not a
   mistake: in Zangband a Mage takes *two* realms, and the second is where the
   damage comes from. Take Chaos alongside it, and the pair is the classic
   Zangband Mage.

Choosing them
-------------

Realm choice is a birth step of its own, after race and class, because which
realms you may study is a property of your **class**. That is what makes the
combination mean something rather than merely being wide: a Paladin is offered
Life or Death and nothing else, and a Mage is offered all seven twice over.

.. list-table::
   :header-rows: 1
   :widths: 22 34 44

   * - Class
     - First realm
     - Second realm
   * - Mage
     - any of the seven
     - any of the seven
   * - Priest
     - Life or Death
     - any of the other five
   * - Paladin
     - Life or Death
     - —
   * - Rogue
     - Arcane, Death, Sorcery or Trump
     - —
   * - Ranger
     - Nature
     - Arcane, Death, Sorcery, Chaos or Trump
   * - Druid
     - Nature
     - —
   * - Necromancer
     - Death
     - —
   * - Blackguard
     - Death
     - —

Two of those rows are worth reading twice. **A Priest's second realm cannot be
Life or Death** — the first slot offers the two priestly realms and the second
offers the other five, so a Priest always ends up with one holy realm and one
that is not. And **a Ranger's first realm is Nature and there is no choice about
it**; you are not asked, because a list of one is not a question.

These entitlements are Zangband's own, taken from the same table Zangband keeps
them in, and the game checks itself against that table rather than against
anybody's idea of what a class ought to study.

Sorcery
-------

The meta-realm, and the first of the three new ones to arrive. Detection,
identification, protection, escape, and every convenience a dungeon delver
wants — and **not one direct attack spell in it**. That is Sorcery's whole
character: it makes everything else you do easier and kills nothing itself.

Four books: the **Beginner's Handbook**, the **Master Sorcerer's Handbook**,
**Pattern Sorcery** (a name this game has a better claim on than Zangband did),
and the **Grimoire of Power**. The first two are sold in town; the last two are
found below.

Its best workings are the ones no other realm has. *Teleport Away* fires a beam
that removes monsters from your path rather than killing them. *Telekinesis*
pulls an object to your feet from anywhere you can reach — the strongest of the
three versions in the game, lifting fifteen pounds a level and needing no sight
of what it takes. *Recharging* is four times the strength of the Arcane spell of
the same name. And *Globe of Invulnerability* makes you briefly untouchable.

.. note::

   **Four of the thirty-two do nothing, and say so.** *Identify True*, *Detect
   Enchantment*, *Self Knowledge* and *Explosive Rune* each need a mechanism
   Angband 4.2 removed or never had — whole-object identification, a
   separate is-this-magical bit, a self-knowledge readout the character sheet
   already carries, and a glyph that explodes rather than wards. They are in
   their books with their levels and costs, described as beyond what the game
   can express, rather than quietly missing: a book with a hole in it and a note
   saying so is honest, and a spell absent without explanation is not.

Trump, and why you cannot choose it
------------------------------------

Trump is in the game as a name and nothing else, and the birth step will not
offer it to you. Fifteen of its thirty-two spells summon a creature, and in
Zangband every one of them turns on the same question: cast it well and the
creature is *yours*, cast it badly and an angry group of them appears. This game
has no way for a monster to be on your side, so all fifteen would come out as
the bad half — *Trump Cyberdemon* would be sixty mana spent to put a hostile
Cyberdemon next to you. That is not a weaker spell, it is the opposite spell, so
Trump waits.

The same guard covers every realm: **if your class has no books in a realm, you
are not offered it.** That was not true before, and choosing Sorcery in the
version before it shipped gave you a character who could never cast anything.

Chaos
-----

Sorcery's opposite in every respect. Sorcery has no attack spell in it; Chaos is
almost nothing else. The spoiler calls it "the very element of unmaking", and
what it offers instead of protection is an arsenal that runs from *Magic
Missile* at level 2 to *Mana Storm* and *Call the Void*.

Four books: the **Sign of Chaos** and **Chaos Mastery** in town, **Chaos
Channels** and the **Armageddon Tome** below.

Nothing was deferred — all thirty-two work — but six of them do something
measurably different from what Zangband's did, because 4.2 has no
disintegration, no rocket and no radiation, and because a spell's blast radius
is a fixed number here rather than one that grows as you do. The trades are
written out in the project's decision record as DEC-53; the two
worth knowing at the table are that **Call the Void no longer punishes you for
casting it beside a wall**, and that **the demon Summon Demon calls is always
hostile**, where Zangband gave it a one-in-three chance of serving you.

**And Chaos spells backfire.** Fail one and you may not simply lose the mana:
the realm produces a chaotic effect instead, and how bad it is depends on how
deep the spell was. *Magic Missile* never backfires. *Call the Void* almost
always does, and what you get ranges from a short teleport, through a room full
of doors and traps, a mutation, a forgotten map and an earthquake, down to eight
monsters landing on you and, at the very bottom, the Ancient and Foul Curse.
This is Zangband's own table and its own scaling — the spoiler warns that "Chaos
spells are known to backfire easily" — and it is the price of the realm having
no protective spells at all.

The **Chaos-Warrior** is the realm's own class: one realm, no choice, every
spell, from level 2. Mages, Priests and Rangers may take Chaos as one of two
realms; a Chaos-Warrior *is* one.

*Polymorph Self* is here too — the same effect the Chaos mutation uses, reached
from a second place. Chaos is the realm that twists its own caster, and that is
the spell that does it.

Arcane
------

The weak generalist, and the one realm where this game and Angband disagree
outright. Detection, escape, the four temporary resistances, cures for poison
and light wounds, *Stone to Mud*, *Satisfy Hunger*, *Recharging* — everything a
delver wants and almost nothing that kills. Its whole offence is *Zap*, a weak
lightning bolt; *Ray of Light*; and *Elemental Ball*, which throws fire,
lightning, frost or acid and does not let you choose which.

Four books: **Cantrips for Beginners**, **Minor Arcana**, **Major Arcana** and
the **Manual of Mastery** — and **all four are sold in town**, for 100, 250,
1000 and 2500 gold. That is the realm's bargain, and it is the reason to take
it: no other realm can be bought outright. Every other realm keeps its two best
books in the dungeon.

Two of the thirty-two do nothing and say so. *Phlogiston* refuels a light
source, which is a command in this game rather than a spell; and *Detect
Enchantment* wants a single is-this-magical bit, which no longer exists now that
an object's properties are runes you learn one at a time.

What has changed, and what has not
----------------------------------

Two of the four realms Angband already had now hold Zangband's spells instead:
**Life** and **Arcane**. Nature and Death still hold Angband's, and will not for
long. This is DEC-50, taken deliberately, and it has consequences worth stating:

- **A Priest's prayers and a Mage's spells are different spells now**, at
  different levels, in differently-named books. A Paladin gains most — Angband
  gave it three of the Priest's books and sixteen prayers, and Zangband's table
  gives it all thirty-two.
- **Savefiles from before the change do not load.** A book's identity is its
  name, and every old save carries the town temple's stock, so deleting
  *[Novice's Handbook]* made every one of them unreadable. The game is
  pre-release and this was chosen with the numbers in hand.

**Sorcery, Chaos and Trump took nothing away**, because no class carried them
before. Their books go on the *end* of the lists that gain them — a Mage's four
Arcane books are books one to four, with Sorcery's as five to eight and Chaos's
as nine to twelve.

That ordering is not tidiness. Your known spells are recorded by their position
in a flat list across all of your class's books, so appending leaves every
position meaning what it meant, while inserting anywhere else would shift them:
the game would load, the sheet would look reasonable, and a Priest who had
learned Remove Fear would find they knew something else.

One duplicate is visible and intended: a Mage who takes Arcane and Sorcery has
*Phase Door* in two books, because Zangband put it in both realms and both are
that character's. You will only ever see the books of the realms you chose, so
the duplicate is exactly the one Zangband showed.

That is not a courtesy, it is a hazard avoided. A character's known spells are
stored by their position in a flat list across all of their class's books, so
inserting a book anywhere but the end would silently shift every spell a saved
character knows one place along — the game would load, the sheet would look
reasonable, and a Priest who had learned Remove Fear would find they knew
something else instead.
