"""
Artifact conversion: Zangband ``a_info.txt`` -> Angband 4.2 ``artifact.txt``.

Harder than the bestiary, because the two object models differ structurally.
Zangband carries one ``pval`` and a set of flags saying what it applies to;
4.2 gives each property its own value and splits them across ``flags:``,
``values:``, ``brand:``, ``slay:`` and ``curse:`` lines.  Zangband also names
its base object by numeric ``tval:sval``, where 4.2 names it in words, and
scripts activations in Lua where 4.2 selects from a named vocabulary.

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

import re

import zformat


class ObjFlagMap:
    """Disposition of every Zangband object flag (CNT-08, CNT-09)."""

    def __init__(self, spec: dict):
        self.value_pval: dict[str, str] = spec.get("value_pval", {})
        self.value_fixed: dict[str, dict] = spec.get("value_fixed", {})
        self.flag: dict[str, str] = spec.get("flag", {})
        self.brand: dict[str, str] = spec.get("brand", {})
        self.slay: dict[str, str] = spec.get("slay", {})
        self.curse: dict[str, dict] = spec.get("curse", {})
        self.reject: dict[str, str] = spec.get("reject", {})
        self.implement: dict[str, dict] = spec.get("implement", {})
        # Ego-only vocabulary, merged in so one disposition table serves both.
        self.flag.update(spec.get("flag_ego", {}))
        self.value_pval.update(spec.get("value_pval_ego", {}))
        self.implement.update(spec.get("implement_ego", {}))
        for name, entry in spec.get("defer_ego", {}).items():
            self.implement.setdefault(name, entry)
        self.rand_ability: dict[str, dict] = spec.get("rand_ability", {})

    def disposition(self, flag: str) -> tuple[str, str]:
        if flag in self.value_pval:
            return "value", f"-> values:{self.value_pval[flag]}[pval]"
        if flag in self.value_fixed:
            spec = self.value_fixed[flag]
            return "value", f"-> values:{spec['name']}[{spec['level']}]"
        if flag in self.flag:
            return "flag", f"-> flags:{self.flag[flag]}"
        if flag in self.brand:
            return "brand", f"-> brand:{self.brand[flag]}"
        if flag in self.slay:
            return "slay", f"-> slay:{self.slay[flag]}"
        if flag in self.curse:
            spec = self.curse[flag]
            return "curse", f"-> curse:{spec['name']}:{spec['power']}"
        if flag in self.reject:
            return "reject", self.reject[flag]
        if flag in self.implement:
            spec = self.implement[flag]
            return "implement", f"{spec['milestone']}: {spec['note']}"
        return "unresolved", "no disposition recorded in objflagmap.toml"


# --- base object resolution ---------------------------------------------


def build_kind_index(k_info_path: str) -> dict[tuple[int, int], str]:
    """Map Zangband ``(tval, sval)`` to the object kind's name."""
    index: dict[tuple[int, int], str] = {}
    for rec in zformat.parse(k_info_path):
        info = rec.first("I")
        if not info:
            continue
        parts = info.split(":")
        if len(parts) < 2:
            continue
        try:
            index[(int(parts[0]), int(parts[1]))] = rec.name
        except ValueError:
            continue
    return index


def build_base_index(entries: list) -> dict[str, tuple[str, str]]:
    """Map a normalised 4.2 object name to its ``(type, display name)``."""
    index: dict[str, tuple[str, str]] = {}
    for entry in entries:
        kind = entry.get("type")
        if not kind:
            continue
        display = zformat.normalise_name(entry.name).replace("~", "").strip()
        index.setdefault(entry.key, (kind, display))
    return index


# --- activations ---------------------------------------------------------

#: Zangband scripts activations in Lua; 4.2 selects from activation.txt.  The
#: Lua is a small program, so we match on the primitive it calls and the
#: element it passes, and take 4.2's nearest named activation.  Anything not
#: matched here is reported rather than guessed at.
ACTIVATION_PATTERNS: list[tuple[str, str, str]] = [
    (r"wiz_lite\(", "CLAIRVOYANCE", "wiz_lite -> full level illumination"),
    (r"lite_area\(", "ILLUMINATION", "lite_area -> local illumination"),
    (r"detect_all\(", "DETECT_ALL", "detect_all"),
    (r"map_area\(", "MAPPING", "map_area"),
    (r"word_of_recall\(", "RECALL", "word_of_recall"),
    (r"fire_ball\(\s*GF_FIRE", "FIRE_BALL", "fire_ball(GF_FIRE)"),
    (r"fire_ball\(\s*GF_COLD", "COLD_BALL100", "fire_ball(GF_COLD)"),
    (r"fire_ball\(\s*GF_ELEC", "ELEC_BALL", "fire_ball(GF_ELEC)"),
    (r"fire_ball\(\s*GF_ACID", "ACID_BALL", "fire_ball(GF_ACID)"),
    (r"fire_ball\(\s*GF_POIS", "STINKING_CLOUD", "fire_ball(GF_POIS)"),
    (r"fire_ball\(\s*GF_MANA", "MANA_BOLT", "fire_ball(GF_MANA)"),
    (r"fire_bolt\(\s*GF_FIRE", "FIRE_BOLT", "fire_bolt(GF_FIRE)"),
    (r"fire_bolt\(\s*GF_COLD", "COLD_BOLT", "fire_bolt(GF_COLD)"),
    (r"fire_bolt\(\s*GF_ELEC", "ELEC_BOLT", "fire_bolt(GF_ELEC)"),
    (r"fire_bolt\(\s*GF_ACID", "ACID_BOLT", "fire_bolt(GF_ACID)"),
    (r"fire_bolt\(\s*GF_MISSILE", "MISSILE", "fire_bolt(GF_MISSILE)"),
    (r"drain_life\(", "DRAIN_LIFE2", "drain_life"),
    (r"dispel_evil\(", "DISPEL_EVIL", "dispel_evil"),
    (r"turn_monsters\(", "MON_SCARE", "turn_monsters"),
    (r"slow_monsters\(", "MON_SLOW", "slow_monsters"),
    (r"sleep_monsters\(", "SLEEP_ALL", "sleep_monsters"),
    (r"confuse_monsters\(", "MON_CONFUSE", "confuse_monsters"),
    (r"teleport_player\(", "TELE_LONG", "teleport_player"),
    (r"destroy_area\(", "DESTRUCTION2", "destroy_area"),
    (r"earthquake\(", "EARTHQUAKES", "earthquake"),
    (r"inc_fast\(", "HASTE", "inc_fast -> haste self"),
    (r"inc_shero\(", "BERSERKER", "inc_shero -> berserk strength"),
    (r"inc_hero\(", "HERO", "inc_hero"),
    (r"inc_blessed\(", "BLESSING", "inc_blessed"),
    (r"inc_protevil\(", "PROTEVIL", "inc_protevil"),
    (r"inc_oppose_fire\(", "RESIST_FIRE", "inc_oppose_fire"),
    (r"inc_oppose_cold\(", "RESIST_COLD", "inc_oppose_cold"),
    (r"inc_oppose_elec\(", "RESIST_ELEC", "inc_oppose_elec"),
    (r"inc_oppose_acid\(", "RESIST_ACID", "inc_oppose_acid"),
    (r"inc_oppose_pois\(", "RESIST_POIS", "inc_oppose_pois"),
    (r"inc_tim_invis\(", "TMD_SINVIS", "inc_tim_invis"),
    (r"inc_tim_infra\(", "TMD_INFRA", "inc_tim_infra"),
    (r"hp_player\(", "CURE_SERIOUS", "hp_player -> healing"),
    (r"clear_afraid\(", "CURE_PARANOIA", "clear_afraid"),
    (r"clear_poisoned\(", "CURE_POISON", "clear_poisoned"),
    (r"clear_cut\(", "CURE_LIGHT", "clear_cut"),
    (r"remove_curse\(", "REMOVE_CURSE", "remove_curse"),
    (r"recharge\(", "RECHARGE", "recharge"),
    (r"stone_to_mud\(", "STONE_TO_MUD", "stone_to_mud"),
    (r"teleport_monster\(", "TELE_OTHER", "teleport_monster"),
    (r"banish_evil\(", "BANISHMENT", "banish_evil"),
    (r"probing\(", "PROBING", "probing"),
    (r"restore_level\(", "RESTORE_EXP", "restore_level"),
    (r"do_res_stat\(", "RESTORE_ALL", "do_res_stat -> restore statistics"),
    (r"aggravate_monsters\(", "CONFUSING", "aggravate -> no direct equivalent"),
]


def match_activation(script: str) -> tuple[str | None, str]:
    """Choose the nearest 4.2 activation for a Zangband Lua activation."""
    body = "\n".join(
        line for line in script.splitlines() if "msgf(" not in line
    )
    for pattern, activation, note in ACTIVATION_PATTERNS:
        if re.search(pattern, body):
            return activation, note
    return None, "no 4.2 activation matches this script"


_TIMEOUT = re.compile(r"timeout\s*=\s*rand_range\((\d+),\s*(\d+)\)")
_TIMEOUT_FIXED = re.compile(r"timeout\s*=\s*(\d+)")


def match_timeout(script: str) -> str | None:
    """Translate Zangband's recharge time into 4.2's ``time:`` expression."""
    m = _TIMEOUT.search(script)
    if m:
        low, high = int(m.group(1)), int(m.group(2))
        span = max(1, high - low)
        return f"{low}+d{span}"
    m = _TIMEOUT_FIXED.search(script)
    if m:
        return m.group(1)
    return None
