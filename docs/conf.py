# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# http://www.sphinx-doc.org/en/master/config

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
# Always pick up os, to get os.environ, as used below.
import os
# import sys
# sys.path.insert(0, os.path.abspath('.'))


# -- Project information -----------------------------------------------------

project = "ZangbandTK"
copyright = (
    "2026, ZangbandTK contributors; "
    "Angband developers past and present; "
    "Zangband developers past and present"
)
author = "ZangbandTK contributors"

# The full version, including alpha/beta/rc tags
# There's extra clutter here (and for the HTML theme) to allow conf.py to
# be used as is, to override some settings from the environment (convenient
# for use with autoconf; avoids rewriting conf.py from conf.py.in which breaks
# using conf.py as is), and to allow rewriting for use with CMake.  First
# supply something that can be rewritten.
version = "@DOC_VERSION@"
# If that's not modified or gets a dummy value, get the version number
# from the version.sh script.
if (version == "".join(["@", "DOC_VERSION", "@"]) or version == ''):
    import subprocess
    # Python 3.5 introduces subprocess.run(); use check_output() instead in
    # case the system's Python is older than that.
    version = subprocess.check_output(['../scripts/version.sh'],
            universal_newlines=True)
release = version

# -- General configuration ---------------------------------------------------

# 2.0 changed the default value to 'index'.  Set this manually for backwards
# compatibility with previous versions.
master_doc = 'index'

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
#
# myst_parser lets pages be written in Markdown as well as reStructuredText.
# sphinx_design supplies the grids, cards and buttons used by the non-manual
# pages of the site.
extensions = [
    "myst_parser",
    "sphinx_design",
]

# Add any paths that contain templates here, relative to this directory.
templates_path = ["_templates"]

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
#
# README.md documents how to build these documents; it is not part of them,
# and myst_parser would otherwise treat it as an orphaned source file.
exclude_patterns = ["_build", "README.md", "Thumbs.db", ".DS_Store"]


# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
# Use one of Sphinx's builtin themes if set by the build system.  Otherwise
# use the PyData theme, https://pypi.org/project/pydata-sphinx-theme/ .  Its
# top navigation bar is built from the top-level toctree in index.rst, which
# is what lets the manual sit inside a wider site rather than being all of it.
html_theme = "@DOC_HTML_THEME@"
if (html_theme == "".join(["@", "DOC_HTML_THEME", "@"]) or html_theme == ""):
    if ("DOC_HTML_THEME" in os.environ
            and os.environ["DOC_HTML_THEME"] != ""
            and os.environ["DOC_HTML_THEME"] != "none"):
        html_theme = os.environ["DOC_HTML_THEME"]
    else:
        html_theme = "pydata_sphinx_theme"
        html_theme_options = {
            "logo": {"text": "ZangbandTK"},
            "navbar_align": "left",
            "github_url": "https://github.com/z88kat/ZangbandTK",
            "use_edit_page_button": False,
            "show_prev_next": True,
            "navigation_with_keys": False,
            # The build id is a git describe string rather than a release
            # number, so it is not worth a slot in the navigation bar.
            "show_version_warning_banner": False,
            "footer_start": ["copyright"],
            "footer_end": [],
        }
        html_css_files = ["style.css"]

html_title = "ZangbandTK"
html_short_title = "ZangbandTK"

# Where the built site is served from, used for canonical link elements.  Sphinx
# links between pages relatively, so this only matters to search engines and
# only needs changing if the site moves to a domain of its own; the environment
# override is there so that a build can be pointed elsewhere without an edit.
html_baseurl = os.environ.get(
    "DOC_BASE_URL", "https://z88kat.github.io/ZangbandTK/")

# The home page carries the site's own navigation and reads as a landing page,
# so it gets no sidebar.  Every other page keeps the theme's default sidebar.
html_sidebars = {
    "index": [],
}


# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ["_static"]
