Zangband for macOS -- terminal build
====================================

This is the curses build of Zangband.  It draws with characters inside
Terminal.app, iTerm2, or any terminal you can reach over ssh.  There is no
window, no tileset and no sound.  If you wanted the windowed game with tiles,
download the .dmg instead.

There is a build for each kind of Mac, and this tarball is one of them:

    Zangband-<version>-osx-terminal.tar.gz          Apple Silicon
    Zangband-<version>-osx-terminal-intel.tar.gz    Intel

The Apple Silicon build requires macOS 11 or later.  If you took the wrong one,
the other is on the same Releases page.


Running it
----------

    cd Zangband-<version>
    ./zangband

The game reads its data from the lib directory next to the executable, so keep
the two together and start the game from inside the unpacked folder.

The first time you run it, macOS will very likely refuse:

    "zangband" cannot be opened because the developer cannot be verified.

That is quarantine, not a fault in the download.  Anything a browser fetches is
marked, and this build is signed ad-hoc rather than with a paid Apple Developer
ID, so there is no identity for macOS to check.  Clear the mark once:

    xattr -d com.apple.quarantine zangband

and it will start from then on.  If you unpacked with `tar` in a terminal
rather than by double-clicking, the mark is usually not there at all and
nothing needs doing.


Your characters
---------------

Savefiles, scores and panic saves go to

    ~/.angband/ZangbandTK

not into the folder you unpacked, so you can delete this folder or replace it
with a newer version without losing anything.  The folder is named for the
project, ZangbandTK, which was this game's name too up to version 3.128.0, and
it kept the name so that no character made before then was left behind.

Note that this is a different place from where the Mac applications keep their
characters (~/Documents/ZangbandTK).  The terminal build does not share
savefiles with them.  That is deliberate, and it means you can run both without
them treading on each other -- but a character started in one will not appear
in the other.


Terminal size
-------------

80x24 is the minimum and is cramped.  The game is much better at 100x40 or
larger.  Use -n2 through -n6 to split the window into extra subwindows:

    ./zangband -mgcu -n2

Other useful options:

    -B    brighter bold characters
    -D    use the terminal's own background colour
    -K    leave the terminal's colour table alone

Run ./zangband -h for the full list.

Your terminal must be set to UTF-8.  Terminal.app and iTerm2 both are by
default; the game refuses to start otherwise.


More
----

Manual:   docs/index.html in this folder
Website:  https://zangbandtk.com/
Source:   https://github.com/z88kat/ZangbandTK

LICENSE.md travels with every copy, as the Angband licence asks.
