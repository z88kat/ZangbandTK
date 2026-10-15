Zangband
========

Rebuilding the spirit of Zangband on a modern Angband.

  https://zangbandtk.com/


Installing
----------

Drag Zangband.app to your Applications folder, or anywhere else you like.

There is a build for each kind of Mac, and this image holds one of them:

  Zangband-<version>-osx.dmg          Apple Silicon (M1 and later)
  Zangband-<version>-osx-intel.dmg    Intel

If you took the wrong one, the other is on the same Releases page. An Intel Mac
cannot open the Apple Silicon build at all; an Apple Silicon Mac can run the
Intel build through Rosetta, but there is no reason to when the native one is
right there.


If you have ZangbandTK.app
--------------------------

Up to version 3.128.0 this application was called ZangbandTK.app. It is the
same game under its own name: ZangbandTK is now the other Mac download, a front
end built with Tk, in ZangbandTK-<version>-osx-tk.dmg. This one never had any
Tk in it.

An old ZangbandTK.app, 3.128.0 or earlier, can be deleted once this one is
installed. Your characters are not inside it. They are in Documents/ZangbandTK
in your home folder, which keeps its name, and Zangband.app finds them there.


The first launch: macOS will refuse, once
-----------------------------------------

The first time you open Zangband, macOS will stop you with a message like:

  "Apple could not verify 'Zangband' is free of malware that may harm
   your Mac or compromise your privacy."

Nothing is wrong with the download, and this is not a virus warning. macOS
is saying it cannot tell WHO made the application -- not that it found
anything wrong with it. Removing that message for good requires a paid Apple
Developer ID and Apple's notarization service, which this project does not
have.

To open it, do this once:

  1. Try to open Zangband normally. You will get the message above.
     Click Done. This step matters: macOS will not offer the choice below
     until you have been blocked at least once.

  2. Open System Settings, go to Privacy & Security, and scroll down to the
     Security section. You will see a line saying Zangband was blocked.

  3. Click "Open Anyway", and confirm with Touch ID or your password.

macOS remembers the decision. From then on Zangband opens by double-clicking
like anything else, and the message will not come back.

Note for anyone used to older versions of macOS: right-clicking the
application and choosing Open used to work for this, and no longer does.
Apple removed that shortcut. System Settings is now the way.

If you would rather use the Terminal, this does the same thing without needing
the blocked attempt first. Adjust the path if the app is somewhere else:

  xattr -dr com.apple.quarantine /Applications/Zangband.app


Checking the application yourself
---------------------------------

Zangband is signed, just not with a paid identity. You can confirm that the
copy you have is intact and has not been altered since it was built:

  codesign --verify --strict --verbose=2 /Applications/Zangband.app

It should report "valid on disk" and "satisfies its Designated Requirement".
That proves the application has not been tampered with. It cannot prove who
built it -- which is exactly what a Developer ID would add, and exactly what
macOS is complaining about.


What else is on this disk image
-------------------------------

  Zangband.app     The game.
  Docs/            The manual, in HTML. Open Docs/index.html in a browser.
  borg.txt         Documentation for the borg, the automatic player.
  LICENSE.md       The licence this game is distributed under.
  README.txt       This file.


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
