========
Download
========

Getting the game
================

Releases are published on the project's `Releases page`_. A release carries a
build for each platform, and the source:

- ``ZangbandTK-<version>-osx.dmg`` — the macOS disk image: the application, this
  manual in HTML, and the borg's documentation.
- ``ZangbandTK-<version>-win64.zip`` — the Windows build, 64-bit, with the same
  manual beside it. A single executable: libpng and zlib are linked in, so there
  are no DLLs to keep track of. **Prefer this one.**
- ``ZangbandTK-<version>-win32.zip`` — a 32-bit Windows build, for older machines
  and older Windows. It runs on 64-bit Windows too, and needs the two DLLs beside
  the executable.
- ``ZangbandTK-<version>-linux64.AppImage`` — the Linux build, 64-bit. One file:
  make it executable and run it. Carries three front ends, chosen with ``-m``:
  ``-msdl2`` for tiles, ``-mx11`` for a plain window, and ``-mgcu`` to play in a
  terminal with no display at all.
- ``ZangbandTK-<version>-nintendo.zip`` — the Nintendo DS ROM and the 3DS build,
  with the data the card needs. Text mode, no tiles or sound.
- ``ZangbandTK-<version>.tar.gz`` — the source, with the build system already
  generated.

.. _Releases page: https://github.com/z88kat/ZangbandTK/releases

Take the newest release on that page. The :doc:`release log <releases>` says what
each version changed, and the game is moving quickly enough that the difference
between two of them is worth reading.

Two notes for anyone reaching for an older one. The first release, **3.1.1** from
18 August 2026, was cut before the Windows builds were packaged, so it carries the
disk image and the source archive and nothing else. And for one release after that
the 32-bit zip was named ``-win.zip``, before the 64-bit build arrived and both
were named for their architecture.

Every release is marked a **pre-release**, and will be for as long as the game is
early. CI builds, signs and verifies each one, but no version is played through
before it is tagged: there are known bugs and unfinished features, and the badge
on the Releases page says so before you download rather than after.

The disk image requires **macOS on Apple Silicon**. Intel Macs are not
supported; they reach legacy status in September 2026.

The Windows, Linux and Nintendo builds are built, packaged and smoke-tested by
CI, but the game is developed and played on macOS here and none of the others is
played through before a release is tagged. Read `Other platforms`_ before you rely
on one.

The rest of this page is macOS. Mount the image and drag ``ZangbandTK.app``
wherever you keep applications.

The first launch
----------------

The first time you open ZangbandTK, macOS will refuse, with words to the effect
that it *could not verify that ZangbandTK is free of malware that may harm your
Mac or compromise your privacy*.

That reads as an accusation, and it is not one. macOS is saying it cannot tell
**who** made the application — not that it found anything wrong with it. Apple's
Gatekeeper passes an application without comment only if it was signed with a
paid Developer ID certificate and submitted to Apple for notarization.
ZangbandTK is signed, but ad-hoc: the signature is valid and proves the bundle
has not been altered since it was built, but it carries no registered developer
identity, because this project has not bought one.

.. important::

   **Open it once through System Settings.**

   1. Try to open ZangbandTK normally and let it be refused. Click *Done*. This
      step is required — macOS does not offer the next one until something has
      actually been blocked.
   2. Open **System Settings → Privacy & Security**, and scroll down to
      *Security*. There will be a line saying ZangbandTK was blocked.
   3. Click **Open Anyway** and confirm with Touch ID or your password.

   macOS remembers, and afterwards the application opens by double-clicking like
   any other.

.. note::

   If you have done this on older versions of macOS, you may remember
   right-clicking the application and choosing *Open*. Apple removed that
   shortcut; on current macOS it no longer gets you past this, and System
   Settings is the way.

If you would rather clear the quarantine flag directly, this does the same job
without needing the refused attempt first:

.. code-block:: sh

   xattr -dr com.apple.quarantine /Applications/ZangbandTK.app

Verifying it, if you would like to
----------------------------------

.. code-block:: sh

   codesign --verify --strict --verbose=2 /Applications/ZangbandTK.app

That should report ``valid on disk`` and ``satisfies its Designated
Requirement``. This tells you the bundle is internally consistent and unmodified
since it was built. It cannot tell you who built it — which is exactly what a
Developer ID would add, and exactly what macOS is complaining about.

The disk image carries a ``README.txt`` saying all of this too, for anyone who
downloads it without passing through this page.

Building it from source
=======================

Requirements
------------

.. list-table::
   :widths: 30 70

   * - **macOS on Apple Silicon**
     - Intel Macs are not supported; they reach legacy status in September 2026.
   * - **Xcode command line tools**
     - ``xcode-select --install``
   * - CMake
     - Only to run the test suite. ``brew install cmake``
   * - Python 3.11+
     - Only for the data conversion tools.

The last two are optional. Building and playing the game needs the first two.

The game
--------

.. code-block:: sh

   git clone https://github.com/z88kat/ZangbandTK.git
   cd ZangbandTK/src
   make -f Makefile.osx -j$(sysctl -n hw.activecpu)

That produces ``ZangbandTK.app`` in the repository root. Double-click it, or:

.. code-block:: sh

   open ZangbandTK.app

The tests
---------

.. code-block:: sh

   cmake -S . -B build -DSUPPORT_TEST_FRONTEND=ON
   cmake --build build --parallel
   cd build && make alltests

941 unit tests and 5 integration tests. They should all pass; if they do not,
that is a bug worth reporting.


Before you start
================

.. important::

   **Savefiles are not compatible** with Angband or Zangband, and never will be.
   Do not point ZangbandTK at a savefile you care about.

The game is early. It is playable and it already feels different from Angband,
but a good deal of Zangband is still missing — see :doc:`features` for exactly
what. If you have played Angband before, :doc:`balance` is the shortest account
of what will kill you that would not have before.


Linux
=====

The AppImage needs no installation and nothing installed alongside it:

.. code-block:: sh

   chmod +x ZangbandTK-<version>-linux64.AppImage
   ./ZangbandTK-<version>-linux64.AppImage

It bundles its own SDL2, X11 and ncurses libraries, so it does not care which
distribution it is on, and it is built against an older glibc than the current
one deliberately so that it runs on more than just the newest releases. Older
distributions may need ``libfuse2`` installed for AppImages to mount; failing
that, ``--appimage-extract`` unpacks it into a directory you can run from.

Saves go to ``~/.angband/ZangbandTK``, outside the image, which is read only.
That path is inherited from Angband and kept so that nothing has to move later.

There is no 32-bit Linux build. Ubuntu dropped the i386 archive in 19.10, Fedora
and Arch dropped 32-bit years ago, and the source archive covers anyone still
running it.


Nintendo DS and 3DS
===================

Yes, really. The DS build is inherited from Angband, and it works well enough to
be worth shipping — but it is the least tested build here by a wide margin.

The game reads its data from the card rather than from inside the ROM, so both
halves of the zip matter: copy the ``zangbandtk`` folder to the **root** of the
SD card, so the data sits at ``/zangbandtk/lib/``, and put the ``.nds`` wherever
your flashcart keeps its ROMs. On a 3DS the ``.3dsx`` goes in ``/3ds/`` for the
Homebrew Launcher. A game that starts and then cannot find its files has almost
always got that folder one level too deep.

It is text mode: no tilesets and no sound, which is why it is a small download
where the others are twenty megabytes larger.

**The world is smaller here, deliberately.** A DS has 4 MB of memory and the
desktop world does not fit in it — the game loads, reaches character creation and
then runs out of memory generating the surface. So this build ships a 260×260
world with one town, against 2064×2064 and a dozen elsewhere, and a live area of
about one screen — which means the surface is rebuilt as you walk. All thirteen
dungeons are still placed. The settings live in ``constants.txt`` on the card, so
anyone who wants to try the full-size world can, without rebuilding anything.

.. important::

   **"Unable to access filesystem" is a DLDI problem, not a broken download.**
   Homebrew on a DS needs a driver for the particular card it runs from, written
   into the ROM — DLDI patching. Flashcarts like the R4, and loaders such as
   TWiLight Menu++ or the Homebrew Menu, do this for you as they launch, and a
   DSi or 3DS running from its own SD card does not need it at all. Launching the
   ``.nds`` directly, or in an emulator, generally does: patch it with
   ``dlditool <driver>.dldi ZangbandTK.nds``, using the driver for the card in
   question. The ``README.txt`` in the zip goes through this.

There is **one save slot**, at ``/zangbandtk/lib/save/PLAYER``. That is this
port's limitation, not the game's.

.. note::

   **The DS has 4 MB of memory**, and ZangbandTK asks more of it than Angband
   does: 1013 monsters against Angband's 624 or so. The wilderness is not the
   problem — the whole world costs about 98 KB, because it is generated from a
   seed as you walk rather than stored — but the bestiary is real. If memory runs
   out it will happen while loading, before character creation.

   If you have a RAM expansion pak in Slot-2, this port knows how to use it, and
   that is the first thing to try.

   The card path changed from ``/angband/`` to ``/zangbandtk/`` so that a card
   can hold both games without either finding the other's saves. A card set up
   before that needs its folder renamed.


Other platforms
===============

macOS is what the game is developed and played on. Windows is built and packaged
by CI as well — the mingw cross build, MSBuild, MSYS2 and Cygwin all pass — in
both architectures: the 32-bit build cross-compiled with mingw, the 64-bit one on
a Windows runner under MSYS2, because the bundled PNG and zlib that the 32-bit
build links against are 32-bit binaries with no 64-bit counterpart to hand. Both
zips carry a ``README.txt`` for SmartScreen, which greets an unsigned executable
much as Gatekeeper does above. Linux is packaged the same way, as an
AppImage.

The DS and 3DS builds come from
Angband's own ports and are packaged the same way.

Nobody plays the game on any of them here, though, so all are **untested in
play**:
CI proves they build, start and can read their own data, which is not the same as
having been played through. Reports from either are especially welcome for that
reason.


Licence
=======

ZangbandTK is available under the **Angband licence**:

   This software may be copied and distributed for educational, research, and
   not for profit purposes provided that this copyright and statement are
   included in all such copies. Other copyrights may also apply.

Angband is dual-licensed under the GPL v2 *or* the Angband licence. Zangband was
released under the Angband licence alone, and ZangbandTK incorporates Zangband
material, so the Angband licence is the option available here. In practice that
means **non-commercial distribution** — the same terms Zangband itself carried.

See :doc:`copying` for the full statement, including the exceptions covering
bundled libraries and graphics.


Reporting problems
==================

Bugs, build failures and questions go to `GitHub issues`_.

.. _GitHub issues: https://github.com/z88kat/ZangbandTK/issues
