Objects
=======

ZangbandTK adds 86 object kinds, 51 artifacts and 16 ego types to Angband's,
along with three weapon properties Angband has no equivalent for.

Three new weapon properties
---------------------------

These are the mechanics most likely to change how you fight, and all three can
appear on artifacts or on ordinary ego items.

**Vampiric.** Drains life from living creatures to heal you, by the damage each
blow deals. A killing blow drains nothing, as in Zangband: the life has gone
with the monster, so it is the blows that don't kill that feed you. Undead and
other non-living creatures give you nothing. Found on artifacts and on the
``(Vampiric)`` ego.

**Vorpal.** One blow in six cuts far deeper than it should, doubling that blow's
damage. It multiplies the weapon's own contribution and leaves your damage
bonuses alone, so it favours a large weapon over a heavily enchanted small one.
Found on artifacts and on weapons ``of Sharpness``.

**Chaotic.** One blow in seven discharges an unpredictable effect into whatever
you hit: confusion, terror, slowing, stunning, or flinging the target out of
reach entirely. That last one is a drawback, and it's meant to be, since a
chaotic weapon that only ever helped you would not be chaotic. Found on artifacts
and on the ``(Chaotic)`` ego.

All three announce themselves in an item's description, so you'll know a weapon
has them once you have identified it.

New artifacts
-------------

The 51 imported artifacts include Zangband's chaos and Mythos artifacts, and the
regalia of Amber: Grayswandir, Corwin's blade, and Frakir, the strangling cord.

A few were built on base items Angband no longer has. Angband retired several
object kinds over the years, and where an artifact's original base was one of
them it has been rehoused on the nearest surviving equivalent. Grayswandir is a
cutlass here because Angband no longer has sabres. The artifact keeps its own
damage, weight and value.

New ego types
-------------

There are sixteen, including ``(Vampiric)``, ``(Chaotic)``, ``of Sharpness``,
``(Ghoul Touch)``, ``of Immolation``, and the Amber-flavoured
``(Trump Weapon)`` and ``(Pattern Weapon)``.

Ego items are where most players will meet Zangband's character. Artifacts are
rare, and a vampiric long sword is not.

New objects
-----------

There are eighty-six of Zangband's own object kinds, which is most of what it
had that Angband does not: thirteen swords including the Diamond Edge and the
Psiblade, nine polearms, eight hafted weapons, eighteen pieces of armour from the
T-shirt to the feathered Hagaromo, and twenty-five rings and amulets. The
remaining thirteen are consumables: five potions, four scrolls, two rods, a food
and an arrow.

The jewellery is where the interesting things are. An Amulet of Anti-Magic stops
you casting at all, and an Amulet of Anti-Teleportation roots you where you
stand, which is a drawback until the day something tries to teleport you away. A
Ring of Lordly Protection is what its name suggests. An Amulet of Destruction is
not.

A Ring of Wizardry gives a caster more mana. The number on it is per level you
can cast at, so it is worth a little to a novice and a great deal to someone who
has been at it a while. The Ring of Fate has a section of its own, below.

Among the consumables, a Rod of Havoc picks one of twenty-five damage types and
throws it at whatever you aimed at, as a ball or occasionally as a beam, and is
never twice the same rod. A Scroll of Mundanity strips an item back to the plain
thing it was made as: no artifact, no ego, base dice, base armour, and the
identification forgotten with them. It is the only way to be rid of something
that will not come off, and it's indiscriminate. Nothing warns you what you're
about to lose. (A magical figurine keeps its monster, which is what it was made
as.)

Zangband renamed a good deal of what it inherited from Angband. Its Ring of Skill
is Angband's Ring of Accuracy, and its Scroll of Enchant Weapon Deadliness is
Enchant Weapon To-Dam, so importing those by name would have put two of each in
the game under two names. They are compared by the slot they occupy instead of by
what they're called, so the ten renamed ones are recognised as things Angband
already has.

The Ring of Fate
----------------

It sounds like a lucky charm, and it isn't, quite. It sharpens every critical
hit in the game by half again, including the ones landed on you. And roughly one
monster in thirteen generated while you wear it comes from deeper than the level
should produce, sometimes forty levels deeper.

So it is a good ring for a character who kills things quickly and a very bad one
for a character who doesn't, and it will eventually introduce you to something
you were not ready for. Zangband's own source comments the third of its four
effects with "Luck isn't always good for you...", which is the whole item in six
words.

The Ancient and Foul Curse
--------------------------

Some equipment has the Curse of Topi Ylinen, and you'll know it: the item
describes itself as ancient and foul.

Roughly once every hundred turns, a cursed item stirs and one misfortune befalls
you: monsters woken, something summoned, experience drained, paralysis, a
statistic lost, or your memory torn away. But each of those has a chance of
*falling through* to the next, and the next after that, so a woken monster can
escalate into a summoning, into paralysis, into total amnesia, in a single
visitation. When it finishes, there's a one-in-three chance it begins again.
Sometimes the curse stirs and nothing happens at all.

Free action does not make you immune to the paralysis. It gives you a saving
throw, and shortens the paralysis if you fail.

Cursed equipment is only worth carrying if what it gives you makes up for all
this, and often it does.

Activations
-----------

Zangband scripted its artifact activations in a scripting language ZangbandTK
doesn't have, so each one uses the nearest Angband activation instead. An
artifact that threw a fireball still throws a fireball, with the same recharge
time, but the effect is Angband's version and won't behave exactly like
Zangband's.

The lotus
---------

There's a mushroom that takes your memory. It looks like every other mushroom
you haven't identified, some colour and a comma on the floor, and eating it says
only that you feel a little dizzy. Five turns later you forget everything you
know:

- the map of the level you are standing on;
- the world map, and every town and dungeon mouth you had found on it;
- everything you had learned about every monster;
- what every flavoured thing is, so your potions go back to being coloured
  liquids and have to be drunk to find out what they are;
- and every spell you had learned, which must be studied again from the book.

It takes nothing you cannot get back: no experience, no levels, no items. But
none of it comes back quickly, so the price of eating a strange mushroom is
measured in hours of play. A consumable that could end a run outright is one
nobody eats twice.

One thing survives: you always know where home is. The village you started in
stays on the map, along with the country immediately around it, so there's
always one place you can name and one direction worth walking. The reason is
the magetower, which only carries you to places you've found. A character who
had forgotten every place including their own would have a blank map, no travel
and nothing to aim at.

That's also where the idea comes from. The first of Zelazny's Amber novels opens
on a man who has lost his memory and knows only that there is a place called
Amber, and that he is of it.

Something that took your memory the moment you swallowed it would be an ordinary
bad mushroom. The five turns are long enough to work out what you've done, and
not long enough to do anything about it.

Eight more from Zangband
------------------------

**Returning.** A thrown weapon comes back to your hand, nineteen throws in
twenty. The twentieth is what keeps it a weapon you can lose.

**Luck.** Improves your saving throw. Zangband wrote this as a flat value, which
made a lucky novice better at shrugging things off than an unlucky veteran. Here
it's the bonus the name promises.

**A ghoul's touch.** What you strike falls asleep, but only if you strike it
bare-handed. Gloves of this kind are for someone who fights with their hands,
and putting a weapon in them turns the property off entirely.

**Easily enchanted.** Takes twice as many enchantment attempts, and an artifact
so made does not get its usual chance to resist being enchanted at all.

**Weird luck.** The Ring of Fate's property, described above, with both its
edges and a third thing besides. Once in about seven levels gained, something in
the Courts of Chaos notices you and hands down a reward. It won't be a Lord of
your own, and one that was never yours is gentler than one that is: it takes a
passing interest in you, and has no claim on you.

**Magic-powered criticals.** A weapon that spends your mana to land critical
hits, and to make the ones it lands worse (one to three points each time). The
Psiblade has it. With an empty mana pool it does nothing at all, so it's only a
warrior's weapon in the hands of a warrior who casts.

**An anti-magic shell.** You cannot cast. There's no reduced chance or higher
cost: the spell just doesn't happen. Studying and browsing still work, so you
can keep learning spells you are unable to use.

**A chaos patron.** The ``(Chaotic)`` ego has it. A Lord of the Courts takes an
interest in whoever holds it, and every level you gain from then on brings a
reward from the same ladder a Chaos-Warrior climbs, from the generous down to
the actively hostile. Wearing one is not the same as being sworn to one, and the
difference runs the way you wouldn't guess: a Lord with no claim on you is
*less* likely to reach for the bottom of the ladder. It's only glancing over,
and the borrowed reward is a smaller thing in both directions.

Not yet implemented
-------------------

One Zangband object property is left off every item: passing through walls.
A Spectre walks through walls, and so does anyone in wraith form (see
:doc:`mutations`), but no item grants it, because wall-walking you can put on
takes the dungeon away as an obstacle. Zangband's one item with it, the cloak
'Soulless One', comes without it and works in every other respect.

Two more object properties are recorded and inactive, because each cancels a
penalty this game does not impose. ``WILD_SHOT`` ignores the cover a
monster gets from standing in trees, and Angband has no terrain term in its
combat at all. ``WILD_WALK`` removes the extra effort of climbing a mountain and
the poison of wading a swamp, but here a mountainside is a wall, not slow
ground, and a swamp is mud and shallow water that poisons nobody. Building
either flag alone would do nothing. Building what they cancel would change how
the wilderness plays, which is a bigger job than converting a flag. One ego,
``of the Wild``, waits with them: every property it had was ``WILD_WALK``, so
importing it would have put a pair of boots in the game that granted nothing at
all.

Two of Zangband's object kinds are likewise recorded but not imported:

- A Scroll of Artifact Creation, which makes a new artifact out of an ordinary
  item during play. Angband generates its random artifacts once, at birth, and
  stores an artifact in the savefile by name, so one invented mid-game would not
  be there when the character was loaded again.
- A Wand of Rockets, whose damage is its own element (it stuns, and shard
  resistance halves it), and adding an element changes the savefile layout.

Thirteen more had to wait for something else in the game, and all thirteen are
in. The Wand of Tame Monster
needed pets, and has them (:doc:`pets`). The Potion of New Life strips every
mutation you carry (:doc:`mutations`); Zangband's version also re-rolled your
hit points, and that half of it isn't built yet. The Magical Figurine needed an
object that could name a monster: *a Magical Figurine of a cave spider*, thrown,
breaks and leaves the cave spider behind as your pet (:doc:`pets`).

The other ten are statues, which needed the same thing and then a reason. A
statue is a monster rendered in a material (*a Golden Statue of Smeagol*; any
monster at all, uniques included), and it does nothing. It is treasure, worth
what its material is worth, from 75 gold for wood to 10,000 for mithril, and
it's heavy: twenty pounds for the wooden one and fifty for the rest. Statues are
found from level 9 down, never stack, and no monster will pick one up. The
**Statue Store**, which Zangband's great cities had and this game turned away
while it had no statues to sell, buys and sells them, and the general store and
the black market will take one too (:doc:`towns`).

The Scroll of Rumour is dropped for good. Zangband's 647 rumours are in the
archive, and Angband already has the machinery to hold a list like it, so the
mechanism was never the obstacle. But a good many of the lines name things this
game doesn't have or contradict what it does, and a scroll whose whole purpose
is to tell you something true is the wrong place for text that isn't. Sorting
the true from the false is 647 judgements for one flavour scroll, and it is not
worth them.
