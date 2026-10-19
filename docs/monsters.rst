Monsters
========

ZangbandTK's bestiary is Angband's, plus 387 creatures drawn from Zangband.
There are 1013 in total, and the additions aren't evenly spread: shallow levels
are much as you remember them, but the deep dungeon holds a great deal you
won't.

Where the new monsters come from
--------------------------------

Angband's monsters are Tolkien's. Zangband layered three further sources on
top, and those are what ZangbandTK imports.

The first is Roger Zelazny's Amber. Its scions appear as deep uniques, along
with their servants and creatures. They are among the most dangerous things in
the game and are meant to be met late. There are twelve of Oberon's blood:
Oberon himself, Benedict, Corwin, Eric, Caine, Gerard, Julian, Bleys, Fiona,
Brand, Dworkin and Rinaldo. The game treats the twelve as a family. Look one up
and its recall calls it an *Amberite*, some things in the depths can call the
whole house down on you at once, and :ref:`blood-curse` below covers what
killing one may cost.

The second is the Cthulhu Mythos. Nyarlathotep, Hastur, Shub-Niggurath and
Tsathoggua dwell at the bottom of the dungeon, attended by formless spawn and
dark young.

The third is Chaos: creatures of raw chaos, unstable and unpredictable,
scattered throughout the depths.

Monsters that are not what they appear
--------------------------------------

Zangband is fond of monsters that impersonate scenery, and ZangbandTK keeps
them. A weapon lying on the floor may be a **death sword**; a cloak may be a
**cloaker**; a door may be a **door mimic**; a patch of floor may be something
considerably worse. Two of these classes are new to this game, and since Angband
has no symbol for them they get their own:

- ``#`` is for wall monsters: sentient stone, drifting mists, and things that
  live inside the dungeon's structure
- ``|`` is for animated weapons, which hunt rather than wait

The rest borrow Angband's own symbols where its bestiary already had a suitable
class: floating spheres display as vortices, ents as trees, standing water as
elementals.

.. _blood-curse:

The blood curse of Amber
------------------------

Killing one of Oberon's blood is not free. About half the time, a dying Amberite
lays a blood curse on whoever killed it: your equipment is cursed, and the
Ancient and Foul Curse falls on you two to four times over. That curse is the
worst thing in the game (see :doc:`objects`), and having it arrive several times
in a row, at the end of a fight you only just won, is how a good character dies.

There's no way to prevent it and no saving throw against it. The only choice you
get is whether to make the kill at all, and whether to make it in a state fit to
survive the aftermath.

Zangband filed this under a two-word comment: *don't kill Amberites*. It comes
out of the books, where a dying prince's curse is a lasting weapon. Corwin lays
one on Eric at the end of *Nine Princes in Amber*, and it outlives them both.

Monsters that talk
------------------

Eighty-nine of the imported monsters have something to say. They speak in a
fight, roughly one turn in eight and only when you can see them. They say
something different once they have turned and run, and they have a last word when
they die. It's Zangband's own writing, and it is a large part of why its dungeon
felt inhabited.

One of those last words can pay. When a unique that could talk dies, about one
time in ten it turns out there was a price on its head:

.. code-block:: text

   There was a price on Robin Hood, the Outlaw's head.
   Robin Hood, the Outlaw was wanted for smuggling.
   You collect a reward of 4750 gold pieces.

The reward scales with the monster's level and runs from 250 gold to 32,000, so
a deep unique that happened to be wanted is worth a great deal more than its
drop. You cannot seek this out. There's no telling in advance which monsters were
wanted, and the same monster isn't always wanted twice. But it's a reason not to
scroll past what the dead say.

Uniques cannot be put out of the fight
--------------------------------------

A unique monster is never put to sleep, slowed, confused or frightened by
anything you do to it. There's no saving throw to roll: it doesn't happen at any
character level, from any spell, wand, staff, scroll or racial power. It is
Zangband's rule, because the unique is the fight the game means you to have, and
none of those four is a way around it.

That's a change from Angband, which gives a unique a stiff saving throw against
all four, so a low-level unique there can be slept by a determined caster. If
you've played 4.2 and expect *Sleep Monsters* to work on Bullroarer, it won't.

There are four things the rule does **not** cover:

- Stunning and holding still work, because Zangband does not exempt uniques from
  those. A unique can be stunned by a heavy blow like anything else.
- Damage still breaks its nerve. A unique badly enough hurt flees, the same as
  any other monster. The immunity is to being *frightened by an effect*, not to
  fear itself, and a wounded unique running for a corridor is ordinary play.
- Everything that does damage still does it.
- Non-uniques are unchanged. They keep the graded saving throw, so the same
  spell that bounces off a unique may well work on its escort.

Monsters that will not be moved
-------------------------------

A hundred and two of the imported monsters resist being teleported. That is the
answer to a tactic Angband lets you rely on, where *Teleport Other* removes
anything inconvenient. Against these it may just fail.

How often it fails depends on the monster:

- A unique that has it is *unaffected*, every time. If you were planning to
  teleport Oberon out of the way and fight his escort, plan something else.
- Anything else gets a saving throw, and a heavy monster makes it more often
  than a light one. The same creature may be shifted one turn and stand its
  ground the next, so a failed attempt is worth repeating, up to a point.

You'll see ``is unaffected!`` for the first and ``resists!`` for the second,
which is how you tell which you are dealing with. Once you've seen it happen, the
monster's recall records it: *cannot be teleported*.

It applies to every way the game has of moving a monster against its will: the
teleport-other effects, the blink a nexus attack causes, and the shove of a
gravity attack. There's no side door.

It also refuses to be moved towards you. The swap-position mutation trades your
square for a monster's, and a monster with this flag will not make the trade: you
get ``Your teleportation is blocked!`` and neither of you moves. So the flag is a
defence against being brought closer as well as against being sent away.

Things it is a mistake to touch
-------------------------------

Fifty-nine of the imported monsters are wrapped in something. Strike one in
melee and it answers: ``You are suddenly very hot!``, ``very cold``, or ``You
get zapped!``, and the damage rises with how big the monster is and how deep it
lives. Immunity to the element stops it dead, and resistance only blunts it.
Some have two at once, and the recall shows both: *surrounded by flames and
electricity*.

A monster wrapped in fire will not enter water, neither the deep sea nor the
shallows. It's the oldest tactical fact in this game that Zangband wrote down
and never got to use, having no water worth the name. Here there's a shoreline,
and stepping into it is a way to break off a fight you're losing.

Twenty-two others reflect bolt spells, about nine times in ten. Only a single
bolt is reflected, never a ball or a breath. The bolt does not come back at you:
it goes to a grid *beside* you, so it may well miss, and it may equally well hit
something standing next to you.

You can have all three yourself. A shield of Reflection, or a cloak of
Immolation or of Electricity, does to a monster exactly what the monster's aura
does to you, on the same terms. A monster immune to the element shrugs it off,
and your reflected bolt scatters beside whatever cast it.

Five that will not hold still
-----------------------------

The **chaos shapechanger**, the **lord of chaos**, the **unmaker**,
**Nyarlathotep** and **Dworkin Barimen** are never drawn as themselves. Each
time the display refreshes, which for these is every game turn, the symbol is
replaced by that of some other creature entirely, and about one time in
twenty-five by an *object* instead. A shapechanger looks like a kobold, then a
dragon, then a mushroom, then something else.

Nothing about the monster changes. It has the same stats, the same attacks and
the same behaviour throughout, and only the glyph moves. The obvious guess, that
the thing has become a dragon and should be fought like one, is wrong, and
acting on it is how the flag kills people.

Four of the five are creatures of chaos, and the fifth is Dworkin, who drew the
Pattern and lost his mind doing it. In this game he is both a shapechanger and
one of Amber's blood, and he's the only monster who is both.

The one that may not be there
-----------------------------

There's a single **quantum monster** in the game, and it is a poor thing to rely
on. Half of what you swing at it passes through. That isn't a miss you can
improve on with skill; the attack just didn't happen. It takes only half its own
turns. And now and then it stops existing. It doesn't die, it leaves no corpse
and you get no experience: there's just an absence where it was.

You cannot be sent to kill one, because a quest whose target can evaporate is a
quest that can't be finished.

How dangerous are they?
-----------------------

Every monster in the game, Angband's and Zangband's alike, has 73% of its
Angband hit points and 50% of its armour class. See :doc:`balance` for why, and
for how to change it.

Imported monsters don't keep Zangband's numbers. They were placed on Angband's
own difficulty curve, so a Zangband monster at depth 40 should be a reasonable
match for an Angband monster at depth 40. Where Zangband's version was unusually
tough or unusually fragile for its depth, it still is.

This is the least-tested part of the game. If something at a given depth feels
badly out of place, please report it.

Rooms full of them
------------------

Angband gathers monsters of a kind into pits and nests, and ZangbandTK's imports
make three more of those possible:

- Forests: ents, huorns and the older things of the wood
- The living dungeon: sentient walls and the mists that drift between them
- Elemental: spirits and elementals of the four elements

Not yet implemented
-------------------

Zangband also tagged every monster with the terrain it prefers and the dungeons
it belongs in. Those tags are unused here, because this game decides both
questions a different way: a dungeon lists the *kinds* of creature that live in
it, and a fish is kept in the water by being a fish. The tags are still recorded
against the monsters, in case that ever changes.

Things that live in the water
-----------------------------

Twenty-four of the imported monsters are aquatic, drawn as ``N``, and they are the
only creatures in the game with a place of their own to live. They're in the
open sea and the shallows, in the flooded dungeons beneath Amber (Faiella-Bionin
and Rebma), and nowhere else at all. A shark cannot come ashore.

They arrived badly. Zangband drew its aquatic monsters with ``l`` and Angband
draws *trees* with ``l``, so the import gave every fish, whale and kraken in the
game the character of a tree: unable to feel fear, unable to be confused,
regenerating, and, because dungeons choose their inhabitants by kind, living in
the forest of Arden. That's fixed, and fixing it is why they have a letter of
their own.

Not everything is there to be fought
------------------------------------

A **white deer** is a ``q`` in white, standing too still and too unafraid of
you. Walk into it and it heals you, all of it, whatever you were down to. Then
it bounds away far enough to be out of earshot and carries on grazing somewhere
else. It goes that far because a deer moves quickly and hears well, and one
that bounded only a few paces knew exactly where you were and came straight back
to be refused again.

It does that once, and then no beast will do it again for a while. For about
five hundred turns you're carrying a blessing, and every deer you meet shies
from your hand. Follow the one that blessed you and it won't oblige, and nor will
the next one you find. Reloading doesn't persuade them either, because what
remembers is you.

That's what keeps it balanced. A full heal for nothing is worth having. A full
heal for nothing that can be had again a dozen paces later is a character who
never needs a potion, and deer are common.

You can kill it, if you want to. It carries nothing, is worth no experience, and
won't fight back.

Deer are uncommon and live in quiet country, so finding one is luck.

And there's one of them that isn't a deer. **The Unicorn of Amber** is
silver-white, watches you with an interest you have not earned, and is always
about to leave. She is the oldest thing in Amber and there's exactly one of her
in the world.

Walk into her and she makes you whole: your wounds, and everything the healer in
a town will sell you and charge for. That's the poison, the cuts, the stunning,
the blindness, the confusion, the fear, your drained stats, and the levels a
life-draining touch took off you, all of it at once, for nothing.

She does it once. Then she goes about her business, and if you find her again
she'll shy from your hand like any other beast.

She will not fight you. Killing her is possible, and it's exactly the sort of
thing the Courts would do.

Things that fly, and things that swim
-------------------------------------

Deep water stops most creatures, and it's meant to, because wading out of reach
is one of the few ways to break a pursuit in open country. It does not stop
everything. 117 of the imported monsters fly and 91 swim, and those cross the
water after you. A raven doesn't resist drowning; it just never touches the
surface.

What flies also crosses lava, which is the same rule seen from the other side.
And a creature of the water stays in the water unless it can also fly, so the
sea is a wall for the shark in both directions.
