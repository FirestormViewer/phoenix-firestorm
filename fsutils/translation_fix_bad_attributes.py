#!/usr/bin/env python3
"""Remove or relocate locale XUI attributes that do not exist on matching EN elements."""

from __future__ import annotations

import re
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from translation_strings_auditing_tool import (  # noqa: E402
    SKINS_ROOT,
    collect_files,
    compare_extra_attributes,
    extract_element_attrs,
    parse_xml,
)

STRUCTURAL_ATTRS = frozenset({
    "relwidth", "follows", "font", "initial_value", "label_width", "vlabel",
    "unit_label",
})

ATTR_RE = r'(\s{attr}="[^"]*"|\s{attr}=\'[^\']*\')'
ATTR_RE_TMPL = r'(\s{attr}="[^"]*"|\s{attr}=\'[^\']*\')'


@dataclass
class FixAction:
    element: str
    attr: str
    value: str
    hint: str
    en_attrs: dict[str, str]


def attr_pattern(attr: str) -> re.Pattern[str]:
    return re.compile(
        rf'\s+{re.escape(attr)}="[^"]*"|\s+{re.escape(attr)}=\'[^\']*\'',
    )


def line_has_name(line: str, name: str) -> bool:
    return bool(re.search(rf'name\s*=\s*["\']{re.escape(name)}["\']', line))


def remove_attr(line: str, attr: str) -> str:
    return attr_pattern(attr).sub("", line, count=1)


def apply_line_fixes(line: str, actions: list[FixAction]) -> str:
    if not any(line_has_name(line, a.element[1:]) for a in actions):
        return line

    for action in actions:
        name = action.element[1:]
        if not line_has_name(line, name):
            continue

        attr = action.attr
        value = action.value
        hint = action.hint
        en_attrs = action.en_attrs

        if attr == "gnoretext":
            line = remove_attr(line, "gnoretext")
            if value and "ignoretext=" not in line:
                line = line.replace(
                    f'name="{name}"',
                    f'ignoretext="{value.replace(chr(34), "&quot;")}" name="{name}"',
                    1,
                )
            continue

        if attr == "tooltip" and "tool_tip" in hint:
            line = remove_attr(line, "tooltip")
            if value and "tool_tip=" not in line:
                line = line.replace(
                    f'name="{name}"',
                    f'tool_tip="{value.replace(chr(34), "&quot;")}" name="{name}"',
                    1,
                )
            continue

        if attr == "text" and "EN uses label" in hint:
            line = remove_attr(line, "text")
            continue

        if attr == "title" and "EN uses label" in hint:
            line = remove_attr(line, "title")
            if value and "label=" not in line:
                line = line.replace(
                    f'name="{name}"',
                    f'label="{value.replace(chr(34), "&quot;")}" name="{name}"',
                    1,
                )
            continue

        if attr == "label" and "EN uses title" in hint:
            line = remove_attr(line, "label")
            if value and "title=" not in line:
                line = line.replace(
                    f'name="{name}"',
                    f'title="{value.replace(chr(34), "&quot;")}" name="{name}"',
                    1,
                )
            continue

        if attr == "label" and "EN uses value" in hint:
            line = remove_attr(line, "label")
            if value and "value=" not in line:
                line = line.replace(
                    f'name="{name}"',
                    f'value="{value.replace(chr(34), "&quot;")}" name="{name}"',
                    1,
                )
            continue

        if attr == "ignoretext" and f'name="{name}"' in line and 'name="okcancelbuttons"' in line:
            line = line.replace('name="okcancelbuttons"', 'name="okcancelignore"', 1)
            continue

        if attr == "label" and value and "tool_tip" in en_attrs and "label" not in en_attrs:
            en_tip = en_attrs.get("tool_tip", "")
            if "tool_tip=" in line:
                tip_m = re.search(r'tool_tip="([^"]*)"', line)
                loc_tip = tip_m.group(1) if tip_m else ""
                if not loc_tip or loc_tip == en_tip:
                    line = re.sub(
                        r'tool_tip="[^"]*"',
                        f'tool_tip="{value.replace(chr(34), "&quot;")}"',
                        line,
                        count=1,
                    )
            else:
                line = line.replace(
                    f'name="{name}"',
                    f'tool_tip="{value.replace(chr(34), "&quot;")}" name="{name}"',
                    1,
                )
            line = remove_attr(line, "label")
            continue

        line = remove_attr(line, attr)

    return line


def fix_file_content(content: str, actions: list[FixAction]) -> tuple[str, int]:
    by_name: dict[str, list[FixAction]] = defaultdict(list)
    for action in actions:
        by_name[action.element[1:]].append(action)

    lines = content.splitlines(keepends=True)
    fixed = 0
    out: list[str] = []
    for line in lines:
        new_line = line
        for name, name_actions in by_name.items():
            if line_has_name(line, name):
                before = new_line
                new_line = apply_line_fixes(new_line, name_actions)
                if new_line != before:
                    fixed += len(name_actions)
        out.append(new_line)
    return "".join(out), fixed


def fix_file(skin: str, locale: str, rel: str, issues) -> int:
    locale_path = SKINS_ROOT / skin / "xui" / locale / rel
    en_path = SKINS_ROOT / skin / "xui" / "en" / rel
    en_root = parse_xml(en_path)
    if en_root is None or not locale_path.exists():
        return 0

    en_elems = extract_element_attrs(en_root, rel)
    actions = [
        FixAction(
            element=i.element,
            attr=i.attr,
            value=i.value,
            hint=i.hint,
            en_attrs=en_elems.get(i.element, {}),
        )
        for i in issues
    ]

    original = locale_path.read_text(encoding="utf-8")
    updated, count = fix_file_content(original, actions)
    if updated != original:
        locale_path.write_text(updated, encoding="utf-8", newline="")
    return count


def count_remaining() -> int:
    return len(collect_all_issues())


def collect_all_issues() -> list[tuple[str, str, str, object]]:
    issues: list[tuple[str, str, str, object]] = []
    skins = sorted(
        p.name for p in SKINS_ROOT.iterdir()
        if p.is_dir() and (p / "xui" / "en").is_dir()
    )
    for skin in skins:
        en_files = collect_files(SKINS_ROOT / skin / "xui" / "en")
        for loc_dir in sorted((SKINS_ROOT / skin / "xui").iterdir()):
            if not loc_dir.is_dir() or loc_dir.name == "en":
                continue
            locale = loc_dir.name
            loc_files = collect_files(loc_dir)
            for rel in sorted(set(en_files) & set(loc_files)):
                en_root = parse_xml(en_files[rel])
                loc_root = parse_xml(loc_files[rel])
                if en_root is None or loc_root is None:
                    continue
                file_issues = compare_extra_attributes(
                    extract_element_attrs(en_root, rel),
                    extract_element_attrs(loc_root, rel),
                )
                for issue in file_issues:
                    issues.append((skin, locale, rel, issue))
    return issues


def main() -> int:
    total_fixed = 0
    files_touched = 0

    grouped: dict[tuple[str, str, str], list] = defaultdict(list)
    for skin, locale, rel, issue in collect_all_issues():
        grouped[(skin, locale, rel)].append(issue)

    for (skin, locale, rel), issues in sorted(grouped.items()):
        count = fix_file(skin, locale, rel, issues)
        if count:
            files_touched += 1
            total_fixed += count
            print(f"fixed {count:3d}  {skin}/{locale}/{rel}")

    remaining = count_remaining()
    print(f"\nTotal attributes fixed: {total_fixed} in {files_touched} files")
    print(f"Remaining bad attributes: {remaining}")
    return 0 if remaining == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
