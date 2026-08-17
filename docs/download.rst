========
Download
========

.. warning::

   **There are no binary releases yet.** The game is playable but early, and no
   version has been tagged. To play it now, build it from source — it is two
   commands and takes about a minute.

Building it
===========

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
