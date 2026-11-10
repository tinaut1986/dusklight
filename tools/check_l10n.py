#!/usr/bin/env python3
"""
Check the Dusklight UI translations in res/l10n against the source code.

Errors (exit code 1):
  - a locale file is not valid JSON or has no "_meta" object with a "name"
  - a key used in the code (tr()/tr_str()/tr_source() or an l10n key literal)
    is missing from en.json
  - a translation's {placeholders} differ from English (the game would ignore
    that entry and show English, so it is reported as an error here)

Warnings:
  - keys present in a translation but not in en.json (obsolete)
  - keys in en.json that no code references (possibly unused)

Report:
  - per-language coverage, and with --missing the untranslated keys

Usage:
    python tools/check_l10n.py [--missing] [--lang es]
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
L10N = ROOT / "res" / "l10n"
SOURCES = [ROOT / "src" / "dusk"]
SOURCE = "en"

CALL_RE = re.compile(r'\btr(?:_str|_source)?\(\s*"([^"]+)"\s*\)')
# Prefix of a key built at runtime, e.g. tr_str("achievements.name_" + key)
DYNAMIC_RE = re.compile(r'\btr(?:_str|_source)?\(\s*(?:std::string\()?"([^"]+)"\)?\s*\+')
LITERAL_RE = re.compile(r'"([a-z][a-z0-9_]*(?:\.[a-z0-9_]+)+)"')
# Literals like "editor.hpp" or "achievements.json" are file names, not keys.
FILE_EXTENSIONS = {"h", "hpp", "cpp", "c", "json", "png", "rml", "rcss", "ttf", "txt", "log", "ini", "db"}


def flatten(node: dict, prefix: str = "") -> dict[str, str]:
    out: dict[str, str] = {}
    for key, value in node.items():
        if not prefix and key == "_meta":
            continue
        path = f"{prefix}.{key}" if prefix else key
        if isinstance(value, dict):
            out.update(flatten(value, path))
        elif isinstance(value, str):
            out[path] = value
    return out


def placeholders(text: str) -> list[str]:
    text = text.replace("{{", "").replace("}}", "")
    return sorted(re.findall(r"\{[^{}]*\}", text))


def load(path: Path, errors: list[str]) -> tuple[dict, dict[str, str]] | None:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as e:
        errors.append(f"{path.name}: cannot read: {e}")
        return None
    meta = data.get("_meta")
    if not isinstance(meta, dict) or not isinstance(meta.get("name"), str):
        errors.append(f'{path.name}: missing "_meta": {{"name": "..."}}')
    return data, flatten(data)


def code_keys(sections: set[str]) -> tuple[set[str], set[str]]:
    used: set[str] = set()
    prefixes: set[str] = set()
    for base in SOURCES:
        for path in base.rglob("*.[ch]pp"):
            text = path.read_text(encoding="utf-8", errors="replace")
            used.update(CALL_RE.findall(text))
            prefixes.update(DYNAMIC_RE.findall(text))
            for literal in LITERAL_RE.findall(text):
                if literal.rsplit(".", 1)[1] in FILE_EXTENSIONS:
                    continue
                if literal.split(".", 1)[0] in sections:
                    used.add(literal)
    return used, prefixes


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--missing", action="store_true", help="list untranslated keys per language")
    parser.add_argument("--lang", help="only check this language code")
    args = parser.parse_args()

    errors: list[str] = []
    warnings: list[str] = []

    source = load(L10N / f"{SOURCE}.json", errors)
    if source is None:
        print("\n".join(errors))
        return 1
    source_data, english = source

    used, prefixes = code_keys(set(source_data) - {"_meta"})
    for key in sorted(used - english.keys()):
        if not any(key.startswith(p) for p in prefixes):
            errors.append(f"{SOURCE}.json: key used in code but not defined: {key}")
    unused = [
        k for k in english
        if k not in used and not any(k.startswith(p) for p in prefixes)
    ]
    if unused:
        warnings.append(f"{SOURCE}.json: {len(unused)} key(s) not referenced in code "
                        f"(may be built at runtime): {', '.join(sorted(unused)[:10])}"
                        + (" ..." if len(unused) > 10 else ""))

    files = sorted(p for p in L10N.glob("*.json") if p.stem != SOURCE)
    if args.lang:
        files = [p for p in files if p.stem == args.lang]
        if not files:
            print(f"no res/l10n/{args.lang}.json")
            return 1

    for path in files:
        loaded = load(path, errors)
        if loaded is None:
            continue
        data, strings = loaded
        name = data.get("_meta", {}).get("name", path.stem)
        for key in sorted(strings.keys() - english.keys()):
            warnings.append(f"{path.name}: obsolete key (not in {SOURCE}.json): {key}")
        for key in sorted(strings.keys() & english.keys()):
            if placeholders(strings[key]) != placeholders(english[key]):
                errors.append(f"{path.name}: placeholders differ in {key}: "
                              f"{placeholders(strings[key])} vs English {placeholders(english[key])}")
        missing = sorted(english.keys() - strings.keys())
        done = len(english) - len(missing)
        print(f"{path.stem:8} {name:20} {done}/{len(english)} ({100 * done / max(len(english), 1):.1f}%)")
        if args.missing:
            for key in missing:
                print(f"    missing {key}: {english[key]!r}")

    for w in warnings:
        print(f"warning: {w}")
    for e in errors:
        print(f"error: {e}")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
