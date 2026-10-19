The Realms of Magic
===================

.. note::

   All seven realms are playable: Arcane, Life, Nature, Death, Sorcery, Chaos
   and Trump. That's two hundred and twenty-four workings in twenty-eight
   books, all of them Zangband's. You choose your realms at birth, your
   character sheet records them, and that choice decides which books you can
   open at all.

   Sixteen individual spells across the seven are inert, and their own
   descriptions say so. Each one needs a mechanism this game does not have.
   They're named in the realm sections below.

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
nouns are still Angband's. Zangband kept the same information in scattered
per-class constants, and Angband's way of holding it is better.

.. warning::

   Arcane is Zangband's Arcane, which is a change from Angband. The two games
   disagreed about this realm more than any other. Zangband meant its Arcane
   to be the weak generalist, with every book buyable in town and no
   high-level power at all. Angband's arcane realm is the Mage's realm, with
   two books of attacks and a *Wizard's Tome of Power* found only in the
   dungeon. With exactly seven realms the two had to be folded together, and
   the fold went Zangband's way (DEC-49, DEC-50).

   So, unlike in Angband, a Mage doesn't learn its attacks from Arcane. There's
   one ball, one bolt and one beam in its thirty-two spells. In Zangband a Mage
   takes *two* realms, and the damage comes from the second. Take Chaos
   alongside Arcane and you have the classic Zangband Mage.

Choosing them
-------------

Realm choice is a birth step of its own, after race and class, because your
class decides which realms you may study. A Paladin is offered Life or Death
and nothing else. A Mage is offered all seven, then the six it didn't take.

A realm you've already taken is not offered again. A Mage of Arcane and Arcane
would get thirty-two spells where it should have sixty-four, and nobody would
choose that, so you aren't given the choice.

You start with the first book of every realm you study. A Mage or a Priest
starts with two, one for each slot; a Monk or a Chaos-Warrior with one; a
Warrior or a Mindcrafter with none, since they have no realm to hold a book
for. The book matches the realm you chose, so a Rogue who takes Death starts
with a Necromantic Tome and not somebody else's arcane primer. The second,
third and fourth books of a realm have to be bought or found, which is what
the village bookseller is for.

.. list-table::
   :header-rows: 1
   :widths: 22 34 44

   * - Class
     - First realm
     - Second realm
   * - Mage
     - any of the seven
     - any of the other six
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
     - any of the other six
   * - High-Mage
     - any of the seven
     - —

That's twelve of the game's fourteen classes. The other two never reach this
step. A Warrior studies nothing, and a Mindcrafter's psionics aren't a realm:
they're a list of powers, with nothing to choose, nothing to find and nothing
that can be taken away.

A Priest's second realm can't be Life or Death. The first slot offers the two
priestly realms and the second offers the other five, so a Priest always ends
up with one holy realm and one that isn't. A Ranger's first realm is always
Nature, and you aren't asked, because a list of one isn't a question.

No class studies the same realm twice. Whatever the entitlement table lists, a
realm taken in the first slot is struck off the second slot's menu, so two
slots always mean two realms. This affects the Mage and the Warrior-Mage, which
are both entitled to realms that overlap between slots: their second question
has one fewer row than the first.

These entitlements are Zangband's own, taken from the table Zangband keeps
them in, and the game checks itself against that table.

Life
----

Life is the healing realm, and the one Zangband's own spoiler calls "'good'
magic". It is built on cures, blessings and protection, and its handful of
attacks are there for one purpose: harming and banishing foul things. No other
realm keeps a character alive as well, and no other realm cares so little
about anything that isn't evil.

Four books: the **Book of Common Prayer** and **High Mass** in town, the **Book
of the Unicorn** and **Blessings of the Grail** below.

The cures run the whole length of the realm: *Cure Light Wounds*, *Cure Medium
Wounds*, *Cure Critical Wounds*, then *Healing* at three hundred points and
*Healing True* at two thousand. *Holy Word* heals a thousand while it
dispels, so it's the one prayer that deals with both halves of a bad situation
at once. The blessings pile up: *Bless*, then *Prayer* for four times as long,
*Heroism*, *Protection from Evil*, and *Restoration*, which puts back every
drained statistic and any lost experience together.

The attacks are aimed at evil and almost nothing else. *Holy Orb* is a ball of
holy force that does double damage to evil creatures. After it comes a run of
dispels that need no target at all (*Exorcism*, *Dispel Undead & Demons*,
*Dispel Evil*, *Holy Word*), and *Banish*, which teleports every evil creature
in sight away without killing it. At the end of the fourth book are *Divine
Intervention*, the longest chain of effects in the realm, and *Holy
Invulnerability*, which makes you very nearly untouchable for seven to fourteen
turns.

Life is the Priest's own realm, and the numbers show it. A Priest reaches
*Holy Orb* at level 10, where a Paladin waits until 18, a Monk 19 and a Mage
20, and *Holy Invulnerability* at 45, where everybody else needs 50. It is also
the one realm a High-Mage is not first into: a Priest beats it to both of those
spells, by nine levels and by four. These are Zangband's own levels and mana
costs, class by class, unchanged.

*Holy Orb* also does more damage for the two priestly classes: a Priest or a
High-Mage adds three halves of its level to the 3d6, and everybody else adds
five quarters. Zangband tested the caster's class inside the spell itself, and
two spells elsewhere do the same for the two mage classes: Chaos's *Mana
Burst* and Death's *Orb of Entropy*.

*Day of the Dove* turns every monster in line of sight to your side. It was
inert until pets arrived, for the same reason Nature's *Animal Taming* was: the
whole spell is about the creatures becoming *yours*, and before pets there was
no side for them to be on (:doc:`pets`).

.. note::

   Two of the thirty-two used to do nothing, and now work. *Bless Weapon*
   blesses a melee weapon, giving it the same blessed property a Priest
   already fights better with, and drives out any curse on it that isn't
   permanent. A permanent curse drives the blessing off instead, and an
   artifact takes the blessing only one time in three, losing a point of
   enchantment when it refuses. *Holy Vision* tells you everything about an
   object, which in this game means you learn every rune on it at once, as
   with Sorcery's *Identify True*, Trump's *Trump Lore* and Nature's *Stone
   Tell*. Rune knowledge is yours, so a rune the prayer teaches you is known
   on everything else that has it too.

   Three more work differently from Zangband's. 4.2 can dispel undead, evil,
   or everything, but nothing here dispels demons by name, so *Exorcism* and
   *Dispel Undead & Demons* hit undead by name and demons for being evil. That
   catches every other evil creature in sight along with them, which makes
   them wider than Zangband's. Zangband's *Warding True* laid a ring of glyphs
   around the caster; here it places a single glyph beneath you, the same as
   the lesser prayer does. And *Divine Intervention* does nine of the ten
   things Zangband's did. The tenth was the angel it sent to fight for you,
   and this game has no angel among its summon types.

Sorcery
-------

Sorcery is the meta-realm, and the first of the three new realms to arrive. It
has detection, identification, protection, escape and every convenience a
dungeon delver wants, and not one direct attack spell. It makes everything else
you do easier and kills nothing itself.

Four books: the **Beginner's Handbook**, the **Master Sorcerer's Handbook**,
**Pattern Sorcery** (a name this game has a better claim on than Zangband did),
and the **Grimoire of Power**. The first two are sold in town; the last two are
found below.

*Alchemy* turns an object into gold. Read its terms before you use it, because
there's no undoing it. You're paid a third of the object's full value, not a
third of what a shop would offer, up to thirty thousand gold. Artifacts refuse.
A worthless object is destroyed and pays nothing. And the division happens
*before* the quantity multiplies, so a stack of ten two-gold items pays nothing
at all: ten times a third of two, rounded down each time, is zero. The Midas
touch mutation works the same way and on the same terms.

Its best workings are the ones no other realm has. *Teleport Away* fires a beam
that moves monsters out of your path without killing them. *Telekinesis* pulls
an object to your feet from anywhere you can reach. It is the strongest of the
three versions in the game: it lifts a pound and a half a level and doesn't
need to see what it takes, where the Chaos mutation lifts a pound a level and
must see it. *Recharging* is four times the strength of Angband's own spell of
that name, and that's a large part of why the realm is worth studying.
Arcane's *Recharging* is the same spell at the same strength, twenty-one levels
later. *Dimension Door* lets you pick the square you arrive on. And *Globe of
Invulnerability* makes you very nearly untouchable for eight to sixteen turns.

*Charm Monster* turns a single creature to your side for good. It's the
single-target twin of Life's *Day of the Dove*, and like that prayer it was a
different, lesser spell until pets arrived: it took a monster over for a while
and handed it back, because there was no side for it to change to
(:doc:`pets`).

A Rogue and a Ranger never learn *Globe of Invulnerability*. Their Grimoire of
Power holds seven workings where everyone else's holds eight, because
Zangband's table marks the last one unreachable for both. See
:ref:`books-short`.

.. note::

   One of the thirty-two does nothing, and its description says so. *Detect
   Enchantment* would sense which objects are magical before you know them.
   Angband 4.2 dropped that when an object's properties became runes, and it
   will not be brought back. The spell is still in its book with its level and
   cost, described as beyond what the game can express, so the gap in the
   book is explained.

   *Explosive Rune* is a ward with a sting. It draws a rune under you that
   monsters can't cross until they break it, like a glyph of warding, but it
   breaks far more easily: a deep monster gets through about every other try.
   If one breaks it while you're standing on it, it bursts in a ball of mana
   around you, which is where the monster that broke it is standing. Broken
   anywhere else, it's just gone. Your own creatures leave your runes alone.

   *Identify True* and *Self Knowledge* used to be inert too. *Identify True*
   teaches you every rune on an object at once, where the first book's
   *Identify* teaches one. *Self Knowledge* teaches you every rune on the
   equipment you're wearing, which is the part of the old spell the character
   sheet couldn't already do.

Trump
-----

Trump was the last realm to arrive. It had to wait for pets. Fifteen of its
thirty-two spells summon a creature, and in Zangband every one of them turns on
the same question: cast it well and the creature is *yours*, cast it badly and
an angry group of them appears. Until monsters could take sides, all fifteen
would have come out as the bad half. *Trump Cyberdemon* would have been sixty
mana spent on putting a hostile Cyberdemon next to you. Pets fixed that, and
the realm went in whole.

Four books: **Conjurings & Tricks** and the **Deck of Many Things** in town,
**Trumps of Doom** and **Five Aces** below.

.. note::

   A Trump summon that fails just fails here. In Zangband the hostile result
   is the price of the realm: miss the roll and the thing you called arrives
   angry. In this game only Chaos and Death punish a miscast, so a failed
   summons costs you the mana and nothing else, and every creature you do call
   is yours. That makes the realm a good deal softer than Zangband's, and
   Zangband's spoiler opens on the very risk this removes.

.. note::

   Three of the cards don't fetch what they're named after. *Trump
   Cyberdemon* summons a greater demon: this game has no cyberdemons, by
   choice. *Phantasmal Servant* summons an ordinary undead creature, and
   *Conjure Elemental* summons a Maia, not an earth elemental. The other eight
   cards do what they say.

Trump is for reach: a deck that goes places and fetches people. *Teleport*,
*Teleport Away*, *Teleport Level*, *Word of Recall* and *Phase Door* are all in
it, so it's the realm for getting out. The summons run from *Trump Animal* up
through the hounds, spiders, reptiles, dragons and undead to *Trump
Cyberdemon*, and *Mass Trump* calls several at once. *Living Trump* makes the
caster part of the deck, and *Trump Branding* puts a brand on your weapon,
though not one you choose.

It fetches things as well as people. *Trump Reach* pulls a distant object to
you. It's the same spell as Sorcery's *Telekinesis*, at the same pound and a
half a level. *Trump Divination* detects everything nearby in one casting, and
*Banish* teleports away every creature in sight without killing it.

It also has two attacks, which are easy to miss in a realm this busy: *Mind
Blast* early on, a bolt of psychic force that sometimes runs as a beam, and
*Death Dealing* in the third book, which strikes every living creature in sight
at once.

A Ranger loses more here than in any other realm, and a Rogue isn't far
behind. Seven of Trump's thirty-two are out of reach for a Ranger and four for
a Rogue, all of them in the last two books. A Ranger's Five Aces holds four
cards of eight. See :ref:`books-short`.

.. note::

   Every one of the thirty-two does something. The last to arrive was
   *Shuffle*, the deck itself: one draw against twenty-one cards, and the bad
   ones are the common ones. Death calls a crowd of monsters against you, the
   Devil a demon, and the Hanged Man lays the Ancient Curse; the Fool drains
   your intelligence and wisdom, and the Wheel of Fortune is the wild magic of
   a failed Chaos spell. Higher up are teleports, a blessing, the Tower that
   breaks the walls around you or shakes the ground, Temperance that puts
   what's next to you to sleep, a creature drawn as a friend, the Lovers'
   charm and the Hermit's wall of granite behind you. At the top are
   Judgement, which rerolls your hit points and cures every mutation you
   have; the Sun, which lights and maps the level; and the World, which is
   experience. A Rogue or a High-Mage, card sharps both, draws higher the more
   experienced they are.

   *Joker Card* and *Trump Lore* used to do nothing as well. The Joker calls a
   mold, a bat, a vortex or creeping coins to serve you, and the card decides
   which. Like every Trump summon it calls at the depth you're on, so a
   vortex, the shallowest of which lives at 21, won't answer above that.
   *Trump Lore* teaches you every rune on an object at once, as Sorcery's
   *Identify True* does.

   *Dimension Door* was inert too, and shouldn't have been. It lets you choose
   the square you arrive on, which this game has always been able to do:
   Sorcery's copy of the same spell did it from the day that realm went in.
   Both work now.

   So was *Reset Recall*. Each dungeon remembers the level Word of Recall
   brings you back to, and the spell sets it: cast in a dungeon, it asks which
   level, anywhere from the dungeon's first to the deepest you've reached in
   it, and offers the deepest. Above ground there's no dungeon to set, and it
   isn't cast.

If your class has no books in a realm, you are not offered it. That guard used
to keep Trump off the birth menu, and it still covers every realm. Without it,
choosing Sorcery in the version before Sorcery shipped gave you a character
who could never cast anything.

Chaos
-----

Chaos is Sorcery's opposite in every way. Sorcery has no attack spell in it,
and Chaos is almost nothing else. The spoiler calls it "the very element of
unmaking". In place of protection it has an arsenal that runs from *Magic
Missile* in the first book to *Mana Storm* and *Call the Void* in the last.
Chaos has no protective spells at all. That's the realm's own description of
itself, and it's the thing to weigh before taking Chaos as your only realm.

Four books: the **Sign of Chaos** and **Chaos Mastery** in town, **Chaos
Channels** and the **Armageddon Tome** below.

The damage is everywhere. There are bolts of fire, chaos, mana and gravity;
*Fist of Force*, which stuns; *Sonic Boom* and *Invoke Logrus*, centred on
you; *Chain Lightning*, which fires beams in every direction at once. At the
bottom of the fourth book are *Magic Rocket*, *Mana Storm* at three hundred
plus twice your level, *Breathe Logrus*, which does damage equal to your
*current* hit points (so cast it while you're healthy), and *Call the Void*,
three volleys of destruction outward in every direction.

It isn't all artillery. *Wonder* is a hundred-band random table that may clone
a monster, heal it, or drop a fireball on it. *Polymorph Other* reshapes a
creature into something else. *Alter Reality* rebuilds the level around you,
*Teleport Other* clears a whole line of monsters from your path, and *Arcane
Binding* recharges a wand at four times the strength of Angband's own spell.

All thirty-two work, but several behave differently from Zangband's. 4.2 has no
disintegration, no rocket and no radiation, and a blast radius here is a fixed
number, where Zangband's grew as you did. The trades are written up in the
project's decision record as DEC-53. You'll notice three of them in play.
*Call the Void* doesn't punish you for casting it beside a wall, though in
Zangband that was the spell's defining risk. *Fist of Force* keeps its damage
and loses its ability to dig, since the element that did both doesn't exist
here. And *Chaos Branding* gives fire or frost instead of a chaos brand,
because 4.2 has no chaos brand to give.

Two more make a difference if you're counting on them. *Call Chaos* doesn't
roll its damage type. Zangband picked one of thirty elements and then one of
three shapes. Here the three shapes are kept and each has one fixed element, so
the roll still decides what arrives but not what it's made of. You get a ball
of chaos, a scatter of plasma or a wide sphere of mana, and those are the only
three things it can ever be. *Magic Rocket* is shards. In Zangband it was a
rocket, which almost nothing resisted; shards is the element 4.2 has that cuts,
so shard resistance blunts it, on either side of the fight. That's a bigger
deal than it sounds, because a good deal of what lives below two thousand feet
resists shards, and nothing resisted a rocket.

You'll also feel the fixed radius. Zangband grew five of these blasts as the
caster grew. Here each is fixed at the size it has the day you learn it, and
never gets any wider however high you go: *Flash of Light* 1, *Mana Burst* 2,
*Disintegrate* 3, *Sonic Boom* 4, *Invoke Logrus* 6. The damage still climbs,
but the area doesn't, and a level-45 *Sonic Boom* covers exactly what a
level-25 one did.

Chaos spells also backfire. Fail one and you may lose more than the mana: the
realm produces a chaotic effect, and how bad it is depends on how deep the
spell was. *Magic Missile* never backfires. *Call the Void* almost always does.
What you get ranges from a short teleport, through a room full of doors and
traps, a mutation, a forgotten map and an earthquake, down to eight creatures
of one strange kind landing on you (molds, bats, vortices, quylthulgs, or
things that aren't what they seem) and, at the very bottom, the Ancient and
Foul Curse. This is Zangband's own table and its own scaling, and the spoiler
warns that "Chaos spells are known to backfire easily". It is the price of the
realm having no protective spells at all.

*Summon Demon* calls a demon of up to half again your level. One time in three
it serves you; the other two it doesn't, and you're told so. That is Zangband's
own roll, back since pets arrived. In the milestone before pets it was always
hostile, because there was no side for a demon to be on (:doc:`pets`).

The Chaos-Warrior is the realm's own class: one realm, no choice, every spell,
from level 2. Mages, Priests and Rangers may take Chaos as one of two realms; a
Chaos-Warrior *is* one.

A Ranger never reaches the realm's last four spells. *Summon Demon*, *Mana
Storm*, *Breathe Logrus* and *Call the Void* are all out of its reach, which
leaves its Armageddon Tome holding five of eight. See :ref:`books-short`.

*Polymorph Self* is here too, the same effect the Chaos mutation uses, reached
another way. Chaos is the realm that twists its own caster, and this is the
spell that does it.

Arcane
------

Arcane is the weak generalist, and the one realm where this game and Angband
disagree outright. It has detection, escape, the four temporary resistances,
cures for poison and for light and medium wounds, *Stone to Mud*, *Satisfy
Hunger* and *Recharging*: everything a delver wants, and almost nothing that
kills. Its whole offence is *Zap*, a weak lightning bolt; *Ray of Light*; and
*Elemental Ball*, which throws fire, lightning, frost or acid and doesn't let
you choose which.

Four books: **Cantrips for Beginners**, **Minor Arcana**, **Major Arcana** and
the **Manual of Mastery**. All four are sold in town, for 100, 250, 1000 and
2500 gold. That's the realm's bargain and the reason to take it: no other realm
can be bought outright, and every other realm keeps its two best books in the
dungeon.

Arcane is best taken as a second realm. It collects the useful spells of every
other realm, and every other realm sells them cheaper. Nineteen of its
thirty-two appear in some realm a Mage might also be studying, and in seventeen
of those the other realm asks a lower level and less mana, usually much less.
A Mage reads *Identify* here at level 38 for 30 mana and in Sorcery at level 10
for 7; *Recharging* at 28 against Sorcery's 7; *Clairvoyance* at 49 for a
hundred mana against Sorcery's 30 for forty; *Stone to Mud* at 20 against
Nature's 5. Only *Cure Poison* is better here. So Arcane fills the gaps in
another realm, and it's a poor choice as the only realm you have. That is
Zangband's own advice, and it still holds here.

One of the thirty-two does nothing, and its description says so. *Detect
Enchantment* would sense which objects are magical before you know them. That
went when an object's properties became runes you learn one at a time, and it
won't be brought back. *Phlogiston* used to be the other inert one. It adds
half a full load of fuel to the torch or lantern you're holding.

One more does something else entirely. Zangband's *Wizard Lock* jammed a door
shut, and this one makes a door, because 4.2 has no jamming at all: a door is
open, closed, locked or broken, and no spell reaches the lock. Making a door
along the line you aim does the same job, and it works in an empty corridor,
where the original needed a door to be there already.

A Ranger never learns *Clairvoyance*. Its Manual of Mastery holds seven spells
where everyone else's holds eight. That is Zangband's own table, which marks the
spell unreachable for that class, and it isn't the only place the table does
that (see below).

Nature
------

Nature has the weather at the top and the ground at the bottom, with healing,
protection and a pair of bolts in between. It holds the only powerful healing
spell outside Life. *Herbal Healing* mends a thousand points and cures
stunning, bleeding and poison as well, and no realm but Life has anything that
size. That one spell is a large part of why a character who can't study Life
studies Nature instead.

Four books: **Call of the Wild**, **Nature Mastery**, **Nature's Gifts** and
**Nature's Wrath**, which shares its name with the spell inside it.

The protections come in two grades. *Resist Environment* covers fire, cold and
lightning. *Resistance True* covers those three plus acid and poison, and the
poison is the part worth paying for: each of the four elements has a cheap
spell of its own somewhere in the game, and poison doesn't. *Stone Skin*
hardens you for thirty to fifty turns, and *Stone Skin*, *Wall of Stone*,
*Stair Building* and *Door Building* between them let you reshape the dungeon.

The attacks are weather. *Lightning Bolt* and *Frost Bolt* come early, with
*Ray of Sunlight* for things that can't bear light. Then the fourth book has
*Earthquake*, *Whirlwind Attack* (one blow into every square around you) and
three balls: a *Blizzard* at seventy plus your level, a *Lightning Storm* at
ninety and a *Whirlpool* at a hundred. The water one batters and confuses as
well. *Call Sunlight* floods the whole level with light and burns everything
nearby that hates it. *Elemental Branding* puts fire or frost on your weapon,
and doesn't let you pick which.

At the end, *Nature's Wrath* does three things in one casting: it dispels every
creature in sight, shakes the level apart with an earthquake, and detonates
around you. Zangband's third part was a disintegration ball, which this game
has no projection for, so here one event arrives as two effects: the walls come
down, and the damage lands separately.

Every one of the thirty-two does something. The last two to arrive were *Stone
Tell*, which teaches you every rune on an object at once, and *Protect from
Corrosion*, which makes a piece of armour ignore acid for good and undoes what
acid has already done to it, unless it's cursed.

Three others used to be inert. *Animal Taming*, *Summon Animal* and *Animal
Friendship* all needed a creature to be able to take your side. Pets gave them
that, and made Nature the realm that walks into a fight with company
(:doc:`pets`).

The Druid is the first class in the game to cast on borrowed figures. Zangband
never had a Druid, so there's no row of levels and mana costs to import for
it. It takes Zangband's *Priest's*, the only class with Nature that shares the
Druid's three magic constants. The Ranger used to borrow two of the Druid's
books, and now gets the whole realm on its own figures.

Death
-----

Death is the foulest of the realms, and it charges you in blood. Chaos punishes
a miscast too, but in Death even the spells that *work* cost you something:
*Hellfire* takes fifty to a hundred hit points to cast, *Genocide* takes 1d4
for every creature it removes and *Mass Genocide* 1d3, and a failed cast
anywhere in the realm draws blood of its own. In Zangband's words, the most
powerful spells "need their own blood as the focus".

Four books: **Black Prayers**, **Black Mass**, **Black Channels** and the
**Necronomicon**.

Death takes life and gives it to you. *Vampiric Drain* and *Vampirism True*
pull hit points out of a creature and into you; *Death Ray* draws the life out
of anything living and does nothing at all to what's already dead; *Word of
Death* does it to every living thing in sight at once. Around those are nether
and darkness bolts, *Stinking Cloud*, *Terror*, *Horrify*, *Black Sleep*, and
two brands for your weapon. *Berserk* and *Battle Frenzy* turn you into a
fighter (the second hastes you as well), and *Restore Life* puts back drained
experience. *Invoke Spirits* is the realm's dice roll: it reads from the same
hundred-band table as Chaos's *Wonder*, and may as easily summon undead onto
you as help.

At the bottom of the Necronomicon are *Evocation*, which blasts, terrifies and
banishes everything in sight in one casting, and *Hellfire*: six hundred and
sixty-six points that nothing resists and evil creatures take twice.

.. warning::

   A miscast Death spell hurts. Fail one and, on a roll against the spell's
   depth in the realm, you take ``(book + 1)d6``, so the deeper the book, the
   worse it is. In the second half of the realm you may lose experience with
   it as well, one time in six, unless you have Hold Life.

   The Necronomicon is worse. Half of its miscasts shake the reader instead:
   fail a saving throw and you're confused, and one time in three
   hallucinating badly; make that one and fail the next and you lose a point of
   intelligence *and* a point of wisdom. It is the only spell failure in the
   game that can permanently cost a character a statistic, and it's why the
   fourth book is frightening as well as expensive.

One of the thirty-two does nothing. *Raise the Dead* animates the corpses and
skeletons lying on the floor near you, and 4.2 has no such object, so there's
nothing there to raise. Turning it into "summon undead as pets" would make it
a different spell wearing this one's name. This one is paid for by what you've
already killed, which is the whole character of it, so it stays as it is.
Corpses are not something this game is going to have.

*Wraithform* and *Omnicide* used to be inert too. *Wraithform* is the wraith
form of the Wraith mutation: you walk through walls, a tenth of every blow
lands, and light ends it. If it ends while you're inside rock, you die there
(see :doc:`mutations`). *Omnicide* removes every creature on the level except
the uniques, your own creatures included. Each costs you a little health and
gives you a point of mana, up to twice your maximum.

*Enslave Undead* was inert until pets arrived. It does what it says, and binds
one undead creature to you (:doc:`pets`).

Three more reach wider than Zangband's did, all for the same reason: 4.2's
projections were written for a game whose player is on the other side.
There's no dispel-the-good, so *Dispel Good* blasts everything in sight
instead of everything that isn't evil. There's no detect-the-unliving, so
*Detect Unlife* finds every creature nearby, not only the ones without life.
And *Death Ray*, which in Zangband kills outright or fails, is a heavy drain
here, and still useless against the nonliving.

A Rogue and a Ranger each lose seven of the thirty-two here. It's the Rogue's
worst realm, and level with Trump for the Ranger. A Rogue's Necronomicon is the
worst book in the game: four rituals of eight, no *Hellfire* among them, and
two of the four inert. See :ref:`books-short`.

Both of Death's classes cast on borrowed figures, because Zangband had neither
a Necromancer nor a Blackguard. The Necromancer takes Zangband's Mage and the
Blackguard its Paladin.

Casting
-------

Four rules apply to every realm, and the spell list shows none of them.

Your failure chance has a floor and a ceiling. However good you get, most
characters never go below 5 per cent. Only a class with a zero-failure ability
escapes that floor; nothing else does, at any level, with any statistic. At
the other end, no spell ever reads worse than 50 per cent however hopeless the
pairing, so a very hard spell cast by a very unsuitable character is a coin
flip, even where the raw numbers say it's impossible. Being stunned is added
*after* the cap, so a stunned caster can be worse than fifty.

A Necromancer casts badly in the light. A character who draws on unlight (the
Necromancer is the one you'll meet) takes a flat +25 to its failure chance
whenever it's standing on a lit square. What counts is the square under your
feet being lit, by your lantern, by someone else's or by the room; carrying a
light source isn't the penalty in itself. It is the largest single casting
modifier in the game, and the spell list gives no sign of it.

Armour costs you mana. Every spellcasting class can wear a certain weight of
armour before its casting suffers, and past that you lose one point of maximum
mana for every ten tenths of a pound over. A Mage's allowance is thirty pounds
and a Paladin's forty: the more a class leans on its spells, the less it may
wear. The penalty bites hardest early, when your pool is small and a suit of
chain costs most of it, and makes little difference to a character with a
hundred points to spend. Weapons, bows, rings, amulets and your light don't
count; everything else you're wearing does.

You can cast on an empty pool, and it will cost you. Try a spell you can't pay
for and the game tells you so and asks whether you want to attempt it anyway.
If you do:

- the failure chance rises by five percentage points for every point of mana
  you're short;
- you'll very likely faint, for longer the further short you were, and free
  action doesn't prevent it;
- and about half the time you damage your constitution, which on this path
  can be permanent.

All of that happens whether or not the spell goes off. It's a last resort and
it's meant to hurt. The one time it's worth it is the one time you'd otherwise
die.

.. note::

   Gloves do not hinder a caster here. Zangband, following older Angband, took
   mana away from an intelligence-based caster wearing anything on their hands
   unless it gave free action or a dexterity bonus. Angband 4.2 dropped that
   rule and this game hasn't brought it back, so a Mage can wear gauntlets
   with no penalty at all. It's the one casting restriction from the old
   manual that's gone completely.

Who can choose what
-------------------

Every class can study every realm Zangband lets it. That sounds obvious, and it
wasn't always true. The entitlements were right from the start, but a realm's
*books* only went to classes that already had that kind of book, so a Mage was
allowed six realms and had three. Fixing that added forty books and three
hundred and five spells (DEC-57).

In play, that means a Mage can study Death or Life, and a Priest can study
Arcane. That's Zangband's Mage, a far broader class than Angband's, with
healing at one end and *Hellfire* at the other. If you know Angband 4.2, it's
the biggest single change in this chapter.

Whether you *choose* which spell to learn or get a random one depends on your
*class*, never on the book. So a Mage that studies Life picks its prayers, and
a Priest reading the same book takes what it's given. It looks like a bug, but
Zangband does exactly this.

Two classes carried over from the previous milestone are the ones the realm
system exists for:

- A **Warrior-Mage** always studies Arcane in its first slot, and anything else
  it likes in the second. Zangband lists Arcane in both slots, but a realm
  already taken is struck off the next slot's menu, so the second question is
  asked about the other six and a Warrior-Mage always ends up with two
  different realms.
- A **High-Mage** chooses one realm out of seven and gets no second slot. It's
  paid back in figures: it reaches a realm's last spells earlier and more
  cheaply than any other class, in six realms out of seven. Life is the
  exception, because in Zangband's table Life is the Priest's own realm.

Both are entitled to Trump, and both are offered it. Trump used to be the one
entitlement in the table with nothing behind it.

.. _books-short:

Books that come up short
~~~~~~~~~~~~~~~~~~~~~~~~

A Rogue and a Ranger don't get all of a borrowed realm. Every realm is four
books of eight, which for most classes is thirty-two spells. Zangband's table
marks some spells as unreachable for those two, not just expensive, and then a
book holds fewer than eight. Sixteen books in the game are short, and all
sixteen belong to one of those two classes:

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

A Rogue's Necronomicon is the extreme case. It holds four rituals of the eight
(*Death Ray*, *Raise the Dead*, *Esoteria* and *Wraithform*), so *Hellfire*,
*Omnicide*, *Word of Death* and *Evocation* aren't available to a Rogue at any
level. Two of the four it does get are among the realm's inert spells, which
leaves two working rituals in a fifty thousand gold book. The Ranger's Five
Aces is close behind at four cards, one of them inert.

The fourth book of a realm is where its power is, so this is the price those
two classes pay for being entitled to realms they weren't built for. You'll
want to know that before you buy the book.

What has changed, and what has not
----------------------------------

All four of the realms Angband already had now hold Zangband's spells: Life,
Arcane, Nature and Death. Every realm in the game is four books of eight, which
is Zangband's shape throughout. This is DEC-50, and two things follow from it:

- A Priest's prayers and a Mage's spells are different spells now, at
  different levels, in differently named books. A Paladin gains most: Angband
  gave it three of the Priest's books and sixteen prayers, and Zangband's table
  gives it all thirty-two. A Ranger gains nearly as much.
- Savefiles from before the change do not load. A book's identity is its name,
  and every old save holds the town temple's stock, so deleting *[Novice's
  Handbook]* made every one of them unreadable. The game is pre-release, and
  this was decided with the numbers in hand.

Sorcery, Chaos and Trump took nothing away, because no class had them before.
Their books go on the *end* of the lists that gain them: a Mage's four Arcane
books are books one to four, with Sorcery's as five to eight and Chaos's as
nine to twelve.

Your known spells are recorded by their position in a flat list across all of
your class's books. Adding books at the end leaves every position meaning what
it meant, but inserting one anywhere else would shift every spell a saved
character knows one place along: the game would load, the sheet would look
reasonable, and a Priest who had learned Remove Fear would find they knew
something else instead.

One duplicate is visible, and intended: a Mage who takes Arcane and Sorcery has
*Phase Door* in two books, because Zangband put it in both realms and that
character has both. You only ever see the books of the realms you chose, so the
duplicate is exactly the one Zangband showed.
