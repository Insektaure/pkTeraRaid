#!/usr/bin/env python3
"""
Convert CaptureSight Rust SwSh den data to C++ headers for pkTeraRaid.

Usage:
    python3 tools/convert_swsh_nests.py

Reads from ../CaptureSight/libs/csight-core/src/swsh/ and romfs/data/species_en.txt.
Outputs to include/swsh/den_hashes.h, include/swsh/sword_nests.h, include/swsh/shield_nests.h.
"""

import re
import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR)
CAPTURESIGHT_SWSH = os.path.join(PROJECT_DIR, "..", "CaptureSight", "libs", "csight-core", "src", "swsh")
SPECIES_FILE = os.path.join(PROJECT_DIR, "romfs", "data", "species_en.txt")
OUT_DIR = os.path.join(PROJECT_DIR, "include", "swsh")

# Manual mapping for species names that don't match after normalization
SPECIES_OVERRIDE = {
    "FarfetchD": 83,
    "SirfetchD": 865,
    "MrMime": 122,
    "MrRime": 866,
    "MimeJr": 439,
    "NidoranF": 29,
    "NidoranM": 32,
    "HakamoO": 783,
    "JangmoO": 782,
    "KommoO": 784,
    "TypeNull": 772,
    "None": 0,
}


def normalize(name):
    """Normalize a species name for fuzzy matching."""
    return re.sub(r"[^a-zA-Z0-9]", "", name).lower()


def build_species_map():
    """Build species name -> national dex number mapping from species_en.txt."""
    mapping = {}
    with open(SPECIES_FILE, encoding="utf-8") as f:
        for i, line in enumerate(f):
            name = line.strip()
            if name:
                mapping[normalize(name)] = i
    return mapping


def resolve_species(rust_name, species_map):
    """Resolve a Rust species name to a national dex number."""
    if rust_name in SPECIES_OVERRIDE:
        return SPECIES_OVERRIDE[rust_name]
    normalized = normalize(rust_name)
    if normalized in species_map:
        return species_map[normalized]
    print(f"WARNING: Could not resolve species '{rust_name}'", file=sys.stderr)
    return 0


def parse_den_hashes(filepath):
    """Parse den_hashes.rs: extract Nest enum variants, NORMAL_DENS, RARE_DENS."""
    with open(filepath, encoding="utf-8") as f:
        content = f.read()

    # Extract enum variants (skip commented-out ones)
    enum_match = re.search(r"pub enum Nest \{(.*?)\}", content, re.DOTALL)
    variants = []
    for line in enum_match.group(1).splitlines():
        line = line.strip().rstrip(",")
        if line.startswith("//") or not line:
            continue
        variants.append(line)

    # Build name -> index map
    variant_to_id = {v: i for i, v in enumerate(variants)}

    # Parse NORMAL_DENS array
    normal_match = re.search(r"pub const NORMAL_DENS.*?=\s*\[(.*?)\];", content, re.DOTALL)
    normal_dens = []
    for line in normal_match.group(1).splitlines():
        m = re.search(r"Nest::(\w+)", line)
        if m:
            normal_dens.append(variant_to_id[m.group(1)])

    # Parse RARE_DENS array
    rare_match = re.search(r"pub const RARE_DENS.*?=\s*\[(.*?)\];", content, re.DOTALL)
    rare_dens = []
    for line in rare_match.group(1).splitlines():
        m = re.search(r"Nest::(\w+)", line)
        if m:
            rare_dens.append(variant_to_id[m.group(1)])

    return variants, variant_to_id, normal_dens, rare_dens


def parse_nests(filepath, species_map):
    """Parse a Rust nest file (sword_nests.rs or shield_nests.rs).
    Returns dict: nest_const_name -> list of (species_dex, flawless_ivs, [prob0..prob4])
    """
    tables = {}

    nest_re = re.compile(r"const (NEST_\w+):\s*DenEncounters\s*=\s*\[")
    entry_re = re.compile(
        r"DenEncounter\s*\{\s*species:\s*types::Species::(\w+),"
        r"\s*flawless_ivs:\s*(\d+),"
        r"\s*probabilities:\s*\[([^\]]+)\]"
    )

    current_name = None
    current_entries = []

    with open(filepath, encoding="utf-8") as f:
        for line in f:
            m = nest_re.search(line)
            if m:
                if current_name:
                    tables[current_name] = current_entries
                current_name = m.group(1)
                current_entries = []

            m = entry_re.search(line)
            if m:
                sp_name = m.group(1)
                dex = resolve_species(sp_name, species_map)
                flawless = int(m.group(2))
                probs = [int(x.strip()) for x in m.group(3).split(",")]
                while len(probs) < 5:
                    probs.append(0)
                current_entries.append((dex, flawless, probs))

    if current_name:
        tables[current_name] = current_entries

    return tables


def find_dispatch_mapping(filepath, variant_to_id):
    """Parse the get_encounters() match arms to map nest variant -> const name.
    Returns dict: nest_id -> const_name
    """
    dispatch = {}

    with open(filepath, encoding="utf-8") as f:
        for line in f:
            m = re.search(r"Nest::(\w+)\s*=>\s*&(NEST_\w+)", line)
            if m:
                variant = m.group(1)
                const_name = m.group(2)
                if variant in variant_to_id:
                    dispatch[variant_to_id[variant]] = const_name

    return dispatch


def generate_den_hashes_h(variants, normal_dens, rare_dens):
    """Generate include/swsh/den_hashes.h."""
    lines = [
        "#pragma once",
        "// Auto-generated by tools/convert_swsh_nests.py — do not edit manually",
        "#include <cstdint>",
        "",
        "namespace SwShDenHashes {",
        "",
        f"constexpr int NEST_COUNT = {len(variants)};",
        "constexpr int NEST_EVENT = 0;  // EventPlaceholder",
        "",
        "// Nest variant names (for reference):",
    ]
    for i, v in enumerate(variants):
        lines.append(f"//   {i} = {v}")
    lines.append("")

    # NORMAL_DENS
    lines.append(f"inline constexpr uint8_t NORMAL_DENS[{len(normal_dens)}] = {{")
    for i in range(0, len(normal_dens), 10):
        chunk = normal_dens[i : i + 10]
        lines.append("    " + ", ".join(f"{x:3d}" for x in chunk) + ",")
    lines.append("};")
    lines.append("")

    # RARE_DENS
    lines.append(f"inline constexpr uint8_t RARE_DENS[{len(rare_dens)}] = {{")
    for i in range(0, len(rare_dens), 10):
        chunk = rare_dens[i : i + 10]
        lines.append("    " + ", ".join(f"{x:3d}" for x in chunk) + ",")
    lines.append("};")
    lines.append("")

    lines.append("inline uint8_t getNestId(int denIndex, bool isRare) {")
    lines.append(f"    if (denIndex < 0 || denIndex >= {len(normal_dens)}) return NEST_EVENT;")
    lines.append("    return isRare ? RARE_DENS[denIndex] : NORMAL_DENS[denIndex];")
    lines.append("}")
    lines.append("")
    lines.append("} // namespace SwShDenHashes")
    lines.append("")

    return "\n".join(lines)


def generate_nests_h(game_name, tables, dispatch, nest_count):
    """Generate sword_nests.h or shield_nests.h."""
    fn_name = f"get{game_name}Encounters"
    guard = f"SWSH_{game_name.upper()}_NESTS_H"

    lines = [
        f"#pragma once",
        "// Auto-generated by tools/convert_swsh_nests.py — do not edit manually",
        '#include "swsh/den_types.h"',
        "",
        f"namespace SwSh{game_name}Nests {{",
        "",
    ]

    # Emit all nest tables
    # Collect all const names used in dispatch
    used_consts = set(dispatch.values())

    for const_name, entries in sorted(tables.items()):
        if const_name not in used_consts:
            continue
        lines.append(f"inline constexpr SwShDenEncounter {const_name}[12] = {{")
        for dex, flawless, probs in entries:
            prob_str = ", ".join(str(p) for p in probs)
            lines.append(f"    {{{dex:4d}, {flawless}, {{{prob_str}}}}},")
        # Pad to 12 entries if needed
        for _ in range(12 - len(entries)):
            lines.append("    {   0, 0, {0, 0, 0, 0, 0}},")
        lines.append("};")
        lines.append("")

    # Empty table for EventPlaceholder/missing
    lines.append("inline constexpr SwShDenEncounter NEST_EMPTY[12] = {")
    for _ in range(12):
        lines.append("    {   0, 0, {0, 0, 0, 0, 0}},")
    lines.append("};")
    lines.append("")

    # Dispatch function
    lines.append(f"inline const SwShDenEncounter* {fn_name}(uint8_t nestId) {{")
    lines.append("    switch (nestId) {")
    for nest_id, const_name in sorted(dispatch.items()):
        if const_name in tables:
            lines.append(f"    case {nest_id}: return {const_name};")
    lines.append("    default: return NEST_EMPTY;")
    lines.append("    }")
    lines.append("}")
    lines.append("")
    lines.append(f"}} // namespace SwSh{game_name}Nests")
    lines.append("")

    return "\n".join(lines)


def main():
    print("Building species map...")
    species_map = build_species_map()
    print(f"  {len(species_map)} species loaded")

    print("Parsing den_hashes.rs...")
    hashes_file = os.path.join(CAPTURESIGHT_SWSH, "den_hashes.rs")
    variants, variant_to_id, normal_dens, rare_dens = parse_den_hashes(hashes_file)
    print(f"  {len(variants)} nest variants, {len(normal_dens)} normal dens, {len(rare_dens)} rare dens")

    print("Parsing sword_nests.rs...")
    sword_file = os.path.join(CAPTURESIGHT_SWSH, "sword_nests.rs")
    sword_tables = parse_nests(sword_file, species_map)
    sword_dispatch = find_dispatch_mapping(sword_file, variant_to_id)
    print(f"  {len(sword_tables)} tables, {len(sword_dispatch)} dispatch entries")

    print("Parsing shield_nests.rs...")
    shield_file = os.path.join(CAPTURESIGHT_SWSH, "shield_nests.rs")
    shield_tables = parse_nests(shield_file, species_map)
    shield_dispatch = find_dispatch_mapping(shield_file, variant_to_id)
    print(f"  {len(shield_tables)} tables, {len(shield_dispatch)} dispatch entries")

    os.makedirs(OUT_DIR, exist_ok=True)

    print("Generating den_hashes.h...")
    hashes_h = generate_den_hashes_h(variants, normal_dens, rare_dens)
    with open(os.path.join(OUT_DIR, "den_hashes.h"), "w") as f:
        f.write(hashes_h)

    print("Generating sword_nests.h...")
    sword_h = generate_nests_h("Sword", sword_tables, sword_dispatch, len(variants))
    with open(os.path.join(OUT_DIR, "sword_nests.h"), "w") as f:
        f.write(sword_h)

    print("Generating shield_nests.h...")
    shield_h = generate_nests_h("Shield", shield_tables, shield_dispatch, len(variants))
    with open(os.path.join(OUT_DIR, "shield_nests.h"), "w") as f:
        f.write(shield_h)

    print("Done!")
    print(f"  den_hashes.h:   {len(variants)} nests, {len(normal_dens)}+{len(rare_dens)} den mappings")
    print(f"  sword_nests.h:  {len(sword_tables)} tables")
    print(f"  shield_nests.h: {len(shield_tables)} tables")


if __name__ == "__main__":
    main()
