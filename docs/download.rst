========
Download
========

.. warning::

   **The first release has not been tagged yet.** Until it is, the Releases page
   below will be empty and building from source is the only way to play. The
   instructions for both are here and will not change when it lands.

Getting the game
================

Releases are published on the project's `Releases page`_ as a disk image,
``ZangbandTK-<version>-osx.dmg``. It contains the application, this manual in
HTML, and the borg's documentation.

.. _Releases page: https://github.com/z88kat/ZangbandTK/releases

Requirements: **macOS on Apple Silicon**. Intel Macs are not supported; they
reach legacy status in September 2026.

Mount the image and drag ``ZangbandTK.app`` wherever you keep applications.

The first launch
----------------

.. important::

   **Do not double-click it the first time.** Right-click (or Control-click) the
   application and choose **Open**, then confirm at the prompt. macOS only asks
   once; after that it opens normally.

The reason is worth stating plainly rather than leaving you to guess whether the
download is broken. Apple's Gatekeeper only lets an application through without
comment if it has been signed with a paid Developer ID certificate and submitted
to Apple for notarization. ZangbandTK is signed, but ad-hoc — the signature is
valid and proves the bundle has not been altered since it was built, but it
carries no registered developer identity, and this project has not bought one.

macOS therefore refuses a plain double-click on a fresh download. Depending on
the version it will say the application is damaged, or that it cannot be opened
because the developer cannot be verified. Neither is what it appears to mean:
nothing is damaged, and the check is about who signed it, not whether it works.
Opening it from the context menu once is the supported way to say so.

If you would rather clear the quarantine flag directly:

.. code-block:: sh

   xattr -dr com.apple.quarantine /Applications/ZangbandTK.app

Verifying it, if you would like to
----------------------------------

.. code-block:: sh

   codesign --verify --strict --verbose=2 /Applications/ZangbandTK.app

That should report ``valid on disk`` and ``satisfies its Designated
Requirement``. This tells you the bundle is internally consistent and unmodified
since it was built. It cannot tell you who built it — that is exactly what a
Developer ID would add.

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


Other platforms
===============

macOS is the delivery target. The code is kept portable and Angband's CI covers
Linux and Windows builds, but **neither is tested here**. If you build on either,
reports are welcome.


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
