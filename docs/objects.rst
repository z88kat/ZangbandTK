Objects
=======

ZangbandTK adds 51 artifacts and 18 ego types to Angband's, along with three
weapon properties Angband has no equivalent for.

Three new weapon properties
---------------------------

These are the mechanics most likely to change how you fight, and all three can
appear on artifacts or on ordinary ego items.

**Vampiric.** Drains life from living creatures to heal you. The drain is capped
at the damage actually dealt, so a killing blow heals you for the monster's
remaining health rather than for the overkill. Undead and other non-living
creatures give you nothing. Found on artifacts and on the ``(Vampiric)`` ego.

**Vorpal.** One blow in six cuts far deeper than it should, doubling that blow's
damage. It multiplies the weapon's own contribution rather than your damage
bonuses, so it favours a large weapon over a heavily enchanted small one. Found
on artifacts and on weapons ``of Sharpness``.

**Chaotic.** One blow in seven discharges an unpredictable effect into whatever
you hit — confusion, terror, slowing, stunning, or flinging the target out of
reach entirely. That last one is a genuine drawback and is meant to be: a
chaotic weapon that only ever helped you would not be chaotic. Found on
artifacts and on the ``(Chaotic)`` ego.

All three announce themselves in an item's description, so you will know a
weapon has them once you have identified it.

New artifacts
-------------

The 51 imported artifacts include the regalia of Amber — **Grayswandir**,
Corwin's blade, and **Frakir**, the strangling cord — along with Zangband's
chaos and Mythos artifacts.

A few appear on base items Angband no longer has. Angband retired several object
kinds over the years, and where an artifact's original base was one of them it
has been rehoused on the nearest surviving equivalent. Grayswandir is a cutlass
here because Angband no longer has sabres. The artifact's own damage, weight and
value are unchanged.

New ego types
-------------

Eighteen, including ``(Vampiric)``, ``(Chaotic)``, ``of Sharpness``,
``(Ghoul Touch)``, ``of Immolation``, ``of the Wild``, and the Amber-flavoured
``(Trump Weapon)`` and ``(Pattern Weapon)``.

Ego items are where most players will actually meet Zangband's character.
Artifacts are rare by design; a vampiric long sword is not.

The Ancient and Foul Curse
--------------------------

Some equipment carries the Curse of Topi Ylinen, and you will know it: the item
describes itself as ancient and foul.

What makes it feared is not any single outcome but its shape. Roughly once every
hundred turns, a cursed item stirs and one misfortune befalls you — monsters
woken, something summoned, experience drained, paralysis, a statistic lost, or
your memory torn away. But each of those has a chance of *falling through* to the
next, and the next after that, so a woken monster can escalate into a summoning,
into paralysis, into total amnesia, in a single visitation. When it finishes,
there is a one-in-three chance it simply begins again.

Free action does not make you immune to the paralysis. It gives you a saving
throw, and shortens the paralysis if you fail.

Sometimes the curse stirs and nothing happens at all.

It is worth carrying cursed equipment only if what it gives you is worth this.
Often it is.

Activations
-----------

Zangband scripted its artifact activations in a scripting language ZangbandTK
does not carry. Each has been matched to the nearest Angband activation
instead — an artifact that threw a fireball still throws a fireball, and its
recharge time is preserved, but the effect is Angband's version rather than a
faithful reproduction of Zangband's.

The lotus
---------

There is a mushroom that takes your memory.

It looks like every other mushroom you have not identified — some colour and a
comma on the floor — and eating it says only that you feel a little dizzy. Five
turns later you forget everything you know:

- the map of the level you are standing on;
- the world map, and every town and dungeon mouth you had found on it;
- everything you had learned about every monster;
- what every flavoured thing is, so your potions go back to being coloured
  liquids and have to be drunk to find out what they are;
- and every spell you had learned, which must be studied again from the book.

It takes nothing you cannot get back. No experience, no levels, no items. But
none of it comes back quickly, so the price of eating a strange mushroom is
measured in hours of play rather than in a dead character — which is the point.
A consumable that could end a run outright is one nobody eats twice.

**One thing survives: you always know where home is.** The village you started
in stays on the map, along with the country immediately around it, so there is
always one place you can name and one direction worth walking. That is not
mercy — the magetower only carries you to places you have found, so a character
who had forgotten every place including their own would have a blank map, no
travel and nothing to aim at.

Which is also where the idea comes from. The first of Zelazny's Amber novels
opens on a man who has lost his memory and knows only that there is a place
called Amber, and that he is of it.

**The five turns are deliberate.** Something that took your memory the moment
you swallowed it would be an ordinary bad mushroom. Five turns is long enough to
work out what you have done and not long enough to do anything about it.

Not yet implemented
-------------------

A handful of Zangband object properties are recorded but inactive: weapons that
return when thrown, items that let you pass through walls, and properties tied
to luck, which Angband has no equivalent of. Items carrying them work in every
other respect.
