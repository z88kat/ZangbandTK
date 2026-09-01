The Realms of Magic
===================

.. note::

   **Under construction.** You choose your realms at birth, your character sheet
   records them, and **Sorcery is playable** — thirty-two workings in four books,
   and the realm choice decides which books you can open. Chaos and Trump are
   still names with no books behind them.

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

   **This game's Arcane is stronger than Zangband's.** Zangband's Arcane was
   deliberately the weak generalist — every one of its books could be bought in
   town, and it had no high-level power at all. Angband's arcane realm is the
   Mage's realm, with two books of attacks and a *Wizard's Tome of Power* found
   only in the dungeon. Folding the two together, which having exactly seven
   realms requires, keeps the stronger of the two. If you know Zangband, Arcane
   is not the realm you remember.

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

Nothing you could cast before has changed
-----------------------------------------

Adding the realms did not move a single spell. Every class Angband ships keeps
the same books **in the same order** with the same spells at the same levels.
Sorcery's four books went on the *end* of the lists that gained them — a Mage's
five Arcane books are still books one to five, with Sorcery's as six to nine.

That ordering is not tidiness. Your known spells are recorded by their position
in a flat list across all of your class's books, so appending leaves every
position meaning what it meant, while inserting anywhere else would shift them:
the game would load, the sheet would look reasonable, and a Priest who had
learned Remove Fear would find they knew something else.

That is not a courtesy, it is a hazard avoided. A character's known spells are
stored by their position in a flat list across all of their class's books, so
inserting a book anywhere but the end would silently shift every spell a saved
character knows one place along — the game would load, the sheet would look
reasonable, and a Priest who had learned Remove Fear would find they knew
something else instead.
