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
cd docs && ../.venv-docs/bin/python -m sphinx -b html . _build
```

`.venv-docs/` is gitignored.

A clean build emits no warnings. The only two you should see come from urllib3
complaining about the LibreSSL that ships with macOS, and are unrelated to the
documentation.
