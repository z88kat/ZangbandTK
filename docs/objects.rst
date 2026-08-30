Objects
=======

ZangbandTK adds 85 object kinds, 51 artifacts and 17 ego types to Angband's,
along with three weapon properties Angband has no equivalent for.

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

Seventeen, including ``(Vampiric)``, ``(Chaotic)``, ``of Sharpness``,
``(Ghoul Touch)``, ``of Immolation``, and the Amber-flavoured
``(Trump Weapon)`` and ``(Pattern Weapon)``.

Ego items are where most players will actually meet Zangband's character.
Artifacts are rare by design; a vampiric long sword is not.

New objects
-----------

Eighty-two of Zangband's own object kinds, which is most of what it had that
Angband does not: thirteen swords including the **Diamond Edge** and the
**Psiblade**, nine polearms, eight hafted weapons, nineteen pieces of armour
from the **T-shirt** to the feathered **Hagaromo**, and twenty-eight rings and
amulets.

The jewellery is where the interesting things are. An **Amulet of Anti-Magic**
stops you casting at all; an **Amulet of Anti-Teleportation** roots you where
you stand, which is a drawback until the day something tries to teleport you
away. A **Ring of Lordly Protection** is what its name suggests. An **Amulet of
Destruction** is not.

A **Ring of Wizardry** gives a caster more mana — and the number on it is per
level you can cast at, not a flat bonus, so it is worth a little to a novice
and a great deal to someone who has been at it a while.

A **Ring of Fate** deserves its own paragraph, below.

Among the consumables: a **Rod of Havoc**, which picks one of twenty-five
damage types and throws it at whatever you aimed at, as a ball or occasionally
as a beam, and is never twice the same rod. And a **Scroll of Mundanity**,
which strips an item back to the plain thing it was made as — no artifact, no
ego, base dice, base armour, and the identification forgotten with them. It is
the only way to be rid of something that will not come off, and it is
indiscriminate: nothing warns you what you are about to lose.

Some care went into what *not* to import. Zangband renamed a good deal of what
it inherited from Angband — its Ring of Skill is Angband's Ring of Accuracy,
its Scroll of Enchant Weapon Deadliness is Enchant Weapon To-Dam — and
importing those by name would have put two of each in the game under two names.
They are compared by the slot they occupy rather than by what they are called,
so the ten renamed ones are recognised as things Angband already has.

The Ring of Fate
----------------

It sounds like a lucky charm. It is not, quite.

What it actually does is sharpen every critical hit in the game by half again —
**including the ones landed on you**. And roughly one monster in thirteen
generated while you wear it comes from deeper than the level should produce,
sometimes forty levels deeper.

So it is a good ring for a character who kills things quickly and a very bad
one for a character who does not, and it will eventually introduce you to
something you were not ready for. Zangband's own source comments the third of
its four effects with "Luck isn't always good for you...", which is the whole
item in six words.

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

Seven more from Zangband
------------------------

**Returning.** A thrown weapon comes back to your hand, nineteen throws in
twenty. The twentieth is what keeps it a weapon you can lose.

**Luck.** Improves your saving throw. Zangband wrote this as a flat value
rather than a bonus, which made a lucky novice better at shrugging things off
than an unlucky veteran; here it is the bonus the name promises.

**A ghoul's touch.** What you strike falls asleep — but only if you strike it
bare-handed. Gloves of this kind are for someone who fights with their hands,
and putting a weapon in them turns the property off entirely.

**Easily enchanted.** Takes twice as many enchantment attempts, and an artifact
so made does not get its usual chance to resist being enchanted at all.

**Weird luck.** The Ring of Fate's property, described above. Both edges of it —
and a third thing besides: once in about seven levels gained, something in the
Courts of Chaos notices you and hands down a reward. It will not be a Lord of
your own, and one that was never yours is gentler than one that is — a passing
interest rather than a claim.

**Magic-powered criticals.** A weapon that spends your mana to land critical
hits, and to make the ones it lands worse — one to three points each time. The
**Psiblade** carries it. With an empty mana pool it does nothing at all, which
makes it a warrior's weapon only in the hands of a warrior who casts.

**An anti-magic shell.** You cannot cast. Not a reduced chance, not a higher
cost: the spell simply does not happen. Studying and browsing still work, so
you can keep learning spells you are unable to use.

**A chaos patron.** Carried by the ``(Chaotic)`` ego. A Lord of the Courts
takes an interest in whoever holds it, and every level you gain from then on
brings a reward — the same ladder a Chaos-Warrior climbs, from the genuinely
generous down to the actively hostile. Wearing one is not the same as being
sworn to one, and the difference runs the way you would not guess: a Lord with
no claim on you is *less* likely to reach for the bottom of the ladder, not
more. It is glancing over rather than keeping accounts, and the borrowed reward
is a smaller thing in both directions.

Not yet implemented
-------------------

One Zangband object property is recorded but inactive: passing through walls.
Angband has no form of player wall-movement to hang it on — not a spell, not a
temporary state, nothing — so carrying it would mean building wall-phasing for
the player rather than reading a flag. Items with it work in every other
respect.

Two more object properties are recorded and inactive, and both are the same
kind of case: each cancels a penalty this game does not impose. ``WILD_SHOT``
ignores the cover a monster gets from standing in trees, and Angband has no
terrain term in its combat at all. ``WILD_WALK`` removes the extra effort of
climbing a mountain and the poison of wading a swamp — but here a mountainside
is a wall rather than slow ground, and there is no swamp terrain. Building
either flag alone would do nothing; building what they cancel is a change to
how the wilderness plays rather than a conversion of a flag. One ego,
``of the Wild``, waits with them: every property it had was ``WILD_WALK``, so
importing it would have put a pair of boots in the game that granted nothing at
all.

Nineteen of Zangband's object kinds are likewise recorded but not imported,
each for want of a mechanism rather than out of preference:

- **Ten statues and a figurine.** A Zangband statue is a monster rendered in
  a material — the name reads *Mithril Statue of a Balrog* — and the figurine
  is thrown to break and release the creature as a pet. Angband's object names
  are fixed text and an object cannot refer to a monster, so a statue here
  would be a heavy lump with the interesting part missing. The figurine needs
  pets as well.
- **A Wand of Tame Monster**, for the same reason: it makes a pet.
- **A Potion of New Life**, which re-rolls your hit points and cures every
  mutation. Neither mechanism exists yet.
- **A Scroll of Artifact Creation**, which makes a new artifact out of an
  ordinary item during play. Angband generates its random artifacts once, at
  birth, and stores an artifact in the savefile by name — so one invented
  mid-game would not be there when the character was loaded again.
- **A Wand of Rockets**, whose damage is its own element — it stuns, and shard
  resistance halves it — and adding an element changes the savefile layout.
- **A Scroll of Rumour**, and this one is waiting on a decision rather than on
  code. Zangband's 647 rumours are in the archive and Angband already has the
  machinery to hold a list like it, but a good many of them name things this
  game does not have or contradict what it does. A scroll whose whole purpose
  is to tell you something true is the wrong place for text that is not.
