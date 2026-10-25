Zangband for Nintendo DS and 3DS
================================

Rebuilding the spirit of Zangband on a modern Angband.

  https://zangbandtk.com/


What is in this zip
-------------------

  Zangband-<version>.nds     The DS ROM.
  Zangband-<version>.3dsx    The 3DS build, for the Homebrew Launcher.
  zangbandtk/                The game's data. This is not optional.
  LICENSE.md                 The licence this game is distributed under.
  README.txt                 This file.


What you need
-------------

For the DS build: a DS with a Slot-2 (the GBA slot), a flashcart in Slot-1,
and the Nintendo DS Memory Expansion Pak in Slot-2 -- the 8 MB pak that came
with the DS Browser. The pak is not optional. The game no longer fits in the
DS's own 4 MB, and without the pak it runs out of memory.

It has been played on an original DS with the pak and a DSpico flashcart. A
DSi has no Slot-2, and this build has not been tried on one.

The 3DS build needs none of this.


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


"Unable to access filesystem"
-----------------------------

If the game stops at "Error initializing FAT drivers" and then "Unable to
access filesystem", it has not failed to find its data -- it has failed to
reach the card at all, before looking. This is about DLDI, and it is not
something wrong with the download.

Homebrew on a DS needs a small driver for the particular card it is running
from, and that driver has to be written into the ROM. This is called DLDI
patching. Where you stand depends on how you are launching it:

  Most flashcarts and homebrew menus do it for you. R4 and similar cards,
  the DSpico's Pico Launcher, and loaders like TWiLight Menu++ or the
  Homebrew Menu patch DLDI as they launch. If you use one of those, this
  section does not apply to you.

  A DSi or 3DS launching from its own SD card does not need DLDI at all.
  The 3DS build in this zip is the easier route on that hardware.

  Launching the .nds directly, or in an emulator, usually does need it. Patch
  the ROM with the driver for your card using dlditool, which comes with
  devkitPro:

    dlditool <driver>.dldi Zangband-<version>.nds

  In an emulator, the driver has to match the card the emulator pretends to
  be -- mpcf.dldi for DeSmuME's GBA Movie Player, r4tf.dldi for an emulated
  R4 -- and the emulator's filesystem support has to be turned on and pointed
  at the folder holding "zangbandtk".

In melonDS, use DS mode with its DLDI support turned on and folder sync pointed
at the folder holding "zangbandtk", and insert the Memory Expansion Pak with
File > Insert add-on cart before you start the game. melonDS does not remember
the pak, so it has to go in again each time. Keep a clean copy of the ROM:
melonDS writes its own driver into the file it opens.


Saving
------

There is one save slot, at /zangbandtk/lib/save/PLAYER on the card. That is a
limitation of this port rather than of the game; the other builds let you keep
as many characters as you like.

Back it up by copying that file somewhere else. Savefiles are not compatible
with Angband or the original Zangband, and not with a different version of
this game either. Nor do they move between this build and the desktop ones:
the DS's world is a different size, so a character from one does not belong in
the other's.


Playing
-------

The top screen shows 51 columns of the game's 80. While you are on the map it
keeps you in the middle, and the message line at the top and the status line
at the bottom stay where they are. Anywhere else -- a menu, the character
sheet, a store -- it follows the cursor. L or R pressed on its own slides it
about fourteen columns left or right, for anything wide that is off the edge.

The bottom screen has the keyboard, and under it your race, class, stats,
armour, hit points and where you are, which the desktop builds show down the
left of the map.

  D-pad        move; two directions at once for a diagonal
  A            Enter
  B            Escape
  Y            yes, at a yes/no question
  ` key        also Escape, on the touch keyboard

New characters on the DS start with "Center map continuously" switched on in
the user interface options, which is what keeps you in the middle. A
character made before that will want it switched on by hand.


Things to know
--------------

This is a text-mode build. There are no tilesets and no sound, which is why the
download is small: the graphics and audio in the other releases come to over
twenty megabytes and the DS can use none of it.

The world is smaller here than on the other platforms, and deliberately. A DS
has 4 MB of memory, and the desktop world does not fit: the game would load,
reach character creation and then run out of memory generating the surface. So
this build gets a 260x260 world with one town, where the desktop gets 2064x2064
and a dozen. All thirteen dungeons are still out there to be found.

The live area around you is also smaller, about a screen, so the surface is
rebuilt as you walk. In a town that means the townspeople are placed afresh at
every step, so they seem to jump about. That is a known problem and is being
worked on.

If you would rather have the full-size world and see how far you get, the
settings are in zangbandtk/lib/gamedata/constants.txt on the card -- look for
wild:blocks, wild:block-size and wild:towns, and the comments around them.
Nothing needs rebuilding; the game reads that file at startup.

The DS has 4 MB of memory, and this game asks more of it than Angband does --
1013 monsters against Angband's 624 or so, and a wilderness on top. The
wilderness is the cheap part, about 98 KB for the whole world, because it is
generated from a seed as you walk rather than stored; the game's own data, the
bestiary most of all, is what costs. The Memory Expansion Pak makes up the
difference: the map of the level you are on, and a good part of the game's
text, live in the pak.

This is still the least tested build of any of them. If it does not work, that
is worth reporting rather than assuming it is your setup:

  https://github.com/z88kat/ZangbandTK/issues

The game is early in any case -- a good deal of Zangband is still missing, and
the Features page on the site says what is in and what is not.
