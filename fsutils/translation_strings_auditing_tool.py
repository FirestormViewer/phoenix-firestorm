#!/usr/bin/env python3

import html
import re
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
EN_DIR = REPO / "indra/newview/skins/default/xui/en"
PL_DIR = REPO / "indra/newview/skins/default/xui/pl"
OUT_FILE = REPO / "_audit_result.html"

TRANSLATABLE_ATTRS = frozenset({
    "label", "tool_tip", "title", "text", "yestext", "notext", "canceltext",
    "message", "help_text", "short_desc", "long_desc",
})

ONLY_PLACEHOLDERS_RE = re.compile(
    r"^(\[[A-Za-z0-9_,.:+\-]+\]|\$[A-Za-z_][A-Za-z0-9_]*|[ \t\r\n]+)+$",
)
PLACEHOLDER_WITH_PUNCT_RE = re.compile(
    r"^(\[[A-Za-z0-9_,.:+\-]+\][ \t]*)+[:.,;!?%°²³]*$",
)
AGE_OLD_RE = re.compile(
    r"^;?\s*(\[[A-Za-z0-9_,]+\][ \t]*)+old$", re.IGNORECASE,
)
DATE_FORMAT_RE = re.compile(
    r"^\[[^\]]+,datetime,[^\]]+\](/\[[^\]]+,datetime,[^\]]+\])+$",
)
INTEGER_RE = re.compile(r"^-?\d+$")
PUNCT_ONLY_RE = re.compile(r"^[\W_]+$")
SINGLE_LETTER_RE = re.compile(r"^[A-Za-z]:?$")
SLASH_COMMAND_RE = re.compile(r"^/[A-Za-z0-9!]+$")
KEYBOARD_LABELS = frozenset({
    "Alt", "Backsp", "CapsLock", "Ctrl", "Del", "Down", "End", "Enter", "Esc",
    "Home", "Ins", "Left", "Right", "Shift", "Up", "PgUp", "PgDn", "Space",
    "Tab", "KBCtrl", "KBShift", "AM", "PM", "Ok", "OK", "Mono", "uuid", "CPU",
    "Torus", "Metal", "Swap", "Start", "Stop", "Area", "Group", "Region", "Type",
    "Parcel", "Discord", "Flickr", "Marketplace", "Primfeed", "Chaplin",
})
BRAND_STRINGS = frozenset({
    "Firestorm", "FIRESTORM", "FS", "Second Life Grid", "APP_NAME", "APP_NAME_ABBR",
    "CAPITALIZED_APP_NAME", "CURRENT_GRID", "SECOND_LIFE", "DOWNLOAD_URL",
})
SKIP_FILES = frozenset({"xui_version.xml"})


@dataclass
class Entry:
    key: str
    value: str
    kind: str


@dataclass
class FileReport:
    rel: str
    missing: list[Entry] = field(default_factory=list)
    redundant: list[Entry] = field(default_factory=list)


def normalise_text(text: str) -> str:
    if text is None:
        return ""
    return " ".join(text.split())


def is_trivial(value: str, key: str = "") -> bool:
    v = normalise_text(value)
    if not v:
        return True
    if INTEGER_RE.match(v):
        return True
    if ONLY_PLACEHOLDERS_RE.match(v):
        return True
    if PLACEHOLDER_WITH_PUNCT_RE.match(v):
        return True
    if AGE_OLD_RE.match(v):
        return True
    if DATE_FORMAT_RE.match(v):
        return True
    if len(v) == 1:
        return True
    if SINGLE_LETTER_RE.match(v):
        return True
    if PUNCT_ONLY_RE.match(v):
        return True
    if SLASH_COMMAND_RE.match(v):
        return True
    if re.match(r"^F\d{1,2}$", v):
        return True
    if v in KEYBOARD_LABELS:
        return True
    if v.startswith(("PAD_BUTTON", "http://", "https://", "secondlife://")):
        return True
    if re.match(r"^[\d.]+%$", v):
        return True
    if re.match(r"^[a-z][a-z0-9_]*_panel$", v):
        return True
    if "TestString PleaseIgnore" in v:
        return True
    if key.startswith("string:"):
        name = key[7:]
        if name in BRAND_STRINGS or v in BRAND_STRINGS:
            return True
        if name.startswith("#"):
            return True
        if v.startswith("#"):
            return True
        if v.startswith("PAD_"):
            return True
        if v in {"(online)", "mainland", "L$", "IM"}:
            return True
        if key.startswith("string:font_"):
            return True
        if v.endswith("+") and len(v) <= 6:
            return True
    return False


def needs_pl_entry(value: str, key: str) -> bool:
    if is_trivial(value, key):
        return False
    if key.startswith("string:") and value == key[7:]:
        return False
    if key.endswith("::label"):
        name = key[1:].split("::", 1)[0]
        if value == name:
            return False
    return True


def element_identity(elem: ET.Element) -> str:
    name = elem.attrib.get("name", "").strip()
    if name:
        return name
    value = elem.attrib.get("value", "").strip()
    if value:
        return value
    label = elem.attrib.get("label", "").strip()
    if label:
        return label
    return ""


def entry_key(elem: ET.Element, attr: str) -> str:
    ident = element_identity(elem)
    if ident:
        return f"@{ident}::{attr}"
    tag = elem.tag.split("}")[-1] if "}" in elem.tag else elem.tag
    return f"{tag}::{attr}"


def extract_entries(root: ET.Element, rel: str) -> dict[str, Entry]:
    entries: dict[str, Entry] = {}

    if rel == "strings.xml":
        for child in root:
            tag = child.tag.split("}")[-1]
            if tag != "string":
                continue
            name = child.attrib.get("name", "").strip()
            if not name:
                continue
            value = normalise_text("".join(child.itertext()))
            key = f"string:{name}"
            entries[key] = Entry(key, value, "#text")
        return entries

    def walk(elem: ET.Element) -> None:
        tag = elem.tag.split("}")[-1] if "}" in elem.tag else elem.tag
        ident = element_identity(elem)

        for attr in TRANSLATABLE_ATTRS:
            if attr in elem.attrib:
                value = normalise_text(elem.attrib[attr])
                key = entry_key(elem, attr)
                entries[key] = Entry(key, value, attr)

        body = normalise_text("".join(elem.itertext()))
        if ident and body and tag in {
            "global", "notification", "text", "string", "ignore", "button",
        }:
            key = entry_key(elem, "#text")
            if key not in entries:
                entries[key] = Entry(key, body, "#text")

        for child in elem:
            walk(child)

    walk(root)
    return entries


def parse_xml(path: Path) -> ET.Element | None:
    try:
        return ET.parse(path).getroot()
    except ET.ParseError as exc:
        print(f"Parse error in {path}: {exc}")
        return None


def collect_files(base: Path) -> dict[str, Path]:
    files: dict[str, Path] = {}
    for path in base.rglob("*.xml"):
        rel = path.relative_to(base).as_posix()
        if rel not in SKIP_FILES:
            files[rel] = path
    return files


def compare_file(rel: str, en_path: Path, pl_path: Path | None) -> FileReport:
    report = FileReport(rel=rel)
    en_root = parse_xml(en_path)
    if en_root is None:
        return report

    en_all = extract_entries(en_root, rel)
    en_entries = {k: v for k, v in en_all.items() if needs_pl_entry(v.value, k)}

    if pl_path is None or not pl_path.exists():
        report.missing = list(en_entries.values())
        return report

    pl_root = parse_xml(pl_path)
    if pl_root is None:
        report.missing = list(en_entries.values())
        return report

    pl_all = extract_entries(pl_root, rel)
    pl_entries = {k: v for k, v in pl_all.items() if not is_trivial(v.value, k)}

    for key, entry in en_entries.items():
        if key not in pl_all:
            report.missing.append(entry)

    for key, entry in pl_entries.items():
        if key not in en_all:
            report.redundant.append(entry)

    return report


def render_html(
    en_count: int,
    pl_count: int,
    missing_files: list[str],
    redundant_files: list[str],
    file_reports: list[FileReport],
) -> str:
    total_missing = sum(len(r.missing) for r in file_reports)
    total_redundant = sum(len(r.redundant) for r in file_reports)
    files_with_missing = sum(1 for r in file_reports if r.missing)
    files_with_redundant = sum(1 for r in file_reports if r.redundant)

    parts = [
        "<!DOCTYPE html>",
        "<html lang='en'>",
        "<head>",
        "<meta charset='utf-8'>",
        "<title>PL XUI audit vs EN</title>",
        "<style>",
        "body{font-family:Segoe UI,Arial,sans-serif;margin:24px;line-height:1.45;color:#1a1a1a;}",
        "h1,h2,h3{margin-top:1.4em;}",
        ".summary table{border-collapse:collapse;margin:12px 0;}",
        ".summary td,.summary th{border:1px solid #ccc;padding:6px 12px;}",
        ".file{margin:24px 0;padding:12px 16px;border:1px solid #ddd;border-radius:6px;}",
        ".file h3{margin-top:0;font-family:Consolas,monospace;}",
        ".missing h4{color:#a11;}",
        ".redundant h4{color:#a60;}",
        "table.entries{width:100%;border-collapse:collapse;font-size:13px;}",
        "table.entries th,table.entries td{border:1px solid #ddd;padding:4px 8px;vertical-align:top;}",
        "table.entries th{background:#f5f5f5;text-align:left;}",
        "code{background:#f3f3f3;padding:1px 4px;border-radius:3px;}",
        ".note{color:#444;font-size:14px;}",
        "ul.files{column-count:2;}",
        "</style>",
        "</head>",
        "<body>",
        "<h1>XUI locale audit</h1>",
        "<p class='note'>Compared <code>indra/newview/skins/default/xui/pl/</code> against "
        "<code>indra/newview/skins/default/xui/en/</code> on 2026-06-13.</p>",
        "<p class='note'>Entries are matched by widget <code>name</code> (viewer layered-XML rules). "
        "Omitted as trivial: integers, placeholder-only text, keyboard labels, slash commands, "
        "font names, brand constants, and EN labels identical to the widget name.</p>",
        "<div class='summary'>",
        "<h2>Summary</h2>",
        "<table>",
        f"<tr><th>EN XML files</th><td>{en_count}</td></tr>",
        f"<tr><th>PL XML files</th><td>{pl_count}</td></tr>",
        f"<tr><th>Missing PL files</th><td>{len(missing_files)}</td></tr>",
        f"<tr><th>Redundant PL-only files</th><td>{len(redundant_files)}</td></tr>",
        f"<tr><th>Shared files with missing strings</th><td>{files_with_missing}</td></tr>",
        f"<tr><th>Shared files with redundant strings</th><td>{files_with_redundant}</td></tr>",
        f"<tr><th>Total missing entries</th><td>{total_missing}</td></tr>",
        f"<tr><th>Total redundant entries</th><td>{total_redundant}</td></tr>",
        "</table>",
        "</div>",
    ]

    if missing_files:
        parts.append("<h2>Missing translation files (present in EN, absent in PL)</h2><ul class='files'>")
        for rel in missing_files:
            parts.append(f"<li><code>{html.escape(rel)}</code></li>")
        parts.append("</ul>")

    if redundant_files:
        parts.append("<h2>Redundant PL-only files (not in EN)</h2><ul class='files'>")
        for rel in redundant_files:
            parts.append(f"<li><code>{html.escape(rel)}</code></li>")
        parts.append("</ul>")

    parts.append("<h2>Per-file string differences (shared files)</h2>")

    for report in file_reports:
        if not report.missing and not report.redundant:
            continue
        parts.append("<div class='file'>")
        parts.append(f"<h3>{html.escape(report.rel)}</h3>")

        if report.missing:
            parts.append("<div class='missing'><h4>Missing in PL</h4>")
            parts.append("<table class='entries'><tr><th>Key</th><th>EN value</th></tr>")
            for entry in sorted(report.missing, key=lambda e: e.key):
                parts.append(
                    f"<tr><td><code>{html.escape(entry.key)}</code></td>"
                    f"<td>{html.escape(entry.value)}</td></tr>"
                )
            parts.append("</table></div>")

        if report.redundant:
            parts.append("<div class='redundant'><h4>Redundant in PL (not in EN)</h4>")
            parts.append("<table class='entries'><tr><th>Key</th><th>PL value</th></tr>")
            for entry in sorted(report.redundant, key=lambda e: e.key):
                parts.append(
                    f"<tr><td><code>{html.escape(entry.key)}</code></td>"
                    f"<td>{html.escape(entry.value)}</td></tr>"
                )
            parts.append("</table></div>")

        parts.append("</div>")

    parts.extend(["</body>", "</html>"])
    return "\n".join(parts)


def main() -> None:
    en_files = collect_files(EN_DIR)
    pl_files = collect_files(PL_DIR)

    en_rels = set(en_files)
    pl_rels = set(pl_files)

    missing_files = sorted(en_rels - pl_rels)
    redundant_files = sorted(pl_rels - en_rels)

    file_reports: list[FileReport] = []
    for rel in sorted(en_rels & pl_rels):
        report = compare_file(rel, en_files[rel], pl_files.get(rel))
        if report.missing or report.redundant:
            file_reports.append(report)

    for rel in missing_files:
        en_root = parse_xml(en_files[rel])
        if en_root is None:
            continue
        en_all = extract_entries(en_root, rel)
        entries = [e for e in en_all.values() if needs_pl_entry(e.value, e.key)]
        if entries:
            file_reports.append(FileReport(rel=rel, missing=entries))

    file_reports.sort(key=lambda r: r.rel)
    output = render_html(
        len(en_files), len(pl_files), missing_files, redundant_files, file_reports,
    )
    OUT_FILE.write_text(output, encoding="utf-8")
    print(f"Wrote {OUT_FILE}")


if __name__ == "__main__":
    main()
