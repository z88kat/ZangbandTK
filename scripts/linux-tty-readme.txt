ZangbandTK for Linux -- terminal build
======================================

This is the curses build of ZangbandTK: the game drawn with characters, in a
terminal, over ssh if you like.  No window, no tileset, no sound.

x86-64, and any glibc from 2.35 onwards -- Ubuntu 22.04, Debian 12, RHEL 9 and
anything newer.  Nothing else needs to be installed: ncurses is linked in, so
it does not matter which one your distribution ships, or whether it ships one
at all.


Running it

    tar -xzf ZangbandTK-<version>-linux64-terminal.tar.gz
    cd ZangbandTK-<version>
    ./zangbandtk

The game reads its data from the lib directory next to the executable, so keep
the two together and start the game from inside the unpacked folder.


Which download do I want?

If you are on a desktop and want tiles, take the AppImage instead -- it carries
this same curses front end as well, reachable with -mgcu, plus SDL2 for tiles
and X11 for a plain window.

Take this one if the AppImage is awkward where you are: it is mounted through
FUSE, which Debian 12 and Ubuntu 22.04 and later no longer install by default,
and it carries the whole graphics stack whether or not there is a display.  On
a headless server this is the smaller and simpler thing by a long way.


Your characters

Savefiles, scores and panic saves go to

    ~/.angband/ZangbandTK

not into the folder you unpacked, so it can be deleted or replaced with a newer
version without taking your characters with it.

This is the same place the AppImage keeps them, so the two builds share
characters and you can move between them freely.  (The macOS application is the
exception: it keeps its characters in ~/Documents/Angband and shares them with
nothing.)


Terminal size

80x24 is the minimum and is cramped; 100x40 or more is much better.  The front
end can split the window into subwindows:

    ./zangbandtk -mgcu -n2

up to -n6.  Other useful options:

    -B    brighter bold characters
    -D    use the terminal's own background colour
    -K    leave the terminal's colour table alone

Run ./zangbandtk -h for the full list.

Your terminal must be set to UTF-8; the game refuses to start otherwise.  Over
ssh that depends on the locale your session picks up, so if it will not start,
check that locale says UTF-8 at both ends.


More

Manual:   docs/index.html in this folder
Website:  https://zangbandtk.com/
Source:   https://github.com/z88kat/ZangbandTK

LICENSE.md travels with every copy, as the Angband licence asks.
