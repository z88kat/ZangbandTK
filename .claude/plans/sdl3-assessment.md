# SDL 3 — assessment, and why we are not doing it

**Status:** closed · **Decision:** [DEC-65](decisions.md) · **Follow-up:** [§6 of the Phase 2
plan](phase2-development-plan.md) · **Assessed:** 3 September 2026

Shareable write-up: <https://claude.ai/code/artifact/804e2e32-2a16-4e09-984f-ea4d1439b5d9>

Kept here so the question does not have to be re-investigated. If it comes up again, read
§7 first — it is the only part that can have changed.

---

## 1. The answer

**The SDL 2 front end stays on SDL 2.** The port is affordable in isolation. What it costs is
fifteen thousand lines of upstream maintenance we currently get for free, and the browser
build, which has nowhere to land.

---

## 2. What the port would touch

Counted, not estimated.

| | Lines |
|---|---:|
| [`src/main-sdl2.c`](../../src/main-sdl2.c) | 8,502 |
| [`src/sdl2/`](../../src/sdl2/) — the `pui` widget toolkit | 7,094 |
| [`src/snd-sdl.c`](../../src/snd-sdl.c) | 293 |
| **Total** | **15,889** |

228 distinct SDL symbols. **731 occurrences of symbols SDL 3 renames or removes** — a floor,
because it excludes every `SDL_Rect` → `SDL_FRect` conversion the float renderer forces and
every `event.key.keysym.sym` that becomes `event.key.key`.

Reproduce the count by building a list of renamed/removed symbols from
`docs/README-migration.md` in the SDL checkout and running `grep -howFf` over the four
sources above.

---

## 3. The decisive argument — DEC-11, not effort

`pui-ctrl.c`, `pui-dlg.c` and `pui-misc.c` are **byte-identical** to upstream Angband.
`main-sdl2.c` differs by **677 lines**, essentially the Emscripten work and nothing else. So
about 4% of that 15,889 is ours.

Upstream is still working on those files. The two most recent commits touching them are
*SDL2: check for allocation failures from `SDL_strdup()`* and *SDL2: add missing checks for
memory allocation failures*.

[DEC-11](decisions.md) gave up merge compatibility and deliberately kept **cherry-pick**
compatibility, on the argument that cherry-picking needs only that a file "still exists and
is recognisably related", and that what it buys is "crashes, leaks, portability, undefined
behaviour". Those two commits are that list verbatim.

An SDL 3 port does not bend DEC-11. It spends the one thing DEC-11 chose to keep — and
spends it on the subsystem where we have contributed least and would gain least by owning.

---

## 4. Blast radius by shipping target

| Target | Front end | Under SDL 3 | Cost |
|---|---|---|---|
| macOS app | Cocoa | No SDL in `Makefile.osx` | none |
| Windows release | native Win32 | Release job installs no SDL | none |
| DOS | custom | unaffected | none |
| Nintendo DS / 3DS | custom | unaffected | none |
| GCU / X11 | curses, Xlib | unaffected | none |
| MSYS2 CI | SDL 2 | all four packaged as `sdl3*` (lowercase rename) | rename |
| Linux AppImage | SDL 2 | built on ubuntu-22.04 for glibc; no `libsdl3-dev` there or in 24.04 | build from source |
| **WebAssembly** | SDL 2 via Emscripten ports | **no `sdl3_image` port exists** | **blocked** |

### The browser build in detail

Emscripten ships exactly two SDL 3 ports: `sdl3.py` and `sdl3_ttf.py`. There is no
`sdl3_image.py`. Our tilesets go through `IMG_Load`, so the browser build would need
SDL3_image compiled from source under `emcmake` and linked by hand, or it loses tiles.

Separately: Emscripten's SDL 3 port arrived in **5.0.0** (January 2026) and `sdl3_ttf` in
**5.0.3** (March 2026). [`Makefile.wasm`](../../src/Makefile.wasm) and
[`wasm.yaml`](../../.github/workflows/wasm.yaml) pin **3.1.51**. That is a four-major-version
toolchain jump underneath a build whose existence depends on Asyncify continuing to behave.
Emscripten's port also pins SDL **3.4.2**, not the current 3.4.16.

---

## 5. Work that is not a rename pass

| Change | Why it is not mechanical |
|---|---|
| `SDL_Rect` → `SDL_FRect` | SDL 3's renderer is float-based. 88 uses, several in the toolkit's public headers, so it ripples through the widget API. |
| `SDL_RendererInfo` removed | `choose_pixelformat()` ([main-sdl2.c:6490](../../src/main-sdl2.c#L6490)) walks `info->texture_formats`; becomes a properties lookup. |
| `SDL_CreateRenderer` flags gone | We persist `renderer_flags` in the player's config file. The `hardware`/`software` keys survive; the plumbing under them does not. |
| `SDL_WINDOWEVENT` split | Distinct event types now. 22 sites. |
| Return conventions inverted | 0-on-success becomes `bool`. `if (SDL_Foo())` silently flips meaning rather than failing to compile — the dangerous one. |
| SDL_mixer 3 | Ground-up redesign, new `MIX_` API. `snd-sdl.c` is a rewrite. Only affects Linux: the browser has `SOUND` off, Windows uses its own backend. |

Satellite-library renames are mechanical but numerous: `TTF_SizeUTF8` → `TTF_GetStringSize`,
`TTF_GlyphMetrics` → `TTF_GetGlyphMetrics`, `TTF_FontHeight` → `TTF_GetFontHeight`,
`TTF_FontFaceIsFixedWidth` → `TTF_FontIsFixedWidth`, `IMG_Init`/`IMG_Quit` removed outright,
`*_GetError` → `SDL_GetError`.

---

## 6. What SDL 3 would give us

Better HiDPI, better Wayland, a cleaner audio API, main callbacks. For a 2D tile-and-text
renderer, nothing that is currently broken for us.

That is the case for the other side, stated fairly. It does not reach the price.

---

## 7. Conditions to revisit

Only these three change the answer. Everything above is stable.

1. **Upstream Angband ports its own SDL front end.** The cost inverts — staying becomes the
   divergence, and the 15,212 upstream lines start working against us. This is the one most
   likely to happen.
2. **Emscripten ships an `sdl3_image` port.** Removes the hard block on the browser build.
   Check `tools/ports/` in the emscripten tree.
3. **An SDL 2 bug we cannot work around.** None today.

If anyone experiments in the meantime, pin **`release-3.4.16`** — not `main`, which is
3.5.0-dev and not what "3.4" means. Homebrew has `sdl3` 3.4.14; MSYS2 has all four
(`sdl3` 3.4.14, `sdl3-ttf` 3.2.2, `sdl3-image` 3.4.4, `sdl3-mixer` 3.2.4).

---

## 8. What the investigation actually found

[`main-sdl.c`](../../src/main-sdl.c) is 6,169 lines of SDL **1.2**, built by CI and shipped
by nothing.

Debian and Ubuntu have replaced `libsdl1.2-dev` with **sdl12-compat**, a shim implementing
the SDL 1.2 API by dlopening SDL 2; Fedora and Arch did the same earlier. So the job in
`linux.yaml` labelled *SDL* has not tested SDL 1.2 for some time — it tests SDL 2, through a
translation layer, in a front end nobody runs, against a second copy of the sound backend,
and it goes green.

Our entire local investment in that file is one hunk: the About box, changed to print every
credit line rather than the first. So "bring SDL 1.2 up to SDL 2" is almost entirely
deletion, because the SDL 2 front end it would be brought up to already exists and is better.

Scheduled as [§6 of the Phase 2 development plan](phase2-development-plan.md).
