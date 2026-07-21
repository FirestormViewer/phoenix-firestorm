#!/usr/bin/env python3

from __future__ import annotations

import argparse
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SKINS_ROOT = REPO / "indra/newview/skins"


def relative_files(base: Path) -> set[str]:
    if not base.is_dir():
        return set()
    return {
        path.relative_to(base).as_posix()
        for path in base.rglob("*")
        if path.is_file()
    }


def prune_skin(skin_dir: Path, *, delete: bool) -> int:
    en_dir = skin_dir / "xui" / "en"
    if not en_dir.is_dir():
        return 0

    en_files = relative_files(en_dir)
    removed = 0
    xui = skin_dir / "xui"

    for locale_dir in sorted(xui.iterdir()):
        if not locale_dir.is_dir() or locale_dir.name == "en":
            continue

        for path in sorted(locale_dir.rglob("*")):
            if not path.is_file():
                continue
            rel = path.relative_to(locale_dir).as_posix()
            if rel in en_files:
                continue

            print(f"{'delete' if delete else 'would delete'}: {skin_dir.name}/xui/{locale_dir.name}/{rel}")
            if delete:
                path.unlink()
            removed += 1

        if delete:
            for path in sorted(locale_dir.rglob("*"), reverse=True):
                if path.is_dir() and not any(path.iterdir()):
                    path.rmdir()

    return removed


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Delete translation files not present in each skin's xui/en/ folder.",
    )
    parser.add_argument(
        "--delete",
        action="store_true",
        help="Actually delete files (default is dry run).",
    )
    parser.add_argument(
        "--skin",
        action="append",
        metavar="NAME",
        help="Only process named skin(s); repeatable.",
    )
    args = parser.parse_args()

    wanted = set(args.skin) if args.skin else None
    total = 0

    for skin_dir in sorted(SKINS_ROOT.iterdir()):
        if not skin_dir.is_dir():
            continue
        if wanted and skin_dir.name not in wanted:
            continue
        total += prune_skin(skin_dir, delete=args.delete)

    mode = "deleted" if args.delete else "would delete"
    print(f"\n{total} file(s) {mode}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
