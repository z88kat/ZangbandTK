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

Copyright (c) 2026 ZangbandZK contributors

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


def load_basemap() -> dict[str, dict]:
    """Glyphs whose 4.2 template lives under a different display character."""
    path = Path(__file__).resolve().parent / "basemap.toml"
    if not path.exists():
        return {}
    with path.open("rb") as handle:
        return tomllib.load(handle).get("map", {})


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
                scalars[field] = float(m.group(1))
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
    overrides = load_overrides().get("monster", {})
    lethality = load_lethality()
    mapping = rules.derive_sleep_mapping(old_by_key, new_by_key)
    curve_42 = rules.Curve.fit(new)
    curve_z = _zangband_curve(zang)

    candidates = [m for m in zang
                  if m.key not in old_by_key and m.key not in new_by_key]
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
        keep_glyph = True
        if mon.symbol and mon.symbol in bases:
            record("base", rules.Value(bases[mon.symbol], "CNT-01", rules.CONVERTED,
                                       f"inferred from glyph '{mon.symbol}'"))
        elif mon.symbol and mon.symbol in basemap:
            spec = basemap[mon.symbol]
            keep_glyph = spec.get("keep_glyph", True)
            record("base", rules.Value(spec["base"], "CNT-01", rules.CONVERTED,
                                       f"glyph '{mon.symbol}' mapped to 4.2's "
                                       f"'{spec['base']}' template"))
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
    print(f"entries: {len(report.items)}   invented values: {invented}   "
          f"unmapped flags: {len({f for i in report.items for f in i.unmapped_flags})}")
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

    args = parser.parse_args()
    return {"analyse": cmd_analyse, "monsters": cmd_monsters}[args.command](args)


if __name__ == "__main__":
    raise SystemExit(main())
