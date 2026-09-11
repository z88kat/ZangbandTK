The Realms of Magic
===================

.. note::

   **All seven realms are playable** — Arcane, Life, Nature, Death, Sorcery,
   Chaos and Trump, two hundred and twenty-four workings in twenty-eight books,
   all of them Zangband's. You choose your realms at birth and your character
   sheet records them, and that choice decides which books you can open at all.

   Seventeen individual spells across the seven are inert and say so in their
   own description, each for want of a mechanism this game does not have. They
   are named in the realm sections below.

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
Life or Death and nothing else, and a Mage is offered all seven, then the six
it did not take.

A realm already taken is not offered again. A Mage of Arcane and Arcane would
study thirty-two spells where it should study sixty-four, which is not a choice
anybody would make on purpose and is therefore not one you are given.

**You begin holding the first book of every realm you study.** A Mage and a
Priest start with two, one for each slot; a Monk or a Chaos-Warrior with one;
a Warrior or a Mindcrafter with none, having no realm to hold a book for. The
book follows the realm you actually chose, so a Rogue who takes Death starts
with a Necromantic Tome and not somebody else's arcane primer. The second,
third and fourth books of a realm are bought or found — the village bookseller
exists for exactly that.

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
   * - Monk
     - Life, Nature or Death
     - —
   * - Chaos-Warrior
     - Chaos
     - —
   * - Warrior-Mage
     - Arcane
     - any of the seven
   * - High-Mage
     - any of the seven
     - —

That is twelve of the game's fourteen classes. The other two never reach this
step: a Warrior studies nothing, and a Mindcrafter's psionics are a power list
rather than a realm — nothing to choose, nothing to find, and nothing that can
be taken away.

Two of those rows are worth reading twice. **A Priest's second realm cannot be
Life or Death** — the first slot offers the two priestly realms and the second
offers the other five, so a Priest always ends up with one holy realm and one
that is not. And **a Ranger's first realm is Nature and there is no choice about
it**; you are not asked, because a list of one is not a question.

These entitlements are Zangband's own, taken from the same table Zangband keeps
them in, and the game checks itself against that table rather than against
anybody's idea of what a class ought to study.

Life
----

The healing realm, and the one Zangband's own spoiler calls "'good' magic". It
is built on cures, blessings and protection, and the handful of attacks in it
are there for a single purpose: harming and banishing foul things. No other
realm keeps a character alive as well, and no other realm is so uninterested in
anything that is not evil.

Four books: the **Book of Common Prayer** and **High Mass** in town, the **Book
of the Unicorn** and **Blessings of the Grail** below.

The cures run the whole length of the realm — *Cure Light Wounds*, *Cure Medium
Wounds*, *Cure Critical Wounds*, then *Healing* at three hundred points and
*Healing True* at two thousand — and *Holy Word* heals a thousand while it
dispels, which makes it the one prayer that answers both halves of a bad
situation at once. The blessings accumulate: *Bless*, then *Prayer* for four
times as long, *Heroism*, *Protection from Evil*, and *Restoration*, which puts
back every drained statistic and any lost experience together.

The offence is pointed at evil and almost nowhere else. *Holy Orb* is a ball of
holy force that does double damage to evil creatures, and after it comes a run
of dispels that need no target at all — *Exorcism*, *Dispel Undead & Demons*,
*Dispel Evil*, *Holy Word* — along with *Banish*, which teleports every evil
creature in sight away rather than killing it. At the end of the fourth book
are *Divine Intervention*, the longest chain of effects in the realm, and *Holy
Invulnerability*, which makes you very nearly untouchable for seven to fourteen
turns.

**Life is the Priest's own realm, and the figures say so.** A Priest reaches
*Holy Orb* at level 10, where a Paladin waits until 18, a Monk 19 and a Mage
20; and *Holy Invulnerability* at 45, where everybody else needs 50. It is also
the one realm a High-Mage is not first into — a Priest beats it to both of
those, by nine levels and by four. These are Zangband's own levels and mana
costs, class by class, unchanged.

*Holy Orb* goes further and pays the two priestly classes more damage for the
same prayer: a Priest or a High-Mage adds three halves of its level to the
3d6, and everybody else adds five quarters. Zangband tested the caster's class
in the spell itself, and two spells elsewhere do the same thing for the two
mage classes — Chaos's *Mana Burst* and Death's *Orb of Entropy*.

*Day of the Dove* turns every monster in line of sight to your side. It was
inert until pets arrived, for the same reason Nature's *Animal Taming* was: the
whole content of the spell is that the creatures become *yours*, and until
recently there was no side for them to be on (:doc:`pets`).

.. note::

   **Two of the thirty-two do nothing, and say so.** *Bless Weapon* sits in
   *Blessings of the Grail* with its level and its cost, described as beyond
   what the game can express. 4.2 has a blessed-weapon property and no effect
   that confers it; chaos branding is the nearest thing the engine has and is a
   different spell wearing this one's name. *Holy Vision* is the fourth spell
   in the game to want whole-object identification, and meets the same wall as
   Sorcery's *Identify True*, Trump's *Trump Lore* and Nature's *Stone Tell*:
   4.2 replaced it with runes learned one at a time, so there is no hidden
   description left for a prayer to reveal.

   **Three more do something measurably different.** Nothing here dispels
   demons by name — 4.2 dispels undead, evil, or everything — so *Exorcism* and
   *Dispel Undead & Demons* hit undead by name and demons by being evil, which
   catches every other evil creature in sight along with them: wider than
   Zangband's, not narrower. *Warding True* laid a ring of glyphs around the
   caster and now places the single glyph beneath you, the same as the lesser
   prayer does. And *Divine Intervention* does nine of the ten things
   Zangband's did; the tenth is the angel it sent to fight for you, and this
   game has no angel among its summon types.

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
three versions in the game, lifting a pound and a half a level and needing no
sight of what it takes, where the Chaos mutation lifts a pound a level and must
see what it takes. *Recharging* is four times the strength of Angband's own
spell of that name, which is a large part of why the realm is worth studying —
Arcane's *Recharging* is the same spell at the same strength, twenty-one levels
later. *Dimension Door* lets you pick the square you arrive on rather than
taking what the dungeon gives you. And *Globe of Invulnerability* makes you very
nearly untouchable for eight to sixteen turns.

*Charm Monster* turns a single creature to your side and it stays turned. It is
the single-target twin of Life's *Day of the Dove*, and like that prayer it was
a different and lesser spell until pets arrived — it took a monster over for a
while and handed it back, because there was no side for it to change to
(:doc:`pets`).

**A Rogue and a Ranger never learn Globe of Invulnerability.** Their Grimoire
of Power holds seven workings where everyone else's holds eight; Zangband's
table marks the last one unreachable for both. See :ref:`books-short`.

.. note::

   **Four of the thirty-two do nothing, and say so.** *Identify True*, *Detect
   Enchantment*, *Self Knowledge* and *Explosive Rune* each need a mechanism
   Angband 4.2 removed or never had — whole-object identification, a
   separate is-this-magical bit, a self-knowledge readout the character sheet
   already carries, and a glyph that explodes rather than wards. They are in
   their books with their levels and costs, described as beyond what the game
   can express, rather than quietly missing: a book with a hole in it and a note
   saying so is honest, and a spell absent without explanation is not.

Trump
-----

The last realm to arrive, and the one that had to wait for pets. Fifteen of its
thirty-two spells summon a creature, and in Zangband every one of them turns on
the same question: cast it well and the creature is *yours*, cast it badly and an
angry group of them appears. Until monsters could take sides, all fifteen would
have come out as the bad half — *Trump Cyberdemon* would have been sixty mana
spent to put a hostile Cyberdemon next to you, which is not a weaker spell but
the opposite spell. Pets closed that, and the realm went in whole.

Four books: **Conjurings & Tricks** and the **Deck of Many Things** in town,
**Trumps of Doom** and **Five Aces** below.

.. note::

   **A Trump summon that fails simply fails here.** Zangband makes the hostile
   result the price of the realm — miss the roll and the thing you called
   arrives angry. In this game only Chaos and Death punish a miscast, so a
   failed summons costs you the mana and nothing else, and every creature you
   do call is yours. That is a real softening of the realm rather than a detail
   of the translation, and it is written down here because the spoiler opens on
   the risk it removes.

What it is for is reach: a deck that goes places and fetches people. *Teleport*,
*Teleport Away*, *Teleport Level*, *Word of Recall* and *Phase Door* are all in
it, which makes it the realm of getting out; and the summons run from *Trump
Animal* up through the hounds, spiders, reptiles, dragons and undead to
*Trump Cyberdemon*. *Mass Trump* calls several at once. *Living Trump* makes
the caster part of the deck, and *Trump Branding* puts a brand on your weapon,
though not one you choose.

It fetches more than people. *Trump Reach* pulls a distant object to you — the
same spell as Sorcery's *Telekinesis*, at the same pound and a half a level.
*Trump Divination* detects everything nearby in one casting, and *Banish*
teleports away every creature in sight rather than killing it.

And it does have two attacks, which is easy to miss in a realm this busy:
*Mind Blast* early, a bolt of psychic force that sometimes runs as a beam, and
*Death Dealing* in the third book, which strikes every living creature in sight
at once.

**A Ranger loses more here than in any other realm**, and a Rogue is not far
behind. Seven of Trump's thirty-two are unreachable for a Ranger and four for a
Rogue, all of them in the last two books — a Ranger's Five Aces holds four
cards of eight. See :ref:`books-short`.

.. note::

   **Four of the thirty-two do nothing, and say so.** *Reset Recall* would
   write the depth Word of Recall takes you to, and nothing here can. *Shuffle*
   is the deck itself — one roll read off twenty unequal outcomes, several of
   which need mechanisms this game lacks. *Joker Card* summons the groups 4.2's
   summon table has no equivalent of. And *Trump Lore* is whole-object
   identification, the same wall as Sorcery's *Identify True* and Nature's
   *Stone Tell*.

   *Dimension Door* was the fifth until recently, and should not have been. It
   lets you choose the square you arrive on, which this game can do and always
   could — Sorcery's copy of the same spell had been doing it since that realm
   went in. Both now work.

The guard that used to keep Trump off the birth menu still covers every realm:
**if your class has no books in a realm, you are not offered it.** That was not
true before, and choosing Sorcery in the version before it shipped gave you a
character who could never cast anything.

Chaos
-----

Sorcery's opposite in every respect. Sorcery has no attack spell in it; Chaos is
almost nothing else. The spoiler calls it "the very element of unmaking", and
what it offers instead of protection is an arsenal that runs from *Magic
Missile* in the first book to *Mana Storm* and *Call the Void* in the last.
**Chaos has no protective spells at all** — that is the realm's own description
of itself, and it is the thing to weigh before taking it as your only one.

Four books: the **Sign of Chaos** and **Chaos Mastery** in town, **Chaos
Channels** and the **Armageddon Tome** below.

The damage is the point, and it is everywhere. Bolts of fire, chaos, mana and
gravity; *Fist of Force*, which stuns; *Sonic Boom* and *Invoke Logrus*
centred on you; *Chain Lightning*, which fires beams in every direction at
once; and at the bottom of the fourth book *Magic Rocket*, *Mana Storm* at
three hundred plus twice your level, *Breathe Logrus* — which does damage equal
to your *current* hit points, so cast it healthy — and *Call the Void*, three
volleys of destruction outward in every direction.

It is not purely artillery. *Wonder* is a hundred-band random table that may
clone a monster, heal it, or drop a fireball on it. *Polymorph Other* reshapes
a creature into something else. *Alter Reality* rebuilds the level around you,
*Teleport Other* clears a whole line of monsters from your path, and *Arcane
Binding* recharges a wand at four times the strength of Angband's own spell.

Nothing was deferred — all thirty-two work — but several do something
measurably different from Zangband's, because 4.2 has no disintegration, no
rocket and no radiation, and because a blast radius is a fixed number here
rather than one that grows as you do. The trades are in the project's decision
record as DEC-53. Three are worth knowing at the table: **Call the Void no
longer punishes you for casting it beside a wall**, which was the spell's
defining risk; **Fist of Force keeps its damage and loses its ability to dig**,
since the element that did both no longer exists; and **Chaos Branding gives
fire or frost rather than a chaos brand**, because 4.2 has no chaos brand to
give.

**And Chaos spells backfire.** Fail one and you may not simply lose the mana:
the realm produces a chaotic effect instead, and how bad it is depends on how
deep the spell was. *Magic Missile* never backfires. *Call the Void* almost
always does, and what you get ranges from a short teleport, through a room full
of doors and traps, a mutation, a forgotten map and an earthquake, down to eight
monsters landing on you and, at the very bottom, the Ancient and Foul Curse.
This is Zangband's own table and its own scaling — the spoiler warns that "Chaos
spells are known to backfire easily" — and it is the price of the realm having
no protective spells at all.

*Summon Demon* calls a demon of up to half again your level, and **one time in
three it serves you** — the other two it does not, and says so. That is
Zangband's own roll, restored when pets arrived; it shipped always-hostile for
one milestone before there was any side for a demon to be on (:doc:`pets`).

The **Chaos-Warrior** is the realm's own class: one realm, no choice, every
spell, from level 2. Mages, Priests and Rangers may take Chaos as one of two
realms; a Chaos-Warrior *is* one.

**A Ranger never reaches the realm's last four spells** — *Summon Demon*,
*Mana Storm*, *Breathe Logrus* and *Call the Void* are all unreachable for it,
which leaves its Armageddon Tome holding five of eight. See
:ref:`books-short`.

*Polymorph Self* is here too — the same effect the Chaos mutation uses, reached
from a second place. Chaos is the realm that twists its own caster, and that is
the spell that does it.

Arcane
------

The weak generalist, and the one realm where this game and Angband disagree
outright. Detection, escape, the four temporary resistances, cures for poison
and for light and medium wounds, *Stone to Mud*, *Satisfy Hunger*,
*Recharging* — everything a delver wants and almost nothing that kills. Its
whole offence is *Zap*, a weak lightning bolt; *Ray of Light*; and *Elemental
Ball*, which throws fire, lightning, frost or acid and does not let you choose
which.

Four books: **Cantrips for Beginners**, **Minor Arcana**, **Major Arcana** and
the **Manual of Mastery** — and **all four are sold in town**, for 100, 250,
1000 and 2500 gold. That is the realm's bargain, and it is the reason to take
it: no other realm can be bought outright. Every other realm keeps its two best
books in the dungeon.

**Take it second, not first.** Arcane collects the useful spells of every other
realm, and every other realm sells them cheaper. Nineteen of its thirty-two
appear in some realm a Mage might also be studying, and in seventeen of those
the other realm asks a lower level and less mana — usually much less. A Mage
reads *Identify* here at level 38 for 30 mana and in Sorcery at level 10 for 7;
*Recharging* at 28 against Sorcery's 7; *Clairvoyance* at 49 for a hundred mana
against Sorcery's 30 for forty; *Stone to Mud* at 20 against Nature's 5. Only
*Cure Poison* is genuinely better here. So Arcane is the realm that fills the
gaps in another one, and a poor choice as the only realm you have — which is
Zangband's own advice, and it survives the port intact.

Two of the thirty-two do nothing and say so. *Phlogiston* refuels a light
source, which is a command in this game rather than a spell; and *Detect
Enchantment* wants a single is-this-magical bit, which no longer exists now that
an object's properties are runes you learn one at a time.

One more does something else entirely. **Zangband's Wizard Lock jammed a door shut and this one makes a
door**, because 4.2 has no jamming at all — a door is
open, closed, locked or broken, and no spell reaches the lock. Making one along
the line you aim serves the same purpose, and works in an empty corridor where
the original needed a door to already be there.

**A Ranger never learns Clairvoyance.** Its Manual of Mastery holds seven
spells where everyone else's holds eight. That is Zangband's own table, which
marks the spell unreachable for that class rather than merely expensive, and it
is not the only place it does so — see below.

Nature
------

The weather at the top and the ground at the bottom, with healing, protection
and a pair of bolts in between. **It holds the only powerful healing spell
outside Life**: *Herbal Healing* mends a thousand points and cures stunning,
bleeding and poison with it, and no realm but Life has anything of the size.
That one spell is a large part of why a character who cannot study Life studies
this instead.

Four books: **Call of the Wild**, **Nature Mastery**, **Nature's Gifts** and
**Nature's Wrath** — the last of which shares its name with the spell inside it.

The protections come in two grades. *Resist Environment* covers fire, cold and
lightning; *Resistance True* covers those three plus acid **and poison**, which
is the half worth paying for — each of the four elements has a cheap spell of
its own somewhere in the game and poison does not. *Stone Skin* hardens you for
thirty to fifty turns, and *Stone Skin*, *Wall of Stone*, *Stair Building* and
*Door Building* between them let you reshape the dungeon rather than fight it.

The offence is weather. *Lightning Bolt* and *Frost Bolt* early, *Ray of
Sunlight* for what cannot bear light, and then the fourth book: *Earthquake*,
*Whirlwind Attack* — one blow into every square around you — and the three
balls, a *Blizzard* at seventy plus your level, a *Lightning Storm* at ninety
and a *Whirlpool* at a hundred, water being the one that batters and confuses
as well. *Call Sunlight* floods the whole level with light and burns everything
nearby that hates it. *Elemental Branding* puts fire or frost on your weapon,
and does not let you pick which.

At the end, *Nature's Wrath* does three things in one casting: dispels every
creature in sight, shakes the level apart with an earthquake, and detonates
around you. Zangband's third part was a disintegration ball, which this game has
no projection for; it arrives as the walls coming down and the damage landing
separately, which is the same event described twice rather than one effect.

**Two of the thirty-two do nothing.** *Stone Tell* would tell you everything
about an object in a game that replaced that with runes learned one at a time,
and *Protect from Corrosion* is an object property here rather than anything a
spell can reach.

Three more used to be inert and no longer are. *Animal Taming*, *Summon Animal*
and *Animal Friendship* all needed a creature to be able to take your side; pets
gave them that, and Nature is now the realm that walks into a fight with company
(:doc:`pets`).

The **Druid** is the first class in the game to cast on borrowed figures.
Zangband never had a Druid, so there is no row of levels and mana costs for it
to import; it takes Zangband's *Priest's*, which is the only class carrying
Nature that shares the Druid's three magic constants. The **Ranger**, which used
to borrow two of the Druid's books, now gets the whole realm on its own figures.

Death
-----

The foulest of them, and the only realm that punishes you for casting it badly.
Drains, nether, darkness, poison, terror, two kinds of genocide, and at the
bottom *Hellfire* — six hundred and sixty-six points that nothing resists, and
fifty to a hundred of your own to pay for it.

Four books: **Black Prayers**, **Black Mass**, **Black Channels** and the
**Necronomicon**.

.. warning::

   **A miscast Death spell hurts.** Fail one and, on a roll against the spell's
   depth in the realm, you take ``(book + 2)d6`` — so the deeper the book the
   worse it is. From the second half of the realm you may lose experience with
   it, one time in six, unless you have Hold Life.

   **The Necronomicon is worse.** Half of its miscasts shake the reader instead:
   fail a saving throw and you are confused, and one time in three
   hallucinating badly; make that one and fail the next and you lose a point of
   intelligence *and* a point of wisdom. It is the only spell failure in the
   game that can permanently cost a character a statistic, and it is why the
   fourth book is frightening rather than merely expensive.

Three of the thirty-two do nothing. *Raise the Dead* animates the corpses and
skeletons actually lying on the floor near you, and 4.2 has no such object —
there is nothing lying there to raise. Translating it as "summon undead as pets"
would be a different spell wearing this one's name: this one is paid for by what
you have already killed, and that is the whole character of it. *Wraithform* needs an incorporeal player, which
this game has no notion of. And *Omnicide* kills every creature on the level, one
at a time, taking mana until you run out — a sweep with a running cost that the
realm's own *Mass Genocide* would otherwise duplicate.

*Enslave Undead* was on that list until pets arrived. It now does what it says,
and binds one undead creature to you (:doc:`pets`).

Both of Death's classes cast on borrowed figures: Zangband had neither a
Necromancer nor a Blackguard. The Necromancer takes Zangband's Mage and the
Blackguard its Paladin.

Who can choose what
-------------------

**Every class can study every realm Zangband lets it.** That sounds obvious and
was not true until recently: the entitlements were right from the start, but the
*books* for a realm were only ever given to classes that already carried that
kind of book, so a Mage was allowed six realms and had three. Completing it added
forty books and three hundred and five spells (DEC-57).

What it means at the table: **a Mage can study Death or Life**, and a **Priest
can study Arcane**. That is Zangband's Mage — a far broader class than Angband's,
with healing at one end and *Hellfire* at the other — and if you know Angband
4.2, it is the biggest single change in this chapter.

One oddity worth knowing, because it looks like a bug and is not. Whether you
*choose* which spell to learn or get a random one is a property of your **class**,
never of the book. So a Mage that studies Life picks its prayers, and a Priest
reading the same book takes what it is given. Zangband does exactly this.

The two classes carried over from the previous milestone are the ones the realm
system exists for:

- A **Warrior-Mage** studies **Arcane** in its first slot, always, and anything
  it likes in the second. Zangband lists Arcane in both slots, so it may take it
  twice and study one realm — the table's own answer, left as it stands.
- A **High-Mage** chooses **one** realm out of seven and gets no second slot,
  and is paid for it in figures: it reaches a realm's last spells earlier and
  more cheaply than any other class, in six realms out of seven. Life is the
  exception, because Life is the Priest's own realm and Zangband's table says
  so.

Both are entitled to Trump and both are now offered it; it was the one
entitlement in the table that had nothing behind it, and no longer is.

.. _books-short:

Books that come up short
~~~~~~~~~~~~~~~~~~~~~~~~

**A Rogue and a Ranger do not get all of a borrowed realm.** Every realm is four
books of eight, and for most classes that is thirty-two spells — but Zangband's
table marks some spells unreachable for those two rather than merely expensive,
and a book then holds fewer than eight. Sixteen books in the game are short, and
all sixteen belong to one of those two classes:

.. list-table::
   :header-rows: 1
   :widths: 16 34 16 34

   * - Class
     - Book
     - Spells
     - Realm
   * - Rogue
     - Necronomicon
     - 4 of 8
     - Death
   * - Ranger
     - Five Aces
     - 4 of 8
     - Trump
   * - Ranger
     - Armageddon Tome
     - 5 of 8
     - Chaos
   * - Ranger
     - Necronomicon
     - 5 of 8
     - Death
   * - Rogue
     - Five Aces
     - 5 of 8
     - Trump
   * - Rogue, Ranger
     - Black Mass
     - 6 of 8
     - Death
   * - Ranger
     - Black Channels
     - 6 of 8
     - Death
   * - Ranger
     - Trumps of Doom
     - 6 of 8
     - Trump
   * - Rogue, Ranger
     - Grimoire of Power
     - 7 of 8
     - Sorcery
   * - Rogue
     - Black Channels
     - 7 of 8
     - Death
   * - Rogue
     - Trumps of Doom
     - 7 of 8
     - Trump
   * - Ranger
     - Manual of Mastery
     - 7 of 8
     - Arcane
   * - Ranger
     - Deck of Many Things
     - 7 of 8
     - Trump
   * - Ranger
     - Chaos Channels
     - 7 of 8
     - Chaos

A Rogue's Necronomicon is the extreme, and worth spelling out. It holds four
rituals of the eight — *Death Ray*, *Raise the Dead*, *Esoteria* and
*Wraithform* — so *Hellfire*, *Omnicide*, *Word of Death* and *Evocation* are
simply not available to a Rogue at any level. Two of the four it does get are
among the realm's inert spells, which leaves **two working rituals in a fifty
thousand gold book**. The Ranger's Five Aces is close behind at four cards, one
of them inert.

The fourth book of a realm is where its power lives, so this is the price those
two classes pay for being entitled to realms they were not built for. It is
worth knowing before you buy the book rather than after.

What has changed, and what has not
----------------------------------

**All four of the realms Angband already had now hold Zangband's spells**:
Life, Arcane, Nature and Death. Every realm in the game is four books of eight,
which is Zangband's shape throughout. This is DEC-50, taken deliberately, and it has consequences worth stating:

- **A Priest's prayers and a Mage's spells are different spells now**, at
  different levels, in differently-named books. A Paladin gains most — Angband
  gave it three of the Priest's books and sixteen prayers, and Zangband's table
  gives it all thirty-two. A Ranger gains nearly as much.
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
