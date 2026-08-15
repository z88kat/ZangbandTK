Objects
=======

ZangbandZK adds 51 artifacts and 18 ego types to Angband's, along with three
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

Activations
-----------

Zangband scripted its artifact activations in a scripting language ZangbandZK
does not carry. Each has been matched to the nearest Angband activation
instead — an artifact that threw a fireball still throws a fireball, and its
recharge time is preserved, but the effect is Angband's version rather than a
faithful reproduction of Zangband's.

Not yet implemented
-------------------

A handful of Zangband object properties are recorded but inactive: weapons that
return when thrown, items that let you pass through walls, and properties tied
to luck, which Angband has no equivalent of. Items carrying them work in every
other respect.
