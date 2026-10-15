ZangbandTK
==========

Rebuilding the spirit of Zangband on a modern Angband.

  https://zangbandtk.com/


What this is
------------

ZangbandTK is Zangband played through a front end modelled on ZangbandTk, the
Zangband of the late 1990s: the map and its panes in the main window, the
character, the inventory, the spell books and the shops in windows of their
own, and the original's menus. It is the same game as Zangband.exe, the other
Windows download, and it is newer: some of it is still being finished.

Sound and your own music are on the Sound page of the Options window, as on
the Mac. The game's sounds are off until you turn on Use Sound; the music comes
from a folder of your own, and MP3, FLAC, Ogg, Opus, WAV and AIFF play -- not
AAC, which the Mac's player has and this one does not.


Installing
----------

Extract the WHOLE folder somewhere and keep it together. ZangbandTK.exe needs
the DLLs beside it, and finds its data in the lib and tcltk folders beside it;
moved out on its own it will not start.

Anywhere you can write to is fine: your Desktop, Documents, or a games folder.
Program Files is a poor choice, because the game saves into its own lib\user
folder and Windows restricts writing there.

Then run ZangbandTK.exe.

This build is 64-bit, for 64-bit Windows.


Windows will warn you, once
---------------------------

Windows marks anything downloaded from the internet, so the first time you run
ZangbandTK you will probably see:

  "Windows protected your PC. Microsoft Defender SmartScreen prevented an
   unrecognised app from starting."

This is not a virus warning. SmartScreen is saying it does not recognise the
publisher -- and it will not, because publisher recognition comes from an
Authenticode code-signing certificate, which costs a few hundred pounds a year
and which this project does not have.

To run it anyway:

  Click "More info", then "Run anyway".

Windows remembers, and will not ask again for that copy.

It is tidier to clear the mark on the zip before extracting anything, which
saves being asked about the files inside:

  1. Right-click the downloaded .zip file and choose Properties.
  2. At the bottom of the General tab, tick "Unblock", then click OK.
  3. Extract the zip as normal.

Your antivirus may also take an interest, for the same reason: an unsigned
executable from an unknown publisher. Nothing in ZangbandTK talks to the
network, apart from Help's "Open in Browser", which opens the manual's page in
your own browser. If you would rather satisfy yourself of that, the complete
source is at the address below and the game can be built from it.


What is in this folder
----------------------

  ZangbandTK.exe     The game.
  *.dll              Tcl and Tk, which the game's windows are built with.
  lib\               Its data. Do not separate this from the executable.
  lib\user\save      Where your savefiles will go.
  lib\user\borg.txt  Documentation for the borg, the automatic player.
  tcltk\             Tcl's and Tk's own scripts.
  docs\              The manual, in HTML. Open docs\index.html in a browser.
                     The game's Help menu has the same manual inside it.
  LICENSE.md         The licence this game is distributed under.
  README.txt         This file.

Your characters are in lib\user\save, as Zangband.exe keeps its own. A
character made in one can be carried on in the other by copying its savefile
across.


Before you start
----------------

Savefiles are NOT compatible with Angband or with the original Zangband, and
never will be. Do not point this game at a savefile you care about.

The game is early. It is playable and already feels different from Angband,
but a good deal of Zangband is still missing. The Features page on the site
says exactly what is in and what is not.

If you have played Angband before, read "How Balance Differs" in the manual
first. It is the shortest account of what will kill you that would not have
before.

This Windows build gets less testing than the macOS one, which is the primary
platform. If something is wrong with it, that is worth hearing about.


Problems
--------

Bugs, build failures and questions:

  https://github.com/z88kat/ZangbandTK/issues


Licence
-------

This game is available under the Angband licence: it may be copied and
distributed for educational, research, and not for profit purposes provided
that the copyright and statement are included in all such copies. Other
copyrights may also apply. In practice that means non-commercial distribution,
the same terms Zangband itself carried. See the manual for the full statement.
