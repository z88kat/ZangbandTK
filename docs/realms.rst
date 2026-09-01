The Realms of Magic
===================

.. note::

   **Under construction.** You choose your realms at birth and your character
   sheet records them. What is not built yet is the spells that fill Sorcery,
   Chaos and Trump — until those arrive, choosing one of the three new realms
   gives you a realm with no books behind it.

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

Nothing you could cast before has changed
-----------------------------------------

Adding the realms did not move a single spell. Every class Angband ships keeps
the same books in the same order with the same spells at the same levels, and a
character saved before the realms arrived loads with exactly the spells they
knew. Only the *name* of the realm two of those book sets belong to has changed,
and no character ever sees a realm's internal name.

That is not a courtesy, it is a hazard avoided. A character's known spells are
stored by their position in a flat list across all of their class's books, so
inserting a book anywhere but the end would silently shift every spell a saved
character knows one place along — the game would load, the sheet would look
reasonable, and a Priest who had learned Remove Fear would find they knew
something else instead.
