ZangbandTK for Nintendo DS and 3DS
==================================

Rebuilding the spirit of Zangband on a modern Angband.

  https://zangbandtk.com/


What is in this zip
-------------------

  ZangbandTK-<version>.nds   The DS ROM.
  ZangbandTK-<version>.3dsx  The 3DS build, for the Homebrew Launcher.
  zangbandtk/                The game's data. This is not optional.
  LICENSE.md                 The licence this game is distributed under.
  README.txt                 This file.


Installing
----------

The game reads its data from the card rather than from inside the ROM, so both
pieces have to be copied across.

  1. Copy the whole "zangbandtk" folder to the ROOT of your SD card, so that
     the data ends up at /zangbandtk/lib/ on the card. Not inside any other
     folder.

  2. Copy the .nds file wherever you keep your ROMs and launch it from your
     flashcart's menu as usual. On a 3DS, copy the .3dsx to /3ds/ instead and
     launch it from the Homebrew Launcher.

If the game starts and then cannot find its files, step 1 is almost always the
reason: the folder is one level too deep. The path on the card has to be
exactly /zangbandtk/lib/gamedata, and so on.


Saving
------

There is one save slot, at /zangbandtk/lib/save/PLAYER on the card. That is a
limitation of this port rather than of the game; the other builds let you keep
as many characters as you like.

Back it up by copying that file somewhere else. Savefiles are not compatible
with Angband or Zangband, and not with a different version of ZangbandTK
either.


Things to know
--------------

This is a text-mode build. There are no tilesets and no sound, which is why the
download is small: the graphics and audio in the other releases come to over
twenty megabytes and the DS can use none of it.

The DS has 4 MB of memory, and ZangbandTK asks more of it than Angband does --
1013 monsters against Angband's 624 or so, and a wilderness on top. The
wilderness is the cheap part, about 98 KB for the whole world, because it is
generated from a seed as you walk rather than stored; the bestiary is what
costs. If memory does run out it will happen while loading, before you reach
character creation.

If you have a DS RAM expansion pak in Slot-2, this port knows how to use it.
That is the first thing to try if the game will not start.

Nobody here plays on a DS, so this is the least tested build of any of them. If
it does not work, that is worth reporting rather than assuming it is your
setup:

  https://github.com/z88kat/ZangbandTK/issues

The game is early in any case -- a good deal of Zangband is still missing, and
the Features page on the site says what is in and what is not.
