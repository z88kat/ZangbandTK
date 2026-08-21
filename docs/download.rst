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
- ``ZangbandTK-<version>.tar.gz`` — the source, with the build system already
  generated.

.. _Releases page: https://github.com/z88kat/ZangbandTK/releases

The first release is **3.1.1**, from 18 August 2026. It was cut before the
Windows builds were packaged, so it holds the disk image and the source archive
only; the zips appear from the release after it. The 32-bit zip was named
``-win.zip`` for one release before the 64-bit build arrived and both were named
for their architecture.

Every release is marked a **pre-release**, and will be for as long as the game is
early. CI builds, signs and verifies each one, but no version is played through
before it is tagged: there are known bugs and unfinished features, and the badge
on the Releases page says so before you download rather than after.

The disk image requires **macOS on Apple Silicon**. Intel Macs are not
supported; they reach legacy status in September 2026. The Windows zip is built
and packaged by CI but nobody plays the game on Windows here, so read `Other
platforms`_ before you rely on it.

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

Nobody plays the game on either here, though, so both are **untested in play**:
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
