ZangbandTK
==========

Rebuilding the spirit of Zangband on a modern Angband.

  https://zangbandtk.com/


Installing
----------

Extract the WHOLE folder somewhere and keep it together. ZangbandTK looks for
its data in the lib folder beside the executable, so moving ZangbandTK.exe out
on its own will stop it starting.

Anywhere you can write to is fine: your Desktop, Documents, or a games folder.
Program Files is a poor choice, because the game saves into its own lib\user
folder and Windows restricts writing there.

Then run ZangbandTK.exe.

Releases carry two Windows builds, and the name of the zip says which one you
have. The 64-bit build (win64) is the one to prefer; it is a single executable
with nothing beside it to keep track of. The 32-bit build (win32) is there for
older machines and older Windows, and runs on 64-bit Windows perfectly well
too.


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
network. If you would rather satisfy yourself of that, the complete source is
at the address below and the game can be built from it.


What is in this folder
----------------------

  ZangbandTK.exe   The game.
  lib\             Its data. Do not separate this from the executable.
  lib\user\save    Where your savefiles will go.
  docs\            The manual, in HTML. Open docs\index.html in a browser.
  lib\user\borg.txt  Documentation for the borg, the automatic player.
  changes.txt      Angband's changelog, which this game is built on. For
                   ZangbandTK's own history see the Release log on the site.
  LICENSE.md       The licence this game is distributed under.
  README.txt       This file.

The 32-bit build also has libpng12.dll and zlib1.dll beside the executable, and
needs them. The 64-bit build has no DLLs: everything is linked into the
executable.


Before you start
----------------

Savefiles are NOT compatible with Angband or Zangband, and never will be. Do
not point ZangbandTK at a savefile you care about.

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

ZangbandTK is available under the Angband licence: it may be copied and
distributed for educational, research, and not for profit purposes provided
that the copyright and statement are included in all such copies. Other
copyrights may also apply. In practice that means non-commercial distribution,
the same terms Zangband itself carried. See the manual for the full statement.
