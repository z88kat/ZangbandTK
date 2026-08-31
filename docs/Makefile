# Minimal makefile for Sphinx documentation
#

# You can set these variables from the command line.
SPHINXOPTS    =
SOURCEDIR     = .
BUILDDIR      = _build

# Prefer the project's own virtualenv over whatever is on PATH.
#
# `.venv-docs` is what docs/README.md tells you to make and is gitignored, so
# it is present on a working tree and absent on a fresh clone. Taking
# sphinx-build from PATH instead -- which is what this used to do -- meant
# `make html` failed with "sphinx-build: command not found" on a machine that
# had a perfectly good Sphinx sitting in the tree, because nothing ever put
# the virtualenv on PATH. That error reads as "Sphinx is not installed", and
# it was believed once.
VENV_SPHINX   = $(CURDIR)/../.venv-docs/bin/sphinx-build
SPHINXBUILD   = $(shell test -x "$(VENV_SPHINX)" && echo "$(VENV_SPHINX)" || echo sphinx-build)

# Put it first so that "make" without argument is like "make help".  $(O) here
# and in the other rules below is meant as a shortcut for adding options to
# what's in $(SPHINXOPTS).
help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.
.DEFAULT:
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

# Explicitly handle this one to take care of the empty directory for the
# built documents.
clean:
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O) clean
	@if test -f $(BUILDDIR) || test -d $(BUILDDIR); then rm -fr $(BUILDDIR); fi

.PHONY: help clean
