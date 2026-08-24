==========================
Frequently Asked Questions
==========================

The best way to get answers to your questions is to post them on the `Angband forum`_.

.. contents:: Contents
   :local:

Issues and problems
-------------------

How do I report a bug?
~~~~~~~~~~~~~~~~~~~~~~

Post on the `Angband forum`_.

Bug reports should include:

* your current operating system (e.g. Windows 10)
* what version the problem appeared in
* the best steps you can figure out to reproduce the bug.

Savefiles that show the problem might be requested, because they help tracking bugs down.

Dark monsters are hard to see
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Fix (reduce) the alpha on your screen, or use the "Interact with colors" screen under the options (``=``) menu.  Navigate to the ``8`` using ``n`` and increase the color intensity with r(ed)/g(reen)/b(lue).

.. _x11-fonts:

How do I avoid the "Couldn't load the requested X11 font (10x20)" message?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The X11 frontend requires *legacy bitmap fonts*. Many modern systems no longer install those fonts by default, or install them in locations that X11 does not search automatically.

Common ways to resolve this include:

* Install legacy X11 bitmap font packages provided by your operating system (package names vary by distribution).
* Explicitly select an installed bitmap font by setting the ``ANGBAND_X11_FONT`` environment variable.
* Use a different frontend (SDL, SDL2, Windows, macOS) that does not rely on X11 bitmap fonts.

On Linux, the required font is typically provided as ``10x20.pcf.gz``. If your distribution allows searching packages by installed files, look for a package that provides that file. Common examples include:

* Debian/Ubuntu and derivatives: ``xfonts-base``
* Red Hat–derived distributions: ``xorg-x11-fonts-misc``
* Arch Linux: ``xorg-fonts-misc``

Depending on the distribution and configuration, installing these packages may not be sufficient if X11 is not configured to search the installed font paths.

If none of the above works, please check the forums for distribution-specific advice or post a question including your OS, distribution, and ZangbandTK version.

Is there a way to disable that thing that pops up when you hit the enter key?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Go into the options menu, choose "Edit keymaps", then "Create a keymap".  Press Enter at the "Key" prompt and a single space as the "Action".

And then you'll probably want to choose to "Save keymaps to a file", and either choose the file name so that it is one automatically loaded when a character is loaded or combine the contents of the saved file with one of the automatically loaded preference files. That allows the change to stay in effect the next time you load the game.

This just replaces the default action of Enter with a "do nothing but don't tell me about help" action. If you want to keep the menu available, say on the 'Tab' key, you can also remap the Tab keypress to the ``\n`` action.


Development
-----------

Where does development happen?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

On GitHub, at https://github.com/z88kat/ZangbandTK — issues, discussion and
pull requests all in the one place.

.. note::

   This section used to answer questions about contributing to *Angband*: post
   on their forums, their developers will file it, and a patch that does not
   suit them may mean *"you may just be better off writing a variant"*. All of
   which is true, and none of which is about this game — it was pointing anyone
   who wanted to help at the wrong project. Writing a variant is advice this
   project has already taken.


.. _Angband forum: https://angband.live/forums/
