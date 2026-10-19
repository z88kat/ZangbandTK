========
Download
========

.. tip::

   If you only want to try the game, you don't need to download anything:
   :doc:`play it in the browser <play>`. Come back here for a build that saves
   to a file you own.

Getting the game
================

Releases are published on the project's `Releases page`_. Each release has a
build for every platform, and the source:

- ``Zangband-<version>-osx.dmg``: the macOS disk image for Apple Silicon, with
  ``Zangband.app``, this manual in HTML, and the borg's documentation.
- ``Zangband-<version>-osx-intel.dmg``: the same disk image for an Intel Mac.
  Take this one if your Mac has an Intel processor; see `Which Mac build do I
  want?`_ if you aren't sure.
- ``Zangband-<version>-osx-terminal.tar.gz``: the same game for macOS drawn
  with characters, to play in Terminal, iTerm2 or over ssh. It's for Apple
  Silicon, and has no window, tiles or sound. See `Playing in a terminal`_.
- ``Zangband-<version>-osx-terminal-intel.tar.gz``: the terminal build for an
  Intel Mac.
- ``ZangbandTK-<version>-osx-tk.dmg``: ``ZangbandTK.app``, the same game
  through a second front end modelled on the 1990s ZangbandTk, with the manual
  and the borg's documentation. Apple Silicon only. See `Two Mac
  applications`_.
- ``Zangband-<version>-win64.zip``: the Windows build, 64-bit, with the same
  manual beside it. It's a single executable with libpng and zlib linked in, so
  there are no DLLs to keep track of. This is the one to take.
- ``Zangband-<version>-win32.zip``: a 32-bit Windows build, for older machines
  and older Windows. It runs on 64-bit Windows too, and needs the two DLLs beside
  the executable.
- ``ZangbandTK-<version>-win64-tk.zip``: ``ZangbandTK.exe``, the Tk front end,
  for 64-bit Windows, with Tcl and Tk beside it. See `ZangbandTK on Windows
  and Linux`_.
- ``Zangband-<version>-linux64.AppImage``: the Linux build, 64-bit. It's one
  file: make it executable and run it. It has three front ends, chosen with
  ``-m``: ``-msdl2`` for tiles, ``-mx11`` for a plain window, and ``-mgcu`` to
  play in a terminal with no display at all.
- ``Zangband-<version>-linux64-terminal.tar.gz``: the Linux terminal build. It's
  the curses front end on its own, with no FUSE and no graphics stack, for a
  server you reach over ssh. See `The Linux terminal build`_.
- ``ZangbandTK-<version>-linux64-tk.AppImage``: the Tk front end for 64-bit
  Linux, one file like the other AppImage. See `ZangbandTK on Windows and
  Linux`_.
- ``Zangband-<version>-nintendo.zip``: the Nintendo DS ROM and the 3DS build,
  with the data the card needs. Text mode, no tiles or sound.
- ``Zangband-<version>-dos.zip``: the DOS build, for a 386 or better with a
  DPMI host. Text mode, CP437, and the manual as plain text beside it. It needs
  ``CWSDPMI.EXE``, which isn't in the zip; see `DOS`_.
- ``ZangbandTK-<version>.tar.gz``: the source, with the build system already
  generated. It's named for the project, not the game, because it builds both
  applications.

.. _Releases page: https://github.com/z88kat/ZangbandTK/releases

Take the newest release on that page. The :doc:`release log <releases>` says what
each version changed, and the game moves quickly enough that the difference
between two of them is worth reading.

Older releases are a little different. The first release, 3.1.1 from 18 August
2026, was cut before the Windows builds were packaged, so it has the disk image
and the source archive and nothing else. For one release after that the 32-bit
zip was named ``-win.zip``, before the 64-bit build arrived and both were named
for their architecture. And up to 3.128.0 every file was named
``ZangbandTK-<version>``, and ``-osx.dmg`` held ``ZangbandTK.app``, which is the
application now called ``Zangband.app``. The name ZangbandTK went to the Tk
front end, the one with Tk in it (see `Two Mac applications`_).

Every release so far is marked as a pre-release. CI builds, signs and verifies
each one, but no version has been played through before it was tagged. There
are known bugs and one milestone's worth of unfinished features, and the badge
on the Releases page tells you so before you download.

CI builds, packages and smoke-tests the Windows, Linux, Nintendo and DOS builds,
but the game is developed and played on macOS here, and none of the others is
played through before a release is tagged. Read `Other platforms`_ before you
rely on one.

The rest of this page is macOS. Mount the image and drag ``Zangband.app``, or
``ZangbandTK.app``, wherever you keep applications.

Which Mac build do I want?
--------------------------

Both processors are supported, and each has its own download. There is no
universal file, so an Apple Silicon player doesn't carry a copy of the game they
can never run, and neither does an Intel one.

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Your Mac
     - Take
   * - **Apple Silicon** (M1 and later)
     - ``-osx.dmg``, or ``-osx-terminal.tar.gz``, or ``-osx-tk.dmg`` for the Tk
       front end. Requires macOS 11 or later.
   * - **Intel**
     - ``-osx-intel.dmg``, or ``-osx-terminal-intel.tar.gz``.

If you don't know which you have, open the Apple menu → *About This Mac*. A
line reading *Chip: Apple M-something* is Apple Silicon; *Processor: Intel
something* is Intel.

Taking the wrong one isn't dangerous, just useless. An Intel Mac can't open the
Apple Silicon build at all, and an Apple Silicon Mac will run the Intel build
through Rosetta, more slowly and only if Rosetta is installed. There's no reason
to do that when the native build is on the same page.

.. note::

   Intel support is new: Intel Macs were ruled out of this project at first,
   and support came later. CI compiles, packages and smoke-tests both builds on
   a machine of the matching architecture (the Intel builds are built on an
   Intel runner, not cross-compiled), but the game is developed and played on
   Apple Silicon here, so that's the build with hours on it.

Two Mac applications
--------------------

The Mac disk images hold one of two applications, and they're the same game.
``Zangband.app``, in ``-osx.dmg`` and ``-osx-intel.dmg``, is the Mac front end
that comes with Angband 4.2, where the game draws everything itself in windows of
text or tiles. ``ZangbandTK.app``, in ``-osx-tk.dmg``, is a front end modelled
on ZangbandTk, the Zangband of the late 1990s: the map and its panes in the main
window, the character, the inventory, the spell books and the shops in windows
of their own, and the original's menus. It's the newer of the two and still
being finished. It runs on Apple Silicon only: there's no Intel Mac build of it,
and there won't be one.

They can be installed side by side, and they share one folder of characters and
scores, ``Documents/ZangbandTK`` in your home folder, so a character made in one
can be carried on in the other. The first launch below is the same for both,
with ``ZangbandTK`` in place of ``Zangband`` for the second.

.. important::

   Up to 3.128.0, Zangband.app was called ZangbandTK.app. It had no Tk in it,
   and the name now belongs to the application that does. An old
   ``ZangbandTK.app`` from 3.128.0 or earlier is ``Zangband.app`` under its old
   name, and you can delete it. Look before you let the Finder replace it with
   the new ``ZangbandTK.app``, because that swaps one application for the other.
   Your characters are safe either way: neither application keeps them inside
   itself, and the folder they live in kept its name for exactly this reason.

The first launch
----------------

The first time you open Zangband, macOS will refuse, with words to the effect
that it *could not verify that Zangband is free of malware that may harm your
Mac or compromise your privacy*.

That sounds like an accusation, but it isn't one. macOS is saying it can't tell
*who* made the application. It hasn't found anything wrong with it. Apple's
Gatekeeper passes an application without comment only if it was signed with a
paid Developer ID certificate and submitted to Apple for notarization.
Zangband is signed, but ad-hoc. The signature is valid and proves the bundle
has not been altered since it was built, but it has no registered developer
identity, because this project hasn't bought one.

.. important::

   Open it once through System Settings:

   1. Try to open Zangband normally and let it be refused. Click *Done*. You
      can't skip this step, because macOS doesn't offer the next one until
      something has been blocked.
   2. Open **System Settings → Privacy & Security**, and scroll down to
      *Security*. There will be a line saying Zangband was blocked.
   3. Click **Open Anyway** and confirm with Touch ID or your password.

   macOS remembers, and afterwards the application opens by double-clicking like
   any other.

.. note::

   If you've done this on older versions of macOS, you may remember
   right-clicking the application and choosing *Open*. Apple removed that
   shortcut. On current macOS it doesn't get you past this, and System Settings
   is the way.

If you'd rather clear the quarantine flag directly, this does the same job
without needing the refused attempt first:

.. code-block:: sh

   xattr -dr com.apple.quarantine /Applications/Zangband.app

Verifying it, if you would like to
----------------------------------

.. code-block:: sh

   codesign --verify --strict --verbose=2 /Applications/Zangband.app

That should report ``valid on disk`` and ``satisfies its Designated
Requirement``. This tells you the bundle is internally consistent and unmodified
since it was built. It can't tell you who built it. That's what a Developer ID
would add, and it's what macOS is complaining about.

The disk image has a ``README.txt`` that says all of this too, for anyone who
downloads it without coming through this page.

Playing in a terminal
=====================

``Zangband-<version>-osx-terminal.tar.gz`` is the curses build: the same game,
drawn with characters, inside a terminal. It's a separate download from the disk
image because it is a different executable. There's no application bundle and no
window server involved at all, and that's what lets it run over ssh.

On an Intel Mac the file is ``Zangband-<version>-osx-terminal-intel.tar.gz``;
everything below applies to both, and the unpacked folder is named the same
either way.

.. code-block:: sh

   tar -xzf Zangband-<version>-osx-terminal.tar.gz
   cd Zangband-<version>
   ./zangband

Keep the executable and the ``lib`` directory beside it together, and start the
game from inside the unpacked folder, because that's where it looks for its data.

Quarantine applies here too. The *Open Anyway* route above is for applications,
so use the direct way:

.. code-block:: sh

   xattr -d com.apple.quarantine zangband

If you unpack it with ``tar`` in a terminal instead of double-clicking the
archive, the mark usually isn't set in the first place.

.. important::

   The terminal build does not share savefiles with the applications. Its
   characters live in ``~/.angband/ZangbandTK``, and the ``.app``'s live in
   ``~/Documents/ZangbandTK``. That's because the terminal build uses the Unix
   convention for a user's data and not the macOS one. It means the two can be
   run side by side without treading on each other, but a character started in
   one won't appear in the other.

Nothing is written inside the unpacked folder while you play, so it can be
deleted or replaced with a newer version without taking your characters with it.

80x24 is the minimum size and it's cramped; 100x40 or larger is much better. The
front end can split the window into subwindows (``./zangband -mgcu -n2``
through ``-n6``), and ``./zangband -h`` lists the rest. Your terminal must be
set to UTF-8, which Terminal.app and iTerm2 both are by default.

Building it from source
=======================

Requirements
------------

.. list-table::
   :widths: 30 70

   * - **macOS**, on Apple Silicon or Intel
     - Either builds the game for itself. To build for the *other* one, see
       `Building for the other architecture`_.
   * - **Xcode command line tools**
     - ``xcode-select --install``
   * - CMake
     - Only to run the test suite. ``brew install cmake``
   * - Python 3.11+
     - Only for the data conversion tools.

The last two are optional. You only need the first two to build and play the
game.

The game
--------

.. code-block:: sh

   git clone https://github.com/z88kat/ZangbandTK.git
   cd ZangbandTK/src
   make -f Makefile.osx -j$(sysctl -n hw.activecpu)

That produces ``Zangband.app`` in the repository root. Double-click it, or:

.. code-block:: sh

   open Zangband.app

The terminal build
------------------

.. code-block:: sh

   cd ZangbandTK/src
   make -f Makefile.osx-tty -j$(sysctl -n hw.activecpu)

That produces ``src/osx-tty/zangband``. It links Apple's curses in
``/usr/lib``, not Homebrew's, so that the executable runs on a machine with no
Homebrew on it. Add the ``dist`` target to build the tarball that goes in the
release:

.. code-block:: sh

   make -f Makefile.osx-tty -j$(sysctl -n hw.activecpu) dist

It has its own object directory because the Cocoa build compiles the same
sources with different flags. The two builds don't interfere, and both can be
there at once.

Building for the other architecture
-----------------------------------

Both makefiles build for the machine they are run on. To ask for the other
architecture, set ``ARCHS``:

.. code-block:: sh

   make -f Makefile.osx clean
   make -f Makefile.osx ARCHS=x86_64
   make -f Makefile.osx-tty clean
   make -f Makefile.osx-tty ARCHS=x86_64

``ARCHS=arm64`` is the default, and it's what you get if you don't say anything.
Don't skip the ``clean``: object files keep the architecture they were compiled
for, and ``make`` will happily link a mixture and fail at the last step with a
list of undefined symbols that says nothing about why.

Clang cross-compiles between the two without any extra toolchain, so this works
from either kind of Mac. It can't *run* the result, though. An Apple Silicon
machine needs Rosetta to start an Intel build, and an Intel machine can't start
an arm64 one at all. So CI builds each architecture on a runner that matches it,
and the smoke test runs what it built.

The released file names follow ``ARCHS`` by themselves: an ``x86_64`` build
writes ``Zangband-<version>-osx-intel.dmg`` and
``Zangband-<version>-osx-terminal-intel.tar.gz``, so if you build both one after
the other in the same tree, the second doesn't overwrite the first.

.. note::

   Universal binaries are not built. ``ARCHS="x86_64 arm64"`` is what vanilla
   Angband does, and the makefiles would still accept it, but the project ships
   two thin downloads instead (see DEC-22 in the decision log).

The tests
---------

.. code-block:: sh

   cmake -S . -B build -DSUPPORT_TEST_FRONTEND=ON
   cmake --build build --parallel
   cd build && make alltests

There are 941 unit tests and 5 integration tests. They should all pass, and if
one doesn't, that's a bug, so please report it.


Before you start
================

.. important::

   Savefiles are not compatible with Angband or the original Zangband, and
   never will be.
   Don't point the game at a savefile you care about.

Your character survives an upgrade. A ZangbandTK savefile is written in blocks,
each with its own version, and the loader keeps a reader for every version it
has ever written, so a character saved by an older build loads into a newer one.
Things that were renamed are mapped across, not lost. On every change the test
suite loads a shipped corpus of characters, played and saved by hand all through
development, so that a break is noticed before a release.

If something was removed outright, the character still loads, just without that
item. The game only refuses a savefile when loading it would mean making
something up: a caster whose spell list has moved underneath them is turned away
instead of being handed somebody else's spells. Content can be dropped, but a
character's identity is never invented. Four characters out of thirty-five in
the corpus are refused on that rule, and each is recorded with its reason.

Everything Zangband is known for is in the game: the wilderness, the towns, the
dungeons, the bestiary, mutations, chaos patrons, the seven realms, pets and
nightmare mode. :doc:`features` lists what's in and what isn't. If you've played
Angband before, :doc:`balance` is the shortest account of what will kill you
that wouldn't have before.


ZangbandTK on Windows and Linux
===============================

``ZangbandTK``, the front end modelled on the 1990s ZangbandTk (see `Two Mac
applications`_), has a download for Windows and one for Linux as well as the
Mac's. It's the same game, built from the same Tcl and Tk.

On Windows, unzip ``ZangbandTK-<version>-win64-tk.zip`` and keep the folder
together. ``ZangbandTK.exe`` needs the DLLs beside it, and finds its data in the
``lib`` and ``tcltk`` folders next to it. It keeps characters in
``lib\user\save``, as ``Zangband.exe`` does, so a character can be carried from
one to the other by copying its savefile. SmartScreen will stop the first
launch, as it does for ``Zangband.exe``, and the ``README.txt`` in the zip says
how to get past it.

On Linux it's one file, and it needs nothing installed but an X11 desktop, or
Wayland through XWayland:

.. code-block:: sh

   chmod +x ZangbandTK-<version>-linux64-tk.AppImage
   ./ZangbandTK-<version>-linux64-tk.AppImage

Characters go to ``~/.angband/ZangbandTK``, which the other Linux downloads use
too, so all three share them.

.. note::

   On Windows and Linux, sound and music come through SDL instead of the Mac's
   own audio, and the music player plays MP3, FLAC, Ogg, Opus, WAV and AIFF but
   not AAC. The Keymaps window draws a PC keyboard, with your layout's keycaps
   and a PC's keypad. CI builds both on every push, plays a scripted session
   through the front end itself, and starts the packaged game to check it runs
   on what it carries, but nobody has played either through yet.

Linux
=====

The AppImage needs no installation and nothing installed alongside it:

.. code-block:: sh

   chmod +x Zangband-<version>-linux64.AppImage
   ./Zangband-<version>-linux64.AppImage

It bundles its own SDL2, X11 and ncurses libraries, so it doesn't care which
distribution it's on. It is built against an older glibc than the current one, so
that it runs on more than just the newest releases. Older distributions may need
``libfuse2`` installed for AppImages to mount; failing that,
``--appimage-extract`` unpacks it into a directory you can run from.

Saves go to ``~/.angband/ZangbandTK``, outside the image, which is read only.
The path comes from Angband and stays as it is so that nothing has to move
later. That's also why it still has the project's name: ZangbandTK was the
game's name too, up to 3.128.0.

There's no 32-bit Linux build. Ubuntu dropped the i386 archive in 19.10, Fedora
and Arch dropped 32-bit years ago, and the source archive covers anyone still
running it.

The Linux terminal build
------------------------

The AppImage above already has the curses front end (run it with ``-mgcu``), so
on a desktop there's nothing else to fetch. ``-linux64-terminal.tar.gz`` is for
the machines where the AppImage is the wrong shape: FUSE is a package to install
before it will mount at all, and the SDL2 and X11 libraries it bundles are most
of its size and no use without a display.

.. code-block:: sh

   tar -xzf Zangband-<version>-linux64-terminal.tar.gz
   cd Zangband-<version>
   ./zangband

It's the executable, its data, and nothing else. Keep the ``lib`` directory
beside the executable and start the game from inside the unpacked folder,
because that's where it looks for its data.

It needs x86-64 and glibc 2.35 or newer: Ubuntu 22.04, Debian 12, RHEL 9 and
anything after those. Nothing needs to be installed alongside it. ncurses is
linked in statically, because a dynamically linked build does not start at all
on a system with no ``libncursesw.so.6`` (a stock Debian container is one), and
on the RPM distributions it starts but prints ``no version information
available`` on every launch, which looks like a fault and isn't one. The
terminfo database is still read from the system at run time, which is how the
game learns what your terminal can do.

Saves go to ``~/.angband/ZangbandTK``, the same place the AppImage keeps them,
so the two share characters and nothing is written inside the unpacked folder.

The terminal size and subwindow notes under `Playing in a terminal`_ apply here
too, and so does the requirement that the terminal be UTF-8. Over ssh, that
depends on the locale your session picks up at both ends.


Nintendo DS and 3DS
===================

Yes, really. The DS build is inherited from Angband, and it works well enough to
ship, but it's the least tested build here by a wide margin.

The game reads its data from the card, not from inside the ROM, so you need both
halves of the zip. Copy the ``zangbandtk`` folder to the *root* of the SD card,
so the data sits at ``/zangbandtk/lib/``, and put the ``.nds`` wherever your
flashcart keeps its ROMs. On a 3DS the ``.3dsx`` goes in ``/3ds/`` for the
Homebrew Launcher. A game that starts and then can't find its files has almost
always got that folder one level too deep.

It's text mode, with no tilesets and no sound, which is why it's a small
download where the others are twenty megabytes larger.

The world is smaller here. A DS has 4 MB of memory and the desktop world does not
fit in it: the game loads, reaches character creation and then runs out of
memory generating the surface. So this build ships a 260×260 world with one
town, against 2064×2064 and a dozen elsewhere, and a live area of about one
screen, which means the surface is rebuilt as you walk. All thirteen dungeons
are still placed. The settings live in ``constants.txt`` on the card, so anyone
who wants to try the full-size world can, without rebuilding anything.

.. important::

   "Unable to access filesystem" means a DLDI problem, not a broken download.
   Homebrew on a DS needs a driver for the particular card it runs from, written
   into the ROM, and that's what DLDI patching is. Flashcarts like the R4, and
   loaders such as TWiLight Menu++ or the Homebrew Menu, do this for you as they
   launch, and a DSi or 3DS running from its own SD card doesn't need it at all.
   Launching the ``.nds`` directly, or in an emulator, generally does need it:
   patch it with ``dlditool <driver>.dldi Zangband-<version>.nds``, using the
   driver for the card in question. The ``README.txt`` in the zip goes through
   this.

There's one save slot, at ``/zangbandtk/lib/save/PLAYER``. That's a limit of
this port, not of the game.

.. note::

   The DS has 4 MB of memory, and the game asks more of it than Angband does:
   1013 monsters against Angband's 624 or so. The wilderness isn't the problem,
   since the whole world costs about 98 KB (it's generated from a seed as you
   walk, not stored). The bestiary is. If memory runs out, it'll happen while
   loading, before character creation.

   If you have a RAM expansion pak in Slot-2, this port knows how to use it, and
   that's the first thing to try.

   The card path changed from ``/angband/`` to ``/zangbandtk/`` so that a card
   can hold both games without either finding the other's saves. A card set up
   before that needs its folder renamed.


DOS
===

This is a 32-bit protected-mode build, cross-compiled with DJGPP. It wants a 386
or better, and it will run under DOSBox, DOSBox-X, or on the real thing.

.. important::

   The zip does not contain a DPMI host, and the game will not start without one.
   A DJGPP program runs in protected mode and needs something to put the
   processor there. Under Windows, or on a DOS with a DPMI server already
   loaded, there's nothing to do. Otherwise fetch ``CWSDPMI.EXE`` (a separate
   package) from `sandmann.dotster.com <https://sandmann.dotster.com/cwsdpmi/>`_
   and run it once before the game, or leave it in the same directory and it
   will be found. DJGPP programs have always been distributed this way, so
   nothing is missing from the build.

Unpack the zip and the game is in a folder called ``zangband``, and the
executable is ``ZANGBAND.EXE``, which is exactly the eight characters a DOS
filename allows. Up to 3.128.0 both were ``angband``, after the internal project
name that still gives the game its data paths on every other platform.

Those eight characters are also why the data files are not named quite as they
are elsewhere. Anything whose name is too long for DOS is shortened when the
archive is built (``monster_spell.txt`` becomes ``monster2.txt``, and the
Zangband bestiary becomes ``monster3.txt``), and the build asks for the short
names to match. Nothing is left out. The manual's own text files keep their long
names, since nothing but a reader ever opens them, so on a filesystem without
long filename support expect those to arrive truncated.

Text is CP437, converted when the archive is built, so the box-drawing and the
accented letters are the ones the code page has.

CI builds this on every push and then plays it: a character is created under
DOSBox-X and saved, which proves the build starts, finds its data files and can
write to its save directory. Nobody has played it through, and it hasn't been
tried on real hardware. If you do either, a report would be very welcome.


Other platforms
===============

The game is developed and played on macOS. CI builds and packages Windows as
well, in both architectures, and the mingw cross build, MSBuild, MSYS2 and
Cygwin all pass. The 32-bit build is cross-compiled with mingw and the 64-bit
one is built on a Windows runner under MSYS2, because the bundled PNG and zlib
that the 32-bit build links against are 32-bit binaries with no 64-bit
counterpart to hand. Both zips have a ``README.txt`` for SmartScreen, which
greets an unsigned executable much as Gatekeeper does above. Linux is packaged
the same way, as an AppImage.

The DS and 3DS builds come from Angband's own ports and are packaged the same
way. The DOS build is a DJGPP cross build, smoke-tested under DOSBox-X on every
push.

ZangbandTK's Windows and Linux downloads are built and checked the same way;
`ZangbandTK on Windows and Linux`_ says what they lack.

Nobody here plays the game on any of them, though, so they're all untested in
play. CI proves they build, start and can read their own data, which is not the
same as playing one through. Reports from any of them are especially welcome
for that reason.


Licence
=======

ZangbandTK is available under the **Angband licence**:

   This software may be copied and distributed for educational, research, and
   not for profit purposes provided that this copyright and statement are
   included in all such copies. Other copyrights may also apply.

Angband is dual-licensed under the GPL v2 *or* the Angband licence. Zangband was
released under the Angband licence alone, and ZangbandTK includes Zangband
material, so the Angband licence is the one available here. In practice that
means non-commercial distribution, the same terms Zangband itself had.

See :doc:`copying` for the full statement, including the exceptions covering
bundled libraries and graphics.


Reporting problems
==================

Bugs, build failures and questions go to `GitHub issues`_.

.. _GitHub issues: https://github.com/z88kat/ZangbandTK/issues
