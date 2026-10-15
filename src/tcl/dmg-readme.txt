ZangbandTK
==========

Rebuilding the spirit of Zangband on a modern Angband.

  https://zangbandtk.com/


What this is
------------

ZangbandTK.app is Zangband played through a different front end: one
modelled on ZangbandTk, the Zangband of the late 1990s, with the map and its
panes in the main window, the character, the inventory, the spell books and
the shops in windows of their own, and the original's menus. It is the same game as
Zangband.app, the other Mac download, and it is newer: some of it is still
being finished.

The two can be installed side by side. They share one folder of characters
and scores, Documents/ZangbandTK in your home folder, so a character made in
one can be carried on in the other.


Installing
----------

Drag ZangbandTK.app to your Applications folder, or anywhere else you like.

If there is a ZangbandTK.app there already, look before you let the Finder
replace it. Up to version 3.128.0 that name belonged to the other application,
the one now called Zangband.app, and replacing it swaps that application for
this one. Your characters are safe either way -- neither application keeps
them inside itself -- but to have both, install Zangband.app from
Zangband-<version>-osx.dmg and delete the old ZangbandTK.app before adding
this one.

This build is for Apple Silicon Macs (M1 and later) only. On an Intel Mac,
take Zangband-<version>-osx-intel.dmg from the same Releases page instead:
that is Zangband.app, the other front end.


The first launch: macOS will refuse, once
-----------------------------------------

The first time you open ZangbandTK, macOS will stop you with a message like:

  "Apple could not verify 'ZangbandTK' is free of malware that may harm
   your Mac or compromise your privacy."

Nothing is wrong with the download, and this is not a virus warning. macOS
is saying it cannot tell WHO made the application -- not that it found
anything wrong with it. Removing that message for good requires a paid Apple
Developer ID and Apple's notarization service, which this project does not
have.

To open it, do this once:

  1. Try to open ZangbandTK normally. You will get the message above.
     Click Done. This step matters: macOS will not offer the choice below
     until you have been blocked at least once.

  2. Open System Settings, go to Privacy & Security, and scroll down to the
     Security section. You will see a line saying ZangbandTK was blocked.

  3. Click "Open Anyway", and confirm with Touch ID or your password.

macOS remembers the decision. From then on ZangbandTK opens by
double-clicking like anything else, and the message will not come back.

Note for anyone used to older versions of macOS: right-clicking the
application and choosing Open used to work for this, and no longer does.
Apple removed that shortcut. System Settings is now the way.

If you would rather use the Terminal, this does the same thing without needing
the blocked attempt first. Adjust the path if the app is somewhere else:

  xattr -dr com.apple.quarantine /Applications/ZangbandTK.app


Checking the application yourself
---------------------------------

ZangbandTK is signed, just not with a paid identity. You can confirm that
the copy you have is intact and has not been altered since it was built:

  codesign --verify --strict --verbose=2 /Applications/ZangbandTK.app

It should report "valid on disk" and "satisfies its Designated Requirement".
That proves the application has not been tampered with. It cannot prove who
built it -- which is exactly what a Developer ID would add, and exactly what
macOS is complaining about.


What else is on this disk image
-------------------------------

  ZangbandTK.app      The game.
  Docs/               The manual, in HTML. Open Docs/index.html in a browser.
                      The game's Help menu has the same manual inside it.
  borg.txt            Documentation for the borg, the automatic player.
  LICENSE.md          The licence this game is distributed under.
  README.txt          This file.


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
