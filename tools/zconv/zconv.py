#!/usr/bin/env python3
"""
zconv — convert Zangband 2.7.5 game data onto Angband 4.2's data model.

Implements BAL-01 through BAL-14 from
``.claude/plans/phase1-balance-calibration.md``.  The data files this produces
are a side effect; the review report is the deliverable (BAL-11).

Usage::

    ./zconv.py analyse            # derive scales and report, write nothing
    ./zconv.py monsters           # convert r_info.txt -> monster.txt fragment
    ./zconv.py monsters --write   # ...and write the data file

Copyright (c) 2026 ZangbandTK contributors

This work is free software; you can redistribute it and/or modify it under the
terms of either:

a) the GNU General Public License as published by the Free Software
   Foundation, version 2, or

b) the "Angband licence":
   This software may be copied and distributed for educational, research,
   and not for profit purposes provided that this copyright and statement
   are included in all such copies.  Other copyrights may also apply.
"""

from __future__ import annotations

import argparse
import re
import sys
import tomllib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import aformat  # noqa: E402
import rules  # noqa: E402
import zformat  # noqa: E402
from report import Converted, Report  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]

ZANGBAND = ROOT / "archive" / "zangband" / "lib" / "edit"
ANGBAND281 = ROOT / "archive" / "angband-281" / "lib" / "edit"
GAMEDATA = ROOT / "lib" / "gamedata"
OUTDIR = Path(__file__).resolve().parent / "out"


def load_valid_flags() -> set[str]:
    """4.2's monster race and spell flags, read from the source of truth."""
    flags: set[str] = set()
    for name, pattern in (
        ("list-mon-race-flags.h", r"^RF\(([A-Z0-9_]+)"),
        ("list-mon-spells.h", r"^RSF\(([A-Z0-9_]+)"),
    ):
        path = ROOT / "src" / name
        if not path.exists():
            continue
        rx = re.compile(pattern, re.M)
        flags.update(rx.findall(path.read_text(encoding="utf-8", errors="replace")))
    flags.discard("NONE")
    return flags


def load_bases() -> dict[str, str]:
    """Map display glyph to 4.2 monster_base template name.

    Every 4.2 monster carries ``base:``, which supplies default flags and pain
    messages, so imported entries need one too.  monster_base.txt keys templates
    by glyph and the mapping is 1:1 apart from ``P``, where the generic 'giant'
    is preferred over the Morgoth-specific template.
    """
    path = GAMEDATA / "monster_base.txt"
    if not path.exists():
        return {}

    bases: dict[str, str] = {}
    name: str | None = None
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = line.strip()
        if line.startswith("name:"):
            name = line[5:].strip()
        elif line.startswith("glyph:") and name:
            glyph = line[6:].strip()
            if glyph == "P":
                bases.setdefault(glyph, "giant")
            else:
                bases.setdefault(glyph, name)
    return bases


def load_obj_max_depth() -> int:
    """The deepest row of 4.2's object allocation table, from the game's data.

    `obj-make:max-depth` is the height of the table `alloc_init_objects()`
    builds, and `get_obj_num()` clamps every request to it (obj-make.c:1127).
    An object whose allocation range lies wholly above it can never be chosen
    -- there is no row of the table it appears in -- and nothing reports that,
    because the object parses and the game starts.

    Read here rather than written down so the converter cannot drift from the
    game it is generating for.
    """
    text = (GAMEDATA / "constants.txt").read_text(encoding="utf-8",
                                                  errors="replace")
    match = re.search(r"^obj-make:max-depth:(\d+)", text, re.M)
    return int(match.group(1)) if match else 100


def load_renames() -> dict[str, dict]:
    """Records Zangband renamed rather than added, per file.

    See renames.toml: each of r_info, a_info and e_info needs a different
    notion of identity, and none of them has the (tval, sval) slot that made
    the object renames findable by structure alone.
    """
    path = Path(__file__).resolve().parent / "renames.toml"
    if not path.exists():
        return {}
    with path.open("rb") as handle:
        return tomllib.load(handle)


def load_basemap() -> dict[str, dict]:
    """Glyphs whose 4.2 template lives under a different display character."""
    path = Path(__file__).resolve().parent / "basemap.toml"
    if not path.exists():
        return {}
    with path.open("rb") as handle:
        return tomllib.load(handle).get("map", {})


def load_spellmap() -> dict:
    """Monster spell vocabulary translation."""
    path = Path(__file__).resolve().parent / "spellmap.toml"
    if not path.exists():
        return {"rename": {}, "reject": {}, "defer": {}}
    with path.open("rb") as handle:
        spec = tomllib.load(handle)
    return {"rename": spec.get("rename", {}), "reject": spec.get("reject", {}),
            "defer": spec.get("defer", {})}


def load_spell_vocabulary() -> set[str]:
    """4.2's accepted monster spells, read from the source of truth."""
    path = ROOT / "src" / "list-mon-spells.h"
    if not path.exists():
        return set()
    names = set(re.findall(r"^RSF\(([A-Z0-9_]+)", 
                           path.read_text(encoding="utf-8", errors="replace"),
                           re.M))
    names.discard("NONE")
    names.discard("MAX")
    return names


def load_blowmap() -> dict[str, dict[str, dict]]:
    """Blow method and effect vocabulary translation."""
    path = Path(__file__).resolve().parent / "blowmap.toml"
    if not path.exists():
        return {"method": {}, "effect": {}}
    with path.open("rb") as handle:
        spec = tomllib.load(handle)
    return {"method": spec.get("method", {}), "effect": spec.get("effect", {})}


def load_blow_vocabulary() -> tuple[set[str], set[str]]:
    """4.2's accepted blow methods and effects, read from the data files."""
    def names(filename: str) -> set[str]:
        path = GAMEDATA / filename
        if not path.exists():
            return set()
        return set(re.findall(r"^name:(.+)$",
                              path.read_text(encoding="utf-8", errors="replace"),
                              re.M))

    return names("blow_methods.txt"), names("blow_effects.txt")


def convert_blow(blow: str, blowmap: dict, methods: set[str],
                 effects: set[str]) -> tuple[str | None, list[str]]:
    """Translate one ``METHOD:EFFECT:DAMAGE`` blow into 4.2's vocabulary.

    Returns the converted blow and any notes about what was translated.  4.2
    rejects unknown methods and effects at parse time, so an unmappable blow is
    dropped rather than allowed to break the data file.
    """
    parts = blow.split(":")
    notes: list[str] = []

    method = parts[0].strip() if parts else ""
    if method and method not in methods:
        spec = blowmap["method"].get(method)
        if not spec:
            return None, [f"blow method `{method}` has no 4.2 equivalent and none "
                          "is mapped in blowmap.toml; blow dropped"]
        notes.append(f"blow method `{method}` -> `{spec['to']}`: {spec['note']}")
        parts[0] = spec["to"]

    if len(parts) > 1:
        effect = parts[1].strip()
        if effect and effect not in effects:
            spec = blowmap["effect"].get(effect)
            if not spec:
                return None, [f"blow effect `{effect}` has no 4.2 equivalent and none "
                              "is mapped in blowmap.toml; blow dropped"]
            notes.append(f"blow effect `{effect}` -> `{spec['to']}`: {spec['note']}")
            parts[1] = spec["to"]

    return ":".join(parts), notes


class FlagMap:
    """Disposition of every Zangband flag with no direct 4.2 name (BAL-10)."""

    def __init__(self, spec: dict):
        self.rename: dict[str, str] = spec.get("rename", {})
        self.field: dict[str, dict] = spec.get("field", {})
        self.reject: dict[str, str] = spec.get("reject", {})
        self.manual: dict[str, str] = spec.get("manual", {})
        self.implement: dict[str, dict] = spec.get("implement", {})

        # Deferred groups are keyed by topic; flatten to flag -> group.
        self.defer: dict[str, dict] = {}
        for name, group in spec.get("defer", {}).items():
            for flag in group.get("flags", []):
                self.defer[flag] = {"group": name, **group}

    def disposition(self, flag: str) -> tuple[str, str]:
        """Return (disposition, human-readable reason) for a flag."""
        if flag in self.rename:
            return "rename", f"-> {self.rename[flag]}"
        if flag in self.field:
            entry = self.field[flag]
            return "field", entry.get("note", f"-> {entry['key']}")
        if flag in self.reject:
            return "reject", self.reject[flag]
        if flag in self.defer:
            entry = self.defer[flag]
            return "defer", (f"{entry['milestone']} ({entry['requirement']}): "
                             f"{entry['note']}")
        if flag in self.implement:
            entry = self.implement[flag]
            return "implement", f"{entry['milestone']}: {entry['note']}"
        if flag in self.manual:
            return "manual", self.manual[flag]
        return "unresolved", "no disposition recorded in flagmap.toml"


def load_flagmap() -> FlagMap:
    path = Path(__file__).resolve().parent / "flagmap.toml"
    if not path.exists():
        return FlagMap({})
    with path.open("rb") as handle:
        return FlagMap(tomllib.load(handle))


def load_overrides() -> dict:
    """Hand-tuned values, reapplied on every run (BAL-12)."""
    path = Path(__file__).resolve().parent / "overrides.toml"
    if not path.exists():
        return {}
    with path.open("rb") as handle:
        return tomllib.load(handle)


def load_lethality() -> rules.Lethality:
    """BAL-14 — read the scalars from constants.txt if present, else the BAL-13 defaults."""
    path = GAMEDATA / "constants.txt"
    scalars = {}
    if path.exists():
        text = path.read_text(encoding="utf-8", errors="replace")
        for key, field in (("lethality:hit-points", "hp_scale"),
                           ("lethality:armor-class", "ac_scale")):
            m = re.search(rf"^{re.escape(key)}:([0-9.]+)$", text, re.M)
            if m:
                # constants.txt stores these as percentages; Lethality holds
                # fractions.
                scalars[field] = float(m.group(1)) / 100
    return rules.Lethality(**scalars)


THEME = re.compile(
    r"amber|chaos|cthulhu|nyarlathotep|hastur|shub|azathoth|yog|tsathoggua|"
    r"benedict|corwin|eric|julian|caine|gerard|fiona|bleys|brand|random|"
    r"pattern|trump|unicorn|logrus",
    re.I,
)


def is_themed(mon: zformat.Monster) -> bool:
    """DEC-19 'theme first': does this monster carry Amber, Mythos or Chaos identity?"""
    return bool(THEME.search(mon.name) or THEME.search(mon.desc))


def cmd_analyse(args) -> int:
    """Derive scales from the corpus and report, without writing anything."""
    old = zformat.read_monsters(str(ANGBAND281 / "r_info.txt"))
    zang = zformat.read_monsters(str(ZANGBAND / "r_info.txt"))
    new = aformat.parse(str(GAMEDATA / "monster.txt"))

    old_by_key = {m.key: m for m in old}
    zang_by_key = {m.key: m for m in zang}
    new_by_key = aformat.index_by_key(new)

    print(f"corpus: 2.8.1={len(old)}  zangband={len(zang)}  4.2={len(new)}")
    print(f"shared 2.8.1<->4.2: {len(set(old_by_key) & set(new_by_key))}")
    print(f"zangband-only vs both: "
          f"{len([k for k in zang_by_key if k not in old_by_key and k not in new_by_key])}")
    print()

    mapping = rules.derive_sleep_mapping(old_by_key, new_by_key)
    print("BAL-07 sleepiness mapping (balance-calibration open question 1)")
    print(f"  observations:      {mapping.sample}")
    print(f"  distinct 2.8.1 values mapped: {len(mapping.exact)}")
    print(f"  ambiguous (one 2.8.1 value -> several 4.2 values): {mapping.ambiguous}")
    print(f"  fallback ratio:    "
          f"{mapping.ratio:.4f}" if mapping.ratio else "  fallback ratio:    none")
    print()
    print("  2.8.1 -> 4.2 (first 20):")
    for old_value, new_value in list(mapping.exact.items())[:20]:
        ratio = f"x{new_value / old_value:.2f}" if old_value else "n/a"
        print(f"    {old_value:>4} -> {new_value:>4}   {ratio}")

    lethality = load_lethality()
    print()
    print(f"BAL-13 lethality scalar: {lethality.describe()}")

    curve = rules.Curve.fit(new)
    print(f"BAL-09 4.2 curve fitted over {len(curve.by_depth)} depth bands")
    return 0


def cmd_monsters(args) -> int:
    """Convert Zangband-only monsters onto 4.2's monster.txt model."""
    old = zformat.read_monsters(str(ANGBAND281 / "r_info.txt"))
    zang = zformat.read_monsters(str(ZANGBAND / "r_info.txt"))
    new = aformat.parse(str(GAMEDATA / "monster.txt"))

    old_by_key = {m.key: m for m in old}
    new_by_key = aformat.index_by_key(new)

    valid_flags = load_valid_flags()
    flagmap = load_flagmap()
    bases = load_bases()
    basemap = load_basemap()
    blowmap = load_blowmap()
    methods, effects = load_blow_vocabulary()
    spellmap = load_spellmap()
    valid_spells = load_spell_vocabulary()
    overrides = load_overrides().get("monster", {})
    lethality = load_lethality()
    mapping = rules.derive_sleep_mapping(old_by_key, new_by_key)
    curve_42 = rules.Curve.fit(new)
    curve_z = _zangband_curve(zang)

    renames = load_renames().get("monster", {})
    candidates = [m for m in zang
                  if m.key not in old_by_key and m.key not in new_by_key
                  and m.key not in renames]
    if args.theme_only:
        themed = [m for m in candidates if is_themed(m)]
    else:
        themed = candidates

    report = Report(
        title="zconv — monster conversion review",
        source=str(ZANGBAND / "r_info.txt"),
        target=str(GAMEDATA / "monster.txt"),
        lethality=lethality.describe(),
    )
    for key, spec in sorted(renames.items()):
        report.skipped.append((
            key, "Zangband's name for Angband's '%s', which 4.2 still ships. %s"
            % (spec["to"], spec["evidence"])))

    report.notes.append(
        f"{len(candidates)} monsters exist in Zangband but in neither Angband 2.8.1 "
        f"nor 4.2.6."
    )
    if args.theme_only:
        report.notes.append(
            f"DEC-19 'theme first' filter applied: {len(themed)} of {len(candidates)} "
            f"carry Amber, Mythos or Chaos identity. The remaining "
            f"{len(candidates) - len(themed)} were not converted — rerun without "
            "--theme-only to include them."
        )
    report.notes.append(
        "BAL-13's lethality scalar is applied by the game at load time, not baked "
        "into these values, so the numbers here are pre-scalar and comparable with "
        "4.2's own data."
    )
    if mapping.ambiguous:
        report.notes.append(
            f"BAL-07: {mapping.ambiguous} source sleep values map to more than one "
            "4.2 sleepiness value; the median was taken for those."
        )

    entries: list[aformat.Entry] = []

    for mon in sorted(themed, key=lambda m: m.index):
        item = Converted(name=mon.name, source_index=mon.index)
        entry = aformat.Entry()

        # 4.2 capitalises uniques as proper nouns and lowercases everything
        # else: "Farmer Maggot" but "filthy street urchin". Zangband capitalises
        # the first word of every name, so lowercasing unconditionally would
        # render Corwin and Nyarlathotep as common nouns.
        raw_name = zformat.normalise_name(mon.name)
        is_unique = "UNIQUE" in mon.flags
        entry.set("name", raw_name if is_unique else raw_name.lower())

        depth = mon.level if mon.level is not None else 0

        def record(key: str, value: rules.Value | None) -> None:
            if value is None:
                return
            item.fields[key] = value
            entry.set(key, value.value)

        # 'base' is mandatory in 4.2 — all 624 of its own monsters carry one.
        # Normally inferable from the glyph; basemap.toml covers the glyphs
        # whose 4.2 template lives under a different display character.
        #
        # basemap is consulted *before* the glyph is inferred from, because the
        # interesting cases are the ones where inference does not fail loudly
        # but succeeds wrongly.  Both games draw with 'l' and disagree about
        # what it means -- aquatic here, trees there -- so an inference-first
        # order silently made every shark a tree and never consulted the entry
        # written to prevent exactly that.
        keep_glyph = True
        if mon.symbol and mon.symbol in basemap:
            spec = basemap[mon.symbol]
            keep_glyph = spec.get("keep_glyph", True)
            record("base", rules.Value(spec["base"], "CNT-01", rules.CONVERTED,
                                       f"glyph '{mon.symbol}' mapped to 4.2's "
                                       f"'{spec['base']}' template"))
        elif mon.symbol and mon.symbol in bases:
            record("base", rules.Value(bases[mon.symbol], "CNT-01", rules.CONVERTED,
                                       f"inferred from glyph '{mon.symbol}'"))
        elif mon.symbol:
            record("base", rules.Value("person", "CNT-01", rules.INVENTED,
                                       f"glyph '{mon.symbol}' has no 4.2 monster_base "
                                       "template and none is mapped in basemap.toml"))
        else:
            record("base", rules.Value("person", "CNT-01", rules.INVENTED,
                                       "no glyph in source; placeholder base assigned"))

        if mon.colour:
            item.fields["color"] = rules.Value(mon.colour, "CNT-01", rules.EXACT)
            entry.set("color", mon.colour)
        # Only override the glyph where it differs from the base's default;
        # where 4.2's own symbol is the better choice, inherit it.
        if mon.symbol and keep_glyph:
            item.fields["glyph"] = rules.Value(mon.symbol, "CNT-01", rules.EXACT)
            entry.set("glyph", mon.symbol)

        if mon.speed is not None:
            record("speed", rules.Value(mon.speed, "CNT-01", rules.EXACT,
                                        "Zangband never changed speed from 2.8.1"))

        hp = rules.hit_points(mon.hp_dice)
        if hp is None:
            hp = curve_42.calibrate(depth, "hp", None, None)
        record("hit-points", hp)

        record("armor-class", rules.armour_class(mon.ac))
        record("sleepiness", rules.sleepiness(mon.sleep, mapping))
        if mon.vision is not None:
            record("hearing", rules.Value(mon.vision, "CNT-01", rules.EXACT))
        record("depth", rules.Value(depth, "CNT-01", rules.EXACT))
        if mon.rarity is not None:
            record("rarity", rules.Value(mon.rarity, "CNT-01", rules.EXACT))

        exp = rules.experience(mon.mexp, mon.level)
        if exp is None:
            exp = curve_42.calibrate(depth, "exp",
                                     float(mon.mexp) if mon.mexp else None, curve_z)
        record("experience", exp)

        for blow in mon.blows:
            converted, notes = convert_blow(blow, blowmap, methods, effects)
            for note in notes:
                item.translations.append(note)
            if converted:
                entry.pairs.append(("blow", converted))

        # Spells (BAL-10 applies here too: nothing is dropped silently).
        spells: list[str] = []
        for spell in mon.spells:
            if spell in valid_spells:
                spells.append(spell)
            elif spell in spellmap["rename"]:
                spells.append(spellmap["rename"][spell])
            elif spell in spellmap["reject"]:
                item.flag_dispositions.append(
                    (spell, "reject", spellmap["reject"][spell]))
            elif spell in spellmap["defer"]:
                spec = spellmap["defer"][spell]
                item.flag_dispositions.append((
                    spell, "defer",
                    f"{spec['milestone']} ({spec['requirement']}): {spec['note']}"))
            else:
                item.flag_dispositions.append((
                    spell, "unresolved",
                    "no disposition recorded in spellmap.toml"))

        if spells:
            seen_spells: dict[str, None] = {}
            for spell in spells:
                seen_spells.setdefault(spell, None)
            entry.set("spells", " | ".join(seen_spells))
            # 4.2 states casting frequency as a percentage; Zangband as 1-in-N.
            freq = mon.spell_freq or 10
            entry.set("spell-freq", max(1, round(100 / freq)))
            item.fields["spells"] = rules.Value(
                len(seen_spells), "CNT-01", rules.CONVERTED,
                f"1_IN_{freq} -> spell-freq {max(1, round(100 / freq))}")

        # Flag resolution (BAL-10). Every flag is carried across, renamed,
        # converted to a field, or recorded with the reason it was not.
        mapped: list[str] = []
        for flag in mon.flags:
            if flag in valid_flags:
                mapped.append(flag)
                continue

            disposition, reason = flagmap.disposition(flag)
            if disposition == "rename":
                mapped.append(flagmap.rename[flag])
            elif disposition == "field":
                spec = flagmap.field[flag]
                if spec["key"] == "flag":
                    mapped.append(spec["value"])
                elif spec["key"] == "friends":
                    entry.pairs.append(("friends", spec["value"]))
                else:
                    entry.set(spec["key"], spec["value"])
            else:
                item.flag_dispositions.append((flag, disposition, reason))

        # De-duplicate while preserving order (BAL-12: stable output).
        seen: dict[str, None] = {}
        for flag in mapped:
            seen.setdefault(flag, None)
        if seen:
            entry.set("flags", " | ".join(seen))

        if mon.desc:
            entry.set("desc", mon.desc)

        # BAL-12 — overrides reapplied last so a re-run never discards tuning.
        for key, value in overrides.get(mon.key, {}).items():
            entry.set(key, value)
            item.fields[key] = rules.Value(value, "BAL-12", rules.EXACT,
                                           "hand-tuned in overrides.toml")
            item.overridden.append(key)

        report.items.append(item)
        entries.append(entry)

    OUTDIR.mkdir(exist_ok=True)
    report_path = OUTDIR / "monsters.report.md"
    report_path.write_text(report.render(), encoding="utf-8")
    print(f"report:  {report_path.relative_to(ROOT)}")

    if args.write:
        data_path = OUTDIR / "monster.zangband.txt"
        aformat.write(
            str(data_path), entries,
            preamble=(
                "# monster.zangband.txt — generated by tools/zconv. Do not hand-edit.\n"
                "# Hand-tuned values belong in tools/zconv/overrides.toml (BAL-12).\n"
                f"# Source: {ZANGBAND / 'r_info.txt'}\n"
            ),
        )
        print(f"data:    {data_path.relative_to(ROOT)}")
    else:
        print("data:    not written (pass --write)")

    invented = sum(len(i.invented) for i in report.items)
    unresolved = {f for i in report.items
                  for f, disposition, _ in i.flag_dispositions
                  if disposition == "unresolved"}
    print(f"entries: {len(report.items)}   invented values: {invented}   "
          f"unresolved flags: {len(unresolved)}")
    return 0


def _zangband_curve(monsters: list[zformat.Monster]) -> rules.Curve:
    """Fit Zangband's own depth curve, so BAL-09 can read a monster's relative role."""
    fake: list[aformat.Entry] = []
    for mon in monsters:
        if mon.level is None:
            continue
        entry = aformat.Entry()
        entry.set("name", mon.name)
        entry.set("depth", mon.level)
        if mon.hp_dice:
            count, sides = mon.hp_dice
            entry.set("hit-points", round(count * (sides + 1) / 2))
        if mon.ac is not None:
            entry.set("armor-class", mon.ac)
        if mon.mexp is not None and mon.level:
            entry.set("experience", max(1, round(mon.mexp / mon.level)))
        fake.append(entry)
    return rules.Curve.fit(fake)


def cmd_artifacts(args) -> int:
    """Convert Zangband-only artifacts onto 4.2's artifact model."""
    import artifacts as art

    # Artifacts are read as a list, not a name-keyed dict. Zangband has two
    # called "of Sawall" and two called "of the Dwarves", and keying by name
    # silently dropped one of each -- one of which was a candidate for import.
    old = {zformat.match_key(r.name)
           for r in zformat.parse(str(ANGBAND281 / "a_info.txt"))}
    zang = list(zformat.parse(str(ZANGBAND / "a_info.txt")))
    existing = aformat.parse(str(GAMEDATA / "artifact.txt"))
    existing_keys = {e.key for e in existing}
    collisions = load_renames().get("artifact_collision", {})

    with (Path(__file__).resolve().parent / "objflagmap.toml").open("rb") as fh:
        flagmap = art.ObjFlagMap(tomllib.load(fh))

    kinds = art.build_kind_index(str(ZANGBAND / "k_info.txt"))
    with (Path(__file__).resolve().parent / "basesubs.toml").open("rb") as fh:
        basesubs = tomllib.load(fh).get("substitute", {})
    bases = art.build_base_index(aformat.parse(str(GAMEDATA / "object.txt")))
    overrides = load_overrides().get("artifact", {})

    report = Report(
        title="zconv — artifact conversion review",
        source=str(ZANGBAND / "a_info.txt"),
        target=str(GAMEDATA / "artifact.txt"),
        lethality="not applicable to artifacts",
    )

    candidates = [(zformat.match_key(r.name), r) for r in zang
                  if zformat.match_key(r.name) not in old
                  and zformat.match_key(r.name) not in existing_keys]

    # 4.2 restores an artifact from a savefile by name (load.c:149), taking the
    # first exact match, so it cannot carry two of one name. Where Zangband has
    # two, one is kept and the other is deferred with its reason rather than
    # being lost to whichever the reader happened to overwrite.
    for key, spec in sorted(collisions.items()):
        clash = [r for k, r in candidates if k == key]
        if len(clash) < 2:
            continue
        for rec in clash:
            if rec.index != spec["keep"]:
                report.deferred.append((
                    "%s (index %d)" % (rec.name, rec.index),
                    "a name of its own", spec["why"]))
        candidates = [(k, r) for k, r in candidates
                      if k != key or r.index == spec["keep"]]
    report.notes.append(
        f"{len(candidates)} artifacts exist in Zangband but in neither Angband "
        f"2.8.1 nor 4.2.6.")
    report.notes.append(
        "Zangband applies a single `pval` to every stat and modifier an item "
        "grants, so an artifact giving +4 STR gives +4 to each of its other "
        "modifiers too. That is Zangband's design, not a conversion artefact.")

    entries: list[aformat.Entry] = []

    for key, rec in sorted(candidates, key=lambda kv: kv[1].index):
        item = Converted(name=rec.name, source_index=rec.index)
        entry = aformat.Entry()

        info = (rec.first("I") or "").split(":")
        world = (rec.first("W") or "").split(":")
        power = (rec.first("P") or "").split(":")

        try:
            tval, sval = int(info[0]), int(info[1])
            pval = int(info[2]) if len(info) > 2 else 0
        except (ValueError, IndexError):
            report.skipped.append((rec.name, "unreadable I: line"))
            continue

        # Base object: tval:sval -> Zangband kind name -> 4.2 type and name.
        kind_name = kinds.get((tval, sval))
        kind_key = zformat.match_key(kind_name) if kind_name else ""
        base = bases.get(kind_key)
        substituted = None

        # 4.2 retired several object kinds. Rather than lose the artifact,
        # substitute the nearest surviving base of the same class.
        if not base and kind_key in basesubs:
            spec = basesubs[kind_key]
            base = bases.get(zformat.match_key(spec["to"]))
            if base and spec.get("type"):
                base = (spec["type"], base[1])
            substituted = spec

        if not base:
            report.skipped.append((
                rec.name,
                f"base object tval {tval} sval {sval} "
                f"({kind_name or 'unknown kind'}) has no 4.2 equivalent and no "
                "substitution is recorded in basesubs.toml"))
            continue

        entry.set("name", rec.name)
        entry.set("base-object", f"{base[0]}:{base[1]}")
        if substituted:
            item.fields["base-object"] = rules.Value(
                f"{base[0]}:{base[1]}", "CNT-06", rules.DERIVED,
                f"'{kind_name}' has no 4.2 equivalent; substituted "
                f"'{substituted['to']}' — {substituted['note']}")
        else:
            item.fields["base-object"] = rules.Value(
                f"{base[0]}:{base[1]}", "CNT-06", rules.CONVERTED,
                f"tval {tval} sval {sval} = '{kind_name}'")

        if len(world) >= 4:
            depth, rarity, weight, cost = (int(world[0]), int(world[1]),
                                           int(world[2]), int(world[3]))
            entry.set("level", depth)
            entry.set("weight", weight)
            entry.set("cost", cost)
            # 4.2's alloc is commonness plus a depth range; Zangband's rarity
            # is an inverse frequency, so commonness is its reciprocal.
            commonness = max(1, min(100, round(100 / max(1, rarity))))
            # 4.2's allocation ceiling is 127, and min must never exceed max --
            # an inverted range parses cleanly and silently makes the artifact
            # ungeneratable for the life of the game.
            alloc_max = 127
            alloc_min = min(depth, alloc_max)
            entry.set("alloc", f"{commonness}:{alloc_min} to {alloc_max}")
            item.fields["alloc"] = rules.Value(
                f"{commonness}:{alloc_min} to {alloc_max}", "CNT-06",
                rules.DERIVED,
                f"Zangband rarity {rarity} inverted to commonness"
                + (f"; source depth {depth} clamped to the {alloc_max} ceiling"
                   if depth > alloc_max else ""))

        if len(power) >= 5:
            entry.set("attack", f"{power[1]}:{power[2]}:{power[3]}")
            entry.set("armor", f"{power[0]}:{power[4]}")

        # Flag resolution across 4.2's five destinations.
        flags, values, brands, slays, curses = [], [], [], [], []
        for flag in rec.flags():
            disposition, reason = flagmap.disposition(flag)
            if disposition == "value":
                if flag in flagmap.value_pval:
                    if pval:
                        values.append(f"{flagmap.value_pval[flag]}[{pval}]")
                    else:
                        item.flag_dispositions.append((
                            flag, "manual",
                            "modifier flag on an artifact with no pval; "
                            "Zangband would grant nothing"))
                else:
                    spec = flagmap.value_fixed[flag]
                    values.append(f"{spec['name']}[{spec['level']}]")
            elif disposition == "flag":
                flags.append(flagmap.flag[flag])
            elif disposition == "brand":
                brands.append(flagmap.brand[flag])
            elif disposition == "slay":
                slays.append(flagmap.slay[flag])
            elif disposition == "curse":
                spec = flagmap.curse[flag]
                curses.append(f"{spec['name']}:{spec['power']}")
            else:
                item.flag_dispositions.append((flag, disposition, reason))

        def dedupe(seq):
            seen: dict[str, None] = {}
            for value in seq:
                seen.setdefault(value, None)
            return list(seen)

        if flags:
            entry.set("flags", " | ".join(dedupe(flags)))
        if values:
            entry.set("values", " | ".join(dedupe_values(values)))
        for brand in dedupe(brands):
            entry.pairs.append(("brand", brand))
        for slay in dedupe(slays):
            entry.pairs.append(("slay", slay))
        for curse in dedupe(curses):
            entry.pairs.append(("curse", curse))

        # Activation: Lua script -> 4.2's named vocabulary.
        script = "\n".join(rec.all("L"))
        if script:
            activation, note = art.match_activation(script)
            if activation:
                entry.set("act", activation)
                item.fields["act"] = rules.Value(activation, "CNT-06",
                                                 rules.DERIVED, note)
                timeout = art.match_timeout(script)
                if timeout:
                    entry.set("time", timeout)
            else:
                item.flag_dispositions.append((
                    "ACTIVATE", "manual",
                    "Lua activation with no matching 4.2 activation; the "
                    "artifact is imported without one"))

        entry.set("desc", f"An artifact of Zangband.")

        for field_name, value in overrides.get(key, {}).items():
            entry.set(field_name, value)
            item.overridden.append(field_name)

        report.items.append(item)
        entries.append(entry)

    OUTDIR.mkdir(exist_ok=True)
    report_path = OUTDIR / "artifacts.report.md"
    report_path.write_text(report.render(), encoding="utf-8")
    print(f"report:  {report_path.relative_to(ROOT)}")

    if args.write:
        data_path = OUTDIR / "artifact.zangband.txt"
        aformat.write(
            str(data_path), entries,
            preamble=(
                "# artifact.zangband.txt — generated by tools/zconv. "
                "Do not hand-edit.\n"
                "# Hand-tuned values belong in tools/zconv/overrides.toml "
                "(BAL-12).\n"
                f"# Source: {ZANGBAND / 'a_info.txt'}\n"
            ),
        )
        print(f"data:    {data_path.relative_to(ROOT)}")
    else:
        print("data:    not written (pass --write)")

    print(f"entries: {len(report.items)}   skipped: {len(report.skipped)}   "
          f"unresolved flags: "
          f"{len({f for i in report.items for f, d, _ in i.flag_dispositions if d == 'unresolved'})}")
    return 0


def cmd_egos(args) -> int:
    """Convert Zangband-only ego types onto 4.2's ego_item model."""
    import artifacts as art

    def read(path: str) -> dict[str, zformat.Record]:
        return {zformat.match_key(r.name): r for r in zformat.parse(path)}

    old = read(str(ANGBAND281 / "e_info.txt"))
    zang = read(str(ZANGBAND / "e_info.txt"))
    existing = {e.key for e in aformat.parse(str(GAMEDATA / "ego_item.txt"))}
    renames = load_renames().get("ego", {})

    here = Path(__file__).resolve().parent
    with (here / "objflagmap.toml").open("rb") as fh:
        flagmap = art.ObjFlagMap(tomllib.load(fh))
    with (here / "slotmap.toml").open("rb") as fh:
        slots = {int(k): v for k, v in tomllib.load(fh).get("slot", {}).items()}

    overrides = load_overrides().get("ego", {})

    report = Report(
        title="zconv — ego item conversion review",
        source=str(ZANGBAND / "e_info.txt"),
        target=str(GAMEDATA / "ego_item.txt"),
        lethality="not applicable to ego items",
    )
    report.notes.append(
        "Zangband identifies applicable items by equipment slot; 4.2 lists "
        "object types. One slot therefore expands to several types — a weapon "
        "ego becomes sword, polearm and hafted.")

    entries: list[aformat.Entry] = []

    for key, spec in sorted(renames.items()):
        report.skipped.append((
            key, "Zangband's name for Angband's '%s', which 4.2 still ships. %s"
            % (spec["to"], spec["evidence"])))

    for key, rec in sorted(
            [(k, r) for k, r in zang.items()
             if k not in old and k not in existing and k not in renames],
            key=lambda kv: kv[1].index):
        item = Converted(name=rec.name, source_index=rec.index)
        entry = aformat.Entry()

        extra = (rec.first("X") or "").split(":")
        combat = (rec.first("C") or "").split(":")
        world = (rec.first("W") or "").split(":")

        try:
            slot = int(extra[0])
            rating = int(extra[1]) if len(extra) > 1 else 0
        except (ValueError, IndexError):
            report.skipped.append((rec.name, "unreadable X: line"))
            continue

        spec = slots.get(slot)
        if not spec:
            report.skipped.append((
                rec.name, f"equipment slot {slot} has no 4.2 type mapping"))
            continue

        entry.set("name", rec.name)

        cost = int(world[3]) if len(world) >= 4 else 0
        depth = int(world[0]) if world and world[0].isdigit() else 0
        rarity = int(world[1]) if len(world) > 1 and world[1].isdigit() else 1
        entry.set("info", f"{cost}:{rating}")
        commonness = max(1, min(100, round(100 / max(1, rarity))))
        entry.set("alloc", f"{commonness}:{depth} to 127")

        # 4.2 expresses an ego's combat bonuses as dice; Zangband stores the
        # maximum each may roll to, which is the same thing said differently.
        if len(combat) >= 3:
            def dice(value: str) -> str:
                n = int(value) if value.lstrip("-").isdigit() else 0
                return f"d{n}" if n > 0 else "0"
            entry.set("combat", f"{dice(combat[0])}:{dice(combat[1])}:"
                                f"{dice(combat[2])}")
        pval = 0
        if len(combat) >= 4 and combat[3].lstrip("-").isdigit():
            pval = int(combat[3])

        for kind in spec["types"]:
            entry.pairs.append(("type", kind))
        item.fields["type"] = rules.Value(
            ", ".join(spec["types"]), "CNT-07", rules.CONVERTED,
            f"slot {slot} ({spec['note']})")

        flags, values, brands, slays, curses = [], [], [], [], []
        for flag in rec.flags():
            disposition, reason = flagmap.disposition(flag)
            if disposition == "value":
                if flag in flagmap.value_pval:
                    if pval:
                        values.append(f"{flagmap.value_pval[flag]}[{pval}]")
                    else:
                        item.flag_dispositions.append((
                            flag, "manual",
                            "modifier flag with no pval on the source entry"))
                else:
                    fixed = flagmap.value_fixed[flag]
                    values.append(f"{fixed['name']}[{fixed['level']}]")
            elif disposition == "flag":
                flags.append(flagmap.flag[flag])
            elif disposition == "brand":
                brands.append(flagmap.brand[flag])
            elif disposition == "slay":
                slays.append(flagmap.slay[flag])
            elif disposition == "curse":
                spec_curse = flagmap.curse[flag]
                curses.append(f"{spec_curse['name']}:{spec_curse['power']}")
            else:
                item.flag_dispositions.append((flag, disposition, reason))

        def dedupe(seq):
            seen: dict[str, None] = {}
            for value in seq:
                seen.setdefault(value, None)
            return list(seen)

        if flags:
            entry.set("flags", " | ".join(dedupe(flags)))
        if values:
            entry.set("values", " | ".join(dedupe_values(values)))
        for brand in dedupe(brands):
            entry.pairs.append(("brand", brand))
        for slay in dedupe(slays):
            entry.pairs.append(("slay", slay))
        for curse in dedupe(curses):
            entry.pairs.append(("curse", curse))

        # CNT-16: 4.2 generates random abilities itself, given the kind flags.
        rand = flagmap.rand_ability.get(rec.name)
        if rand:
            existing_flags = entry.get("flags")
            merged = ((existing_flags + " | ") if existing_flags else "") + \
                " | ".join(rand["flags"])
            entry.set("flags", merged)
            item.fields["rand-ability"] = rules.Value(
                " | ".join(rand["flags"]), "CNT-16", rules.CONVERTED,
                rand["note"])

        for field_name, value in overrides.get(key, {}).items():
            entry.set(field_name, value)
            item.overridden.append(field_name)

        report.items.append(item)
        entries.append(entry)

    OUTDIR.mkdir(exist_ok=True)
    report_path = OUTDIR / "egos.report.md"
    report_path.write_text(report.render(), encoding="utf-8")
    print(f"report:  {report_path.relative_to(ROOT)}")

    if args.write:
        data_path = OUTDIR / "ego_item.zangband.txt"
        aformat.write(
            str(data_path), entries,
            preamble=(
                "# ego_item.zangband.txt — generated by tools/zconv. "
                "Do not hand-edit.\n"
                "# Hand-tuned values belong in tools/zconv/overrides.toml "
                "(BAL-12).\n"
                f"# Source: {ZANGBAND / 'e_info.txt'}\n"
            ),
        )
        print(f"data:    {data_path.relative_to(ROOT)}")
    else:
        print("data:    not written (pass --write)")

    print(f"entries: {len(report.items)}   skipped: {len(report.skipped)}")
    return 0



def dedupe_values(values: list[str]) -> list[str]:
    """Collapse a `values:` list to one entry per property, strongest first.

    Zangband can give one item both an immunity and a resistance to the same
    element -- Stormbringer carries IM_FIRE and RES_FIRE together -- and both
    map onto the same 4.2 property at different levels. 4.2 parses the line
    left to right and assigns, so `RES_FIRE[3] | RES_FIRE[1]` ends at 1 and
    the immunity is silently downgraded to a resistance. Keeping the strongest
    is what the source item meant.
    """
    best: dict[str, str] = {}
    for value in values:
        name, _, rest = value.partition("[")
        level = rest.rstrip("]")
        if name in best:
            was = best[name].partition("[")[2].rstrip("]")
            if was.isdigit() and level.isdigit() and int(level) <= int(was):
                continue
        best[name] = value
    return list(best.values())


#: Zangband's TERM_ colour constants as 4.2's colour names.
_TERM_COLOUR = {
    "TERM_DARK": "Dark", "TERM_WHITE": "White", "TERM_SLATE": "Slate",
    "TERM_ORANGE": "Orange", "TERM_RED": "Red", "TERM_GREEN": "Green",
    "TERM_BLUE": "Blue", "TERM_UMBER": "Umber", "TERM_L_DARK": "Light Dark",
    "TERM_L_WHITE": "Light Slate", "TERM_VIOLET": "Violet",
    "TERM_YELLOW": "Yellow", "TERM_L_RED": "Light Red",
    "TERM_L_GREEN": "Light Green", "TERM_L_BLUE": "Light Blue",
    "TERM_L_UMBER": "Light Umber",
}


def zangband_flavours(kind: str) -> list[tuple[str, str]]:
    """Read one of Zangband's flavour tables out of its C source.

    Ring and amulet flavours are the one part of Zangband's object data that
    never reached a data file: they are two parallel C arrays in flavor.c, an
    adjective and a colour at the same index. 4.2 keeps the same information in
    flavor.txt, so the arrays convert directly -- and they have to, because
    importing twenty-four rings and amulets into a game with 39 ring flavours
    for 45 rings makes it quit at startup ("Not enough flavors for tval 21").
    """
    source = (ROOT / "archive" / "zangband" / "src" / "flavor.c").read_text(
        encoding="utf-8", errors="replace")

    def table(name: str) -> list[str]:
        match = re.search(r"%s\[[A-Z_]+\]\s*=\s*\{(.*?)\};" % name, source, re.S)
        if not match:
            return []
        body = re.sub(r"/\*.*?\*/", "", match.group(1), flags=re.S)
        return [t.strip().strip('"') for t in body.split(",") if t.strip()]

    return list(zip(table("%s_adj" % kind), table("%s_col" % kind)))


#: Zangband's generation-time scripts, as 4.2 random expressions.
#
# `L:MAKE:` runs when the object is created, and its commonest job is to scale
# the pval with depth: `object.pval = 1 + m_bonus(object.pval, level)` gives a
# Ring of the Cat between +1 and +4 depending on how deep it was found. 4.2
# writes the same thing as a random expression in the value itself -- `1+M4`
# is a base of 1 plus an m_bonus roll to 4 -- so the scaling survives the
# conversion instead of freezing at the ceiling.
_MAKE_PVAL = [
    (re.compile(r"object\.pval\s*=\s*(\d+)\s*\+\s*m_bonus\(object\.pval,\s*level\)"),
     lambda m, pval: "%s+M%d" % (m.group(1), pval)),
    (re.compile(r"object\.pval\s*=\s*randint1\((\d+)\)\s*\+\s*m_bonus\(object\.pval,\s*level\)"),
     lambda m, pval: "d%sM%d" % (m.group(1), pval)),
]

#: And the same for the armour bonus, which 4.2 also takes as an expression.
_MAKE_TOA = [
    (re.compile(r"object\.to_a\s*=\s*rand_range\((\d+),\s*(\d+)\)\s*\+\s*m_bonus\((\d+),\s*level\)"),
     lambda m: "%s+d%dM%s" % (m.group(1), int(m.group(2)) - int(m.group(1)) + 1,
                              m.group(3))),
    (re.compile(r"object\.to_a\s*=\s*object\.to_a\s*\+\s*randint1\((\d+)\)\s*\+\s*m_bonus\((\d+),\s*level\)"),
     lambda m: "d%sM%s" % (m.group(1), m.group(2))),
]

#: Shared by all ten statues, which are one decision rather than ten.
STATUE_NOTE = (
    "Zangband's statue is a monster rendered in a material: the `#` in the "
    "name is replaced by the monster's name and `pval` holds its race index "
    "(object2.c:3179). 4.2 object names are fixed strings and `struct object` "
    "has no monster reference, so the statue without its monster is a lump of "
    "material with no content in it."
)


def cmd_objects(args) -> int:
    """Convert Zangband-only object kinds onto 4.2's object.txt model (CNT-11)."""
    import artifacts as art

    here = Path(__file__).resolve().parent
    with (here / "objflagmap.toml").open("rb") as fh:
        flagmap = art.ObjFlagMap(tomllib.load(fh))
    with (here / "objmap.toml").open("rb") as fh:
        objmap = tomllib.load(fh)

    ceiling = load_obj_max_depth()
    types = {int(k): v for k, v in objmap.get("type", {}).items()}
    renames = objmap.get("rename", {})
    rejects = objmap.get("reject", {})
    defers = objmap.get("defer", {})
    effects = objmap.get("effect", {})
    messages = objmap.get("message", {})
    overrides = load_overrides().get("object", {})

    def by_slot(path: str) -> dict[tuple[int, int], zformat.Record]:
        """Index kinds by (tval, sval).

        Not by name: Zangband gives 59 of its names to more than one kind --
        two Amulets of Sensing, three Copper coins -- and a name-keyed read
        silently drops 80 of its 553 records.  The slot is the identity.
        """
        out: dict[tuple[int, int], zformat.Record] = {}
        for rec in zformat.parse(path):
            info = (rec.first("I") or "").split(":")
            if len(info) < 2:
                continue
            try:
                slot = (int(info[0]), int(info[1]))
            except ValueError:
                continue
            if rec.name and zformat.match_key(rec.name) != "something":
                out[slot] = rec
        return out

    zang = by_slot(str(ZANGBAND / "k_info.txt"))
    old = by_slot(str(ANGBAND281 / "k_info.txt"))

    # 4.2 identifies a kind by type and name together: it has a Potion of
    # Resistance and an Amulet of Resistance, which are different objects.
    present: set[tuple[str, str]] = set()
    kind_type = ""
    for entry in aformat.parse(str(GAMEDATA / "object.txt")):
        kind_type = entry.get("type") or ""
        present.add((kind_type, entry.key))

    BOOKS = set(range(90, 97))

    report = Report(
        title="zconv — object kind conversion review",
        source=str(ZANGBAND / "k_info.txt"),
        target=str(GAMEDATA / "object.txt"),
        lethality="not applicable to object kinds",
    )
    report.notes.append(
        "Kinds are compared by (tval, sval) rather than by name. Zangband "
        "renamed 26 of the kinds it inherited from Angband 2.8.1, so a "
        "name-based comparison reports a Ring of Skill as new content when "
        "4.2 already ships it as a Ring of Accuracy.")
    report.notes.append(
        "Zangband applies one `pval` to every modifier flag an object carries, "
        "as it does for artifacts. An object granting +2 INT and WIS grants +2 "
        "to both because it cannot express anything else.")

    entries: list[aformat.Entry] = []
    counts = {"spellbook": 0, "inherited": 0, "present": 0, "renamed": 0,
              "rejected": 0, "deferred": 0}

    for slot, rec in sorted(zang.items(), key=lambda kv: kv[1].index):
        key = "%d:%d" % slot
        tval, _sval = slot

        if tval in BOOKS:
            counts["spellbook"] += 1
            continue

        if key in rejects:
            counts["rejected"] += 1
            report.skipped.append((rec.name, rejects[key]["why"]))
            continue

        # An INSTA_ART kind is not an object. It exists so an artifact has
        # something to hang on, is never generated in its own right, and in
        # Zangband is often not even named -- seven of them are just "Ring".
        # 4.2 writes a dummy kind for any artifact base it lacks
        # (obj-init.c:113), so importing these does not fill a gap; it puts a
        # plain Ring in front of the dummy that the Ring of Barahir needs, and
        # artifact.txt then fails to parse.
        if "INSTA_ART" in rec.flags():
            counts["rejected"] += 1
            report.skipped.append((
                rec.name,
                "INSTA_ART: an artifact base rather than an object kind, and "
                "4.2 supplies its own (obj-init.c:113)"))
            continue

        if key in renames:
            counts["renamed"] += 1
            spec = renames[key]
            report.skipped.append((
                rec.name,
                "Zangband's name for Angband's '%s', which 4.2 still ships%s"
                % (spec["to"],
                   "; " + spec["note"] if spec.get("note") else "")))
            continue

        if key in defers:
            counts["deferred"] += 1
            spec = defers[key]
            note = spec.get("note",
                             STATUE_NOTE if spec["why"] == "statue" else "")
            # The ten statues share one reason; saying it ten times buries it.
            if note and any(note == n for _, _, n in report.deferred):
                note = "As above."
            report.deferred.append((rec.name, spec["why"], note))
            continue

        obj_type = types.get(tval)
        if obj_type is None:
            counts["inherited"] += 1
            continue

        if (obj_type, zformat.match_key(rec.name)) in present:
            counts["present"] += 1
            continue


        # A slot Zangband inherited and left alone is Angband's object, not
        # Zangband's, even where 4.2 has since retired it. A slot it re-used
        # for something else -- an Amulet of Berserk Strength where 2.8.1 had
        # Adornment -- is new content, and reaches the conversion below. The
        # difference is the name, which is why [rename] has to say which of
        # the two a renamed slot is.
        if (slot in old
                and zformat.match_key(old[slot].name)
                == zformat.match_key(rec.name)):
            counts["inherited"] += 1
            continue

        if slot in old:
            item_note = ("Zangband re-used 2.8.1's slot for '%s'; this is a "
                         "different object, not a rename" % old[slot].name)
        else:
            item_note = ""

        item = Converted(name=rec.name, source_index=rec.index)
        entry = aformat.Entry()
        if item_note:
            item.translations.append(item_note)

        info = (rec.first("I") or "").split(":")
        pval = int(info[2]) if len(info) > 2 else 0

        entry.set("name", rec.name)
        entry.set("type", obj_type)

        graphics = (rec.first("G") or "").split(":")
        if len(graphics) >= 2:
            entry.set("graphics", "%s:%s" % (graphics[0], graphics[1]))

        world = (rec.first("W") or "").split(":")
        if len(world) >= 4:
            entry.set("level", int(world[0]))
            entry.set("weight", int(world[2]))
            entry.set("cost", int(world[3]))

        # Zangband's A: line is up to four "depth/rarity" pairs; 4.2 has one
        # commonness and a depth range. 2.8.1 turned rarity into a weight as
        # `100 / rarity` (init2.c:2284), and 4.2 uses the weight directly.
        #
        # Which band's rarity to use matters. Taking the commonest of them
        # makes a No-dachi -- A:61/3:80/1, rare at 61 and certain by 80 --
        # as common at 61 as it is at 80. The first band is the one at the
        # object's own level, and is the closer read of a single number.
        alloc = [p for p in (rec.first("A") or "").split(":") if p]
        if alloc:
            depths, rarities = [], []
            for pair in alloc:
                depth, _, rarity = pair.partition("/")
                try:
                    depths.append(int(depth))
                    rarities.append(int(rarity) if rarity else 1)
                except ValueError:
                    continue
            if depths:
                commonness = max(1, min(100, round(100 / max(1, rarities[0]))))
                # Both ends are clamped to the allocation table's last row.
                # Angband's own data reads "to 100" throughout and means "to
                # the bottom"; Zangband's dungeon went deeper than 4.2's table
                # does, so a band it puts at 105 has to come back to the
                # ceiling or the object has no row to be found in at all.
                lo = min(max(1, min(depths)), ceiling)
                hi = ceiling
                entry.set("alloc", "%d:%d to %d" % (commonness, lo, hi))
                note = ("Zangband allocation %s; rarity %d at the first band "
                        "inverted to commonness" % (" ".join(alloc), rarities[0]))
                if min(depths) > ceiling:
                    note += ("; source band at depth %d is below the object "
                             "table's last row (%d) and was brought up to it"
                             % (min(depths), ceiling))
                item.fields["alloc"] = rules.Value(
                    entry.get("alloc"), "CNT-11", rules.DERIVED, note)

        # Zangband gives every kind a P: line whether or not it means anything:
        # the Amulet of Destruction is recorded with 7d7 damage dice it can
        # never use. 4.2 writes `attack:0d0:0:0` on everything that is not a
        # weapon, so the dice go and the to-hit and to-dam penalties stay.
        power = (rec.first("P") or "").split(":")
        WEAPONS = {"shot", "arrow", "bolt", "bow", "digger", "hafted",
                   "polearm", "sword"}
        if len(power) >= 5:
            dice = power[1] if obj_type in WEAPONS else "0d0"
            if dice != power[1]:
                item.translations.append(
                    "damage dice %s dropped: Zangband records them on every "
                    "kind, and a %s cannot use them" % (power[1], obj_type))
            entry.set("attack", "%s:%s:%s" % (dice, power[2], power[3]))
            to_a = power[4]
            for pattern, render in _MAKE_TOA:
                match = pattern.search("\n".join(rec.all("L")))
                if match:
                    to_a = render(match)
                    item.translations.append(
                        "armour bonus scales with depth: `%s` becomes `%s`"
                        % (match.group(0).strip(), to_a))
                    break
            entry.set("armor", "%s:%s" % (power[0], to_a))

        # What L:MAKE: does to the pval, as a 4.2 random expression, so that a
        # ring found at depth is worth more than one found at the gate.
        script = "\n".join(rec.all("L"))
        pval_expr = str(pval)
        for pattern, render in _MAKE_PVAL:
            match = pattern.search(script)
            if match:
                pval_expr = render(match, pval)
                item.translations.append(
                    "pval scales with depth: `%s` becomes `%s`"
                    % (match.group(0), pval_expr))
                break

        flags, values, brands, slays, curses = [], [], [], [], []
        for flag in rec.flags():
            if flag in flagmap.flag_kind:
                flags.append(flagmap.flag_kind[flag])
                continue
            disposition, reason = flagmap.disposition(flag)
            if disposition == "value":
                if flag in flagmap.value_pval:
                    if pval:
                        values.append("%s[%s]"
                                      % (flagmap.value_pval[flag], pval_expr))
                    else:
                        item.flag_dispositions.append((
                            flag, "manual",
                            "modifier flag on a kind with no pval; Zangband "
                            "would grant nothing"))
                else:
                    spec = flagmap.value_fixed[flag]
                    values.append("%s[%d]" % (spec["name"], spec["level"]))
            elif disposition == "flag":
                flags.append(flagmap.flag[flag])
            elif disposition == "brand":
                brands.append(flagmap.brand[flag])
            elif disposition == "slay":
                slays.append(flagmap.slay[flag])
            elif disposition == "curse":
                spec = flagmap.curse[flag]
                curses.append("%s:%d" % (spec["name"], spec["power"]))
            else:
                item.flag_dispositions.append((flag, disposition, reason))

        def dedupe(seq):
            seen: dict[str, None] = {}
            for value in seq:
                seen.setdefault(value, None)
            return list(seen)

        if flags:
            entry.set("flags", " | ".join(dedupe(flags)))
        if values:
            entry.set("values", " | ".join(dedupe_values(values)))
        for brand in dedupe(brands):
            entry.pairs.append(("brand", brand))
        for slay in dedupe(slays):
            entry.pairs.append(("slay", slay))
        for curse in dedupe(curses):
            entry.pairs.append(("curse", curse))

        if key in messages:
            entry.set("msg", messages[key])

        if key in effects:
            spec = effects[key]
            for line in spec["lines"]:
                tag, _, value = line.partition(":")
                entry.pairs.append((tag, value))
            item.fields["effect"] = rules.Value(
                spec["lines"][0].partition(":")[2], "CNT-11", rules.DERIVED,
                spec.get("note", ""))
        elif [line for line in rec.all("L") if line.startswith("USE")]:
            item.flag_dispositions.append((
                "L:USE", "manual",
                "carries Lua that objmap.toml has no effect translation for"))
        elif "add_ego_power" in script:
            item.flag_dispositions.append((
                "add_ego_power", "manual",
                "rolls a random extra ability at creation. 4.2 has the same "
                "idea in KF_RAND_POWER and KF_RAND_HI_RES, but reads them from "
                "the ego only (obj-make.c:398), never from a kind, so putting "
                "one here would parse and do nothing."))

        desc = " ".join(d.strip() for d in rec.all("D"))
        entry.set("desc", desc if desc else "An object of Zangband.")

        for field_name, value in overrides.get(key, {}).items():
            entry.set(field_name, value)
            item.overridden.append(field_name)

        report.items.append(item)
        entries.append(entry)

    report.notes.append(
        "Skipped without a line each: %d spellbook kinds (CNT-10, gated on the "
        "magic realms in M9), %d kinds 4.2 already has under the same name, and "
        "%d Angband kinds that 4.2 retired and Zangband did not add."
        % (counts["spellbook"], counts["present"], counts["inherited"]))

    OUTDIR.mkdir(exist_ok=True)
    report_path = OUTDIR / "objects.report.md"
    report_path.write_text(report.render(), encoding="utf-8")
    print("report:  %s" % report_path.relative_to(ROOT))

    # 4.2 quits at startup if a tval has more kinds than flavours, and the
    # rings and amulets above take both pools past their limit. Zangband has
    # its own longer lists; the ones 4.2 does not already carry make up the
    # difference, and are its content rather than an invention.
    have = {tval: set() for tval in ("ring", "amulet")}
    tval_now = ""
    for line in (GAMEDATA / "flavor.txt").read_text(
            encoding="utf-8", errors="replace").splitlines():
        if line.startswith("kind:"):
            tval_now = line.split(":")[1]
        elif tval_now in have and line.startswith(("flavor:", "fixed:")):
            have[tval_now].add(line.rsplit(":", 1)[-1].strip().casefold())

    flavours: list[tuple[str, str, str, str]] = []
    for kind, glyph in (("ring", "="), ("amulet", '"')):
        for adjective, colour in zangband_flavours(kind):
            if adjective.casefold() in have[kind]:
                continue
            if any(a == adjective for _, _, a, _ in flavours):
                continue
            flavours.append((kind, glyph, adjective,
                             _TERM_COLOUR.get(colour, "White")))
    report.notes.append(
        "%d ring and %d amulet flavours were taken from Zangband's flavor.c, "
        "being those 4.2 does not already have. Without them the game will not "
        "start: 4.2 requires at least one flavour per kind of a flavoured tval."
        % (sum(1 for k, _, _, _ in flavours if k == "ring"),
           sum(1 for k, _, _, _ in flavours if k == "amulet")))

    if args.write:
        data_path = OUTDIR / "object.zangband.txt"
        aformat.write(
            str(data_path), entries,
            preamble=(
                "# object.zangband.txt — generated by tools/zconv. "
                "Do not hand-edit.\n"
                "# Hand-tuned values belong in tools/zconv/overrides.toml "
                "(BAL-12).\n"
                "# Source: %s\n" % (ZANGBAND / "k_info.txt")
            ),
        )
        print("data:    %s" % data_path.relative_to(ROOT))

        # Flavour indices continue above the highest flavor.txt uses, since
        # the two files share one list and an index is an identity.
        base = 1 + max(int(n) for n in re.findall(
            r"^(?:flavor|fixed):(\d+)", (GAMEDATA / "flavor.txt").read_text(
                encoding="utf-8", errors="replace"), re.M))
        flavour_path = OUTDIR / "flavor.zangband.txt"
        lines = [
            "# flavor.zangband.txt — generated by tools/zconv. "
            "Do not hand-edit.",
            "# Source: %s" % (ROOT / "archive" / "zangband" / "src" / "flavor.c"),
            "#",
            "# Zangband's ring and amulet flavours that 4.2 does not have. The",
            "# imported kinds need them: 4.2 assigns one flavour per kind and",
            "# quits if it runs out.",
        ]
        last = ""
        for offset, (kind, glyph, adjective, colour) in enumerate(flavours):
            if kind != last:
                lines += ["", "kind:%s:%s" % (kind, glyph)]
                last = kind
            lines.append("flavor:%d:%s:%s" % (base + offset, colour, adjective))
        flavour_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
        print("data:    %s" % flavour_path.relative_to(ROOT))
    else:
        print("data:    not written (pass --write)")

    print("entries: %d   renamed: %d   rejected: %d   deferred: %d   "
          "unresolved flags: %d"
          % (len(report.items), counts["renamed"], counts["rejected"],
             counts["deferred"],
             len({f for i in report.items
                  for f, d, _ in i.flag_dispositions if d == "unresolved"})))
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        prog="zconv", description="Convert Zangband data onto Angband 4.2's model.")
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("analyse", help="derive scales and report; write nothing")

    monsters = sub.add_parser("monsters", help="convert r_info.txt to 4.2 monster entries")
    monsters.add_argument("--write", action="store_true",
                          help="write the data file as well as the report")
    monsters.add_argument("--theme-only", action="store_true",
                          help="DEC-19: only monsters with Amber/Mythos/Chaos identity")

    arts = sub.add_parser("artifacts", help="convert a_info.txt to 4.2 artifacts")
    arts.add_argument("--write", action="store_true",
                      help="write the data file as well as the report")

    egos = sub.add_parser("egos", help="convert e_info.txt to 4.2 ego items")
    egos.add_argument("--write", action="store_true",
                      help="write the data file as well as the report")

    objects = sub.add_parser("objects",
                             help="convert k_info.txt to 4.2 object kinds")
    objects.add_argument("--write", action="store_true",
                         help="write the data file as well as the report")

    args = parser.parse_args()
    return {"analyse": cmd_analyse, "monsters": cmd_monsters,
            "artifacts": cmd_artifacts,
            "egos": cmd_egos,
            "objects": cmd_objects}[args.command](args)


if __name__ == "__main__":
    raise SystemExit(main())
