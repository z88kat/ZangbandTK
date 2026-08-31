# Building the documentation

The manual is a Sphinx project, and it is the whole of
[zangbandtk.com](https://zangbandtk.com/) rather than just the manual part of it.
`.github/workflows/pages.yaml` builds and publishes it on every push to `master`
that touches `docs/`; to build it locally:

```sh
# once — a dedicated virtualenv, kept out of the way of any other Python you
# have. Use the system Python explicitly: on macOS `python3` often resolves to
# a tool-specific environment (PlatformIO, Homebrew) that you do not want to
# install into.
/usr/bin/python3 -m venv .venv-docs
.venv-docs/bin/pip install -r docs/requirements.txt

# each time
make -C docs html
```

`.venv-docs/` is gitignored, so it survives a `git clean` of tracked files but
not a fresh clone. `docs/Makefile` looks for it and falls back to whatever
`sphinx-build` is on PATH, so `make html` works without activating anything.

`scripts/check-build` builds the manual as its fourth pass, with `-W` so a
warning is a failure, and **stops with the recipe above if the virtualenv is
missing** rather than skipping the pass. That is deliberate: this used to be a
step outside the gate, and the invocation people reached for
(`make -C docs html`) took `sphinx-build` from PATH and failed with
"sphinx-build: command not found" on a machine that had a working Sphinx in the
tree. That message reads as "Sphinx is not installed", and was believed once,
in a session that then reported the manual as building when it had not been
built at all. A pass that no-ops when a tool is absent reproduces exactly that,
so this one does not.

A clean build emits no warnings. The only two you should see come from urllib3
complaining about the LibreSSL that ships with macOS, and are unrelated to the
documentation -- they are Python warnings printed at import rather than Sphinx
warnings, which is why `-W` does not trip on them.
