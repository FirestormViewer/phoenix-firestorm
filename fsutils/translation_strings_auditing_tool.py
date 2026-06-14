#!/usr/bin/env python3

import html
import re
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from datetime import date
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SKINS_ROOT = REPO / "indra/newview/skins"
OUT_FILE = REPO / "_audit_result.html"

TRANSLATABLE_ATTRS = frozenset({
    "label", "tool_tip", "title", "text", "yestext", "notext", "canceltext",
    "message", "help_text", "short_desc", "long_desc", "value",
})

NUMERIC_VALUE_RE = re.compile(r"^-?\d+(\.\d+)?$")
PLACEHOLDER_TOKEN_RE = re.compile(r"\[[^\]]+\]|\$[A-Za-z_][A-Za-z0-9_]*")

STRING_BODY_TAGS = frozenset({
    "global", "notification", "text", "string", "ignore", "button",
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
TRANSLATE_FALSE_VALUES = frozenset({"false", "0", "no"})

# Common misspellings of real XUI translation attributes.
ATTR_TYPO_HINTS: dict[str, str] = {
    "tooltip": "tool_tip",
    "toolTip": "tool_tip",
    "tool-tip": "tool_tip",
    "ToolTip": "tool_tip",
    "Tooltip": "tool_tip",
    "helptext": "help_text",
    "helpText": "help_text",
    "shortdesc": "short_desc",
    "longdesc": "long_desc",
    "shortDesc": "short_desc",
    "longDesc": "long_desc",
    "yes_text": "yestext",
    "no_text": "notext",
    "cancel_text": "canceltext",
}

TRANSLATION_ATTR_KEYWORDS = (
    "tip", "label", "title", "text", "message", "desc", "help",
)

ATTR_EN_ALTERNATES: dict[str, tuple[str, ...]] = {
    "label": ("value", "title"),
    "title": ("label", "value"),
    "value": ("label",),
    "text": ("label", "value"),
    "tool_tip": ("help_text",),
    "help_text": ("tool_tip",),
}


@dataclass
class Entry:
    key: str
    value: str
    kind: str


@dataclass
class AttrIssue:
    element: str
    attr: str
    value: str
    hint: str = ""


@dataclass
class PlaceholderIssue:
    key: str
    en_value: str
    locale_value: str
    missing: list[str] = field(default_factory=list)
    extra: list[str] = field(default_factory=list)


@dataclass
class ParseErrorRecord:
    skin: str
    locale: str
    rel: str
    message: str


@dataclass
class DuplicateNameRecord:
    rel: str
    name: str
    count: int


@dataclass
class FileReport:
    rel: str
    missing: list[Entry] = field(default_factory=list)
    redundant: list[Entry] = field(default_factory=list)
    extra_attrs: list[AttrIssue] = field(default_factory=list)
    unchanged: list[Entry] = field(default_factory=list)
    placeholder_issues: list[PlaceholderIssue] = field(default_factory=list)
    locale_only_elements: list[str] = field(default_factory=list)

    @property
    def has_issues(self) -> bool:
        return bool(
            self.missing or self.redundant or self.extra_attrs
            or self.unchanged or self.placeholder_issues
            or self.locale_only_elements,
        )


@dataclass
class LocaleReport:
    locale: str
    en_count: int
    locale_count: int
    missing_files: list[str] = field(default_factory=list)
    redundant_files: list[str] = field(default_factory=list)
    file_reports: list[FileReport] = field(default_factory=list)

    @property
    def total_missing(self) -> int:
        return sum(len(r.missing) for r in self.file_reports)

    @property
    def total_redundant(self) -> int:
        return sum(len(r.redundant) for r in self.file_reports)

    @property
    def total_extra_attrs(self) -> int:
        return sum(len(r.extra_attrs) for r in self.file_reports)

    @property
    def total_unchanged(self) -> int:
        return sum(len(r.unchanged) for r in self.file_reports)

    @property
    def total_placeholder_issues(self) -> int:
        return sum(len(r.placeholder_issues) for r in self.file_reports)

    @property
    def total_locale_only_elements(self) -> int:
        return sum(len(r.locale_only_elements) for r in self.file_reports)

    @property
    def files_with_missing(self) -> int:
        return sum(1 for r in self.file_reports if r.missing)

    @property
    def files_with_redundant(self) -> int:
        return sum(1 for r in self.file_reports if r.redundant)

    @property
    def files_with_extra_attrs(self) -> int:
        return sum(1 for r in self.file_reports if r.extra_attrs)

    @property
    def has_issues(self) -> bool:
        return bool(
            self.missing_files or self.redundant_files
            or any(r.has_issues for r in self.file_reports),
        )


@dataclass
class SkinReport:
    skin: str
    locale_reports: list[LocaleReport] = field(default_factory=list)
    duplicate_names_by_file: dict[str, list[DuplicateNameRecord]] = field(
        default_factory=dict,
    )

    @property
    def en_count(self) -> int:
        return self.locale_reports[0].en_count if self.locale_reports else 0

    @property
    def total_duplicate_names(self) -> int:
        return sum(len(v) for v in self.duplicate_names_by_file.values())


@dataclass
class AuditReport:
    skin_reports: list[SkinReport] = field(default_factory=list)
    parse_errors: list[ParseErrorRecord] = field(default_factory=list)


def normalise_text(text: str) -> str:
    if text is None:
        return ""
    return " ".join(text.split())


def is_numeric_value(value: str) -> bool:
    v = normalise_text(value)
    return bool(v) and bool(NUMERIC_VALUE_RE.match(v))


def tag_name(elem: ET.Element) -> str:
    return elem.tag.split("}")[-1] if "}" in elem.tag else elem.tag


def tag_has_string_body(tag: str) -> bool:
    return tag in STRING_BODY_TAGS or tag.endswith(".string")


def direct_element_text(elem: ET.Element) -> str:
    return normalise_text(elem.text or "")


def string_content(elem: ET.Element) -> str:
    body = direct_element_text(elem)
    if body:
        return body
    return normalise_text(elem.attrib.get("value", ""))


def extract_placeholders(text: str) -> frozenset[str]:
    return frozenset(PLACEHOLDER_TOKEN_RE.findall(text))


def is_translatable_attr_value(attr: str, value: str) -> bool:
    if attr == "value" and is_numeric_value(value):
        return False
    return True


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


def needs_translation_entry(value: str, key: str) -> bool:
    if is_trivial(value, key):
        return False
    if key.startswith("string:") and value == key[7:]:
        return False
    if key.endswith("::label"):
        name = key[1:].split("::", 1)[0]
        if value == name:
            return False
    if key.endswith("::value") and is_numeric_value(value):
        return False
    return True


def find_duplicate_names(root: ET.Element) -> list[DuplicateNameRecord]:
    counts: dict[str, int] = {}
    for elem in root.iter():
        name = elem.attrib.get("name", "").strip()
        if name:
            counts[name] = counts.get(name, 0) + 1
    return [
        DuplicateNameRecord(name=name, count=count, rel="")
        for name, count in sorted(counts.items())
        if count > 1
    ]


def element_has_translatable_content(
    ekey: str,
    attrs: dict[str, str],
    entries: dict[str, Entry],
) -> bool:
    prefix = f"{ekey}::"
    for key, entry in entries.items():
        if key.startswith(prefix) and needs_translation_entry(entry.value, key):
            return True
    for attr, value in attrs.items():
        if not is_translatable_attr_value(attr, value):
            continue
        key = f"{ekey}::{attr}"
        if needs_translation_entry(value, key):
            return True
    return False


def is_translate_disabled(elem: ET.Element) -> bool:
    return elem.attrib.get("translate", "").strip().lower() in TRANSLATE_FALSE_VALUES


def collect_no_translate_targets(root: ET.Element, rel: str) -> frozenset[str]:
    targets: set[str] = set()

    if rel == "strings.xml":
        for child in root:
            tag = child.tag.split("}")[-1]
            if tag != "string":
                continue
            if not is_translate_disabled(child):
                continue
            name = child.attrib.get("name", "").strip()
            if name:
                targets.add(f"string:{name}")
        return frozenset(targets)

    def walk(elem: ET.Element) -> None:
        ekey = element_map_key(elem)
        if ekey and is_translate_disabled(elem):
            targets.add(ekey)
        for child in elem:
            walk(child)

    walk(root)
    return frozenset(targets)


def entry_is_non_translatable(key: str, no_translate: frozenset[str]) -> bool:
    if key in no_translate:
        return True
    if key.startswith("@"):
        element = key.split("::", 1)[0]
        return element in no_translate
    return False


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


def element_map_key(elem: ET.Element) -> str | None:
    ident = element_identity(elem)
    if ident:
        return f"@{ident}"
    return None


def is_xml_meta_attr(attr: str) -> bool:
    return attr.startswith("{") or attr.startswith("xmlns")


def looks_translation_attr(attr: str) -> bool:
    if attr in TRANSLATABLE_ATTRS or attr in ATTR_TYPO_HINTS:
        return True
    lower = attr.lower()
    return any(keyword in lower for keyword in TRANSLATION_ATTR_KEYWORDS)


def should_report_extra_attr(attr: str, value: str, en_attrs: dict[str, str]) -> bool:
    if attr == "value" and is_numeric_value(value):
        return False
    if attr in ATTR_TYPO_HINTS:
        return True
    if attr in TRANSLATABLE_ATTRS:
        return True
    if looks_translation_attr(attr):
        return True
    key = f"@extra::{attr}"
    return not is_trivial(value, key)


def extra_attr_hint(attr: str, en_attrs: dict[str, str]) -> str:
    canonical = ATTR_TYPO_HINTS.get(attr)
    if canonical and canonical in en_attrs:
        return f"EN uses {canonical} on this element"
    if canonical:
        return f"Did you mean {canonical}?"

    for alternate in ATTR_EN_ALTERNATES.get(attr, ()):
        if alternate in en_attrs:
            return f"EN uses {alternate} on this element, not {attr}"

    return "Attribute not present on EN element"


def attr_entry_key(element: str, attr: str) -> str:
    return f"{element}::{attr}"


def extract_element_attrs(root: ET.Element, rel: str) -> dict[str, dict[str, str]]:
    elements: dict[str, dict[str, str]] = {}

    if rel == "strings.xml":
        return elements

    def walk(elem: ET.Element) -> None:
        ekey = element_map_key(elem)
        if ekey:
            bucket = elements.setdefault(ekey, {})
            for attr, value in elem.attrib.items():
                if is_xml_meta_attr(attr):
                    continue
                bucket[attr] = normalise_text(value)
        for child in elem:
            walk(child)

    walk(root)
    return elements


def compare_extra_attributes(
    en_elements: dict[str, dict[str, str]],
    locale_elements: dict[str, dict[str, str]],
    skip_keys: frozenset[str] | None = None,
    skip_elements: frozenset[str] | None = None,
) -> list[AttrIssue]:
    issues: list[AttrIssue] = []
    skip = skip_keys or frozenset()
    skip_elems = skip_elements or frozenset()

    for ekey, locale_attrs in locale_elements.items():
        if ekey in skip_elems:
            continue
        en_attrs = en_elements.get(ekey)
        if en_attrs is None:
            continue
        for attr, value in locale_attrs.items():
            key = attr_entry_key(ekey, attr)
            if key in skip:
                continue
            if attr in en_attrs:
                continue
            if not should_report_extra_attr(attr, value, en_attrs):
                continue
            issues.append(AttrIssue(
                element=ekey,
                attr=attr,
                value=value,
                hint=extra_attr_hint(attr, en_attrs),
            ))

    issues.sort(key=lambda i: (i.element, i.attr))
    return issues


def extract_entries(root: ET.Element, rel: str) -> dict[str, Entry]:
    entries: dict[str, Entry] = {}

    if rel == "strings.xml":
        for child in root:
            if tag_name(child) != "string":
                continue
            if is_translate_disabled(child):
                continue
            name = child.attrib.get("name", "").strip()
            if not name:
                continue
            value = string_content(child)
            key = f"string:{name}"
            entries[key] = Entry(key, value, "#text")
        return entries

    def walk(elem: ET.Element) -> None:
        tag = tag_name(elem)
        ident = element_identity(elem)

        if not is_translate_disabled(elem):
            for attr in TRANSLATABLE_ATTRS:
                if attr not in elem.attrib:
                    continue
                value = normalise_text(elem.attrib[attr])
                if not is_translatable_attr_value(attr, value):
                    continue
                key = entry_key(elem, attr)
                entries[key] = Entry(key, value, attr)

            body = direct_element_text(elem)
            if ident and body and tag_has_string_body(tag):
                key = entry_key(elem, "#text")
                if key not in entries:
                    entries[key] = Entry(key, body, "#text")

        for child in elem:
            walk(child)

    walk(root)
    return entries


def parse_xml(
    path: Path,
    parse_errors: list[ParseErrorRecord] | None = None,
    skin: str = "",
    locale: str = "",
    rel: str = "",
) -> ET.Element | None:
    try:
        return ET.parse(path).getroot()
    except ET.ParseError as exc:
        message = str(exc)
        print(f"Parse error in {path}: {exc}")
        if parse_errors is not None:
            parse_errors.append(ParseErrorRecord(
                skin=skin, locale=locale, rel=rel or path.name, message=message,
            ))
        return None


def collect_files(base: Path) -> dict[str, Path]:
    files: dict[str, Path] = {}
    if not base.is_dir():
        return files
    for path in base.rglob("*.xml"):
        rel = path.relative_to(base).as_posix()
        if rel not in SKIP_FILES:
            files[rel] = path
    return files


def compare_file(
    rel: str,
    en_path: Path,
    locale_path: Path | None,
    parse_errors: list[ParseErrorRecord] | None = None,
    skin: str = "",
    locale: str = "",
) -> FileReport:
    report = FileReport(rel=rel)
    en_root = parse_xml(
        en_path, parse_errors, skin=skin, locale="en", rel=rel,
    )
    if en_root is None:
        return report

    en_all = extract_entries(en_root, rel)
    en_no_translate = collect_no_translate_targets(en_root, rel)
    en_entries = {
        k: v for k, v in en_all.items()
        if needs_translation_entry(v.value, k)
        and not entry_is_non_translatable(k, en_no_translate)
    }

    if locale_path is None or not locale_path.exists():
        report.missing = list(en_entries.values())
        return report

    locale_root = parse_xml(
        locale_path, parse_errors, skin=skin, locale=locale, rel=rel,
    )
    if locale_root is None:
        report.missing = list(en_entries.values())
        return report

    locale_all = extract_entries(locale_root, rel)
    locale_entries = {
        k: v for k, v in locale_all.items()
        if needs_translation_entry(v.value, k)
    }

    for key, entry in en_entries.items():
        if key not in locale_all:
            report.missing.append(entry)
            continue
        loc_entry = locale_all[key]
        if loc_entry.value == entry.value:
            report.unchanged.append(entry)
            continue
        en_ph = extract_placeholders(entry.value)
        if en_ph:
            loc_ph = extract_placeholders(loc_entry.value)
            missing_ph = sorted(en_ph - loc_ph)
            extra_ph = sorted(loc_ph - en_ph)
            if missing_ph or extra_ph:
                report.placeholder_issues.append(PlaceholderIssue(
                    key=key,
                    en_value=entry.value,
                    locale_value=loc_entry.value,
                    missing=missing_ph,
                    extra=extra_ph,
                ))

    for key, entry in locale_entries.items():
        if key not in en_all:
            if entry_is_non_translatable(key, en_no_translate):
                continue
            report.redundant.append(entry)

    redundant_keys = frozenset(e.key for e in report.redundant)
    en_elements = extract_element_attrs(en_root, rel)
    locale_elements = extract_element_attrs(locale_root, rel)
    report.extra_attrs = compare_extra_attributes(
        en_elements,
        locale_elements,
        skip_keys=redundant_keys,
        skip_elements=en_no_translate,
    )

    for ekey in sorted(locale_elements):
        if ekey in en_elements:
            continue
        if ekey in en_no_translate:
            continue
        if element_has_translatable_content(
            ekey, locale_elements[ekey], locale_all,
        ):
            report.locale_only_elements.append(ekey)

    return report


def discover_skins() -> list[str]:
    skins: list[str] = []
    if not SKINS_ROOT.is_dir():
        return skins
    for path in sorted(SKINS_ROOT.iterdir()):
        if path.is_dir() and (path / "xui" / "en").is_dir():
            skins.append(path.name)
    return skins


def discover_locales(skin: str) -> list[str]:
    xui = SKINS_ROOT / skin / "xui"
    if not xui.is_dir():
        return []
    return sorted(
        d.name for d in xui.iterdir()
        if d.is_dir() and d.name != "en"
    )


def audit_locale(
    skin: str,
    locale: str,
    parse_errors: list[ParseErrorRecord],
) -> LocaleReport:
    en_dir = SKINS_ROOT / skin / "xui" / "en"
    locale_dir = SKINS_ROOT / skin / "xui" / locale

    en_files = collect_files(en_dir)
    locale_files = collect_files(locale_dir)

    en_rels = set(en_files)
    locale_rels = set(locale_files)

    report = LocaleReport(
        locale=locale,
        en_count=len(en_files),
        locale_count=len(locale_files),
        missing_files=sorted(en_rels - locale_rels),
        redundant_files=sorted(locale_rels - en_rels),
    )

    for rel in sorted(en_rels & locale_rels):
        file_report = compare_file(
            rel, en_files[rel], locale_files.get(rel),
            parse_errors=parse_errors, skin=skin, locale=locale,
        )
        if file_report.has_issues:
            report.file_reports.append(file_report)

    for rel in report.missing_files:
        en_root = parse_xml(
            en_files[rel], parse_errors, skin=skin, locale="en", rel=rel,
        )
        if en_root is None:
            continue
        en_all = extract_entries(en_root, rel)
        en_no_translate = collect_no_translate_targets(en_root, rel)
        missing = [
            e for e in en_all.values()
            if needs_translation_entry(e.value, e.key)
            and not entry_is_non_translatable(e.key, en_no_translate)
        ]
        if missing:
            report.file_reports.append(FileReport(rel=rel, missing=missing))

    report.file_reports.sort(key=lambda r: r.rel)
    return report


def scan_en_duplicate_names(
    skin: str,
    parse_errors: list[ParseErrorRecord],
) -> dict[str, list[DuplicateNameRecord]]:
    en_dir = SKINS_ROOT / skin / "xui" / "en"
    by_file: dict[str, list[DuplicateNameRecord]] = {}
    for rel, path in collect_files(en_dir).items():
        root = parse_xml(path, parse_errors, skin=skin, locale="en", rel=rel)
        if root is None:
            continue
        dupes = find_duplicate_names(root)
        if dupes:
            by_file[rel] = [
                DuplicateNameRecord(rel=rel, name=d.name, count=d.count)
                for d in dupes
            ]
    return by_file


def audit_skin(
    skin: str,
    parse_errors: list[ParseErrorRecord],
) -> SkinReport:
    locales = discover_locales(skin)
    return SkinReport(
        skin=skin,
        locale_reports=[
            audit_locale(skin, loc, parse_errors) for loc in locales
        ],
        duplicate_names_by_file=scan_en_duplicate_names(skin, parse_errors),
    )


def badge(count: int, kind: str) -> str:
    if count == 0:
        return f"<span class='badge badge-ok'>{count}</span>"
    return f"<span class='badge badge-{kind}'>{count}</span>"


def render_file_report(report: FileReport, locale: str) -> str:
    parts = [
        "<div class='file'>",
        "<h4 class='file-heading'>",
        f"<code>{html.escape(report.rel)}</code>",
    ]
    if report.missing:
        parts.append(badge(len(report.missing), "missing"))
    if report.redundant:
        parts.append(badge(len(report.redundant), "redundant"))
    if report.extra_attrs:
        parts.append(badge(len(report.extra_attrs), "extra-attr"))
    if report.unchanged:
        parts.append(badge(len(report.unchanged), "unchanged"))
    if report.placeholder_issues:
        parts.append(badge(len(report.placeholder_issues), "placeholder"))
    if report.locale_only_elements:
        parts.append(badge(len(report.locale_only_elements), "locale-only"))
    parts.append("</h4><div class='file-body'>")

    if report.missing:
        parts.append("<div class='missing'>")
        parts.append(f"<h5>Missing in {html.escape(locale)}</h5>")
        parts.append(
            "<table class='entries'><tr><th>Key</th><th>EN value</th></tr>",
        )
        for entry in sorted(report.missing, key=lambda e: e.key):
            parts.append(
                f"<tr><td><code>{html.escape(entry.key)}</code></td>"
                f"<td>{html.escape(entry.value)}</td></tr>",
            )
        parts.append("</table></div>")

    if report.redundant:
        parts.append("<div class='redundant'>")
        parts.append(f"<h5>Redundant in {html.escape(locale)} (not in EN)</h5>")
        parts.append(
            "<table class='entries'><tr><th>Key</th>"
            f"<th>{html.escape(locale)} value</th></tr>",
        )
        for entry in sorted(report.redundant, key=lambda e: e.key):
            parts.append(
                f"<tr><td><code>{html.escape(entry.key)}</code></td>"
                f"<td>{html.escape(entry.value)}</td></tr>",
            )
        parts.append("</table></div>")

    if report.extra_attrs:
        parts.append("<div class='extra-attr'>")
        parts.append(
            f"<h5>Attributes on {html.escape(locale)} elements not in EN "
            "(wrong name or typo)</h5>",
        )
        parts.append(
            "<table class='entries'><tr><th>Element</th><th>Attribute</th>"
            f"<th>{html.escape(locale)} value</th><th>Note</th></tr>",
        )
        for issue in report.extra_attrs:
            display_value = issue.value if issue.value else "(empty)"
            parts.append(
                f"<tr><td><code>{html.escape(issue.element)}</code></td>"
                f"<td><code>{html.escape(issue.attr)}</code></td>"
                f"<td>{html.escape(display_value)}</td>"
                f"<td>{html.escape(issue.hint)}</td></tr>",
            )
        parts.append("</table></div>")

    if report.unchanged:
        parts.append("<div class='unchanged'>")
        parts.append(
            f"<h5>Identical to EN in {html.escape(locale)} (likely untranslated)</h5>",
        )
        parts.append(
            "<table class='entries'><tr><th>Key</th><th>Value</th></tr>",
        )
        for entry in sorted(report.unchanged, key=lambda e: e.key):
            parts.append(
                f"<tr><td><code>{html.escape(entry.key)}</code></td>"
                f"<td>{html.escape(entry.value)}</td></tr>",
            )
        parts.append("</table></div>")

    if report.placeholder_issues:
        parts.append("<div class='placeholder'>")
        parts.append(f"<h5>Placeholder mismatch in {html.escape(locale)}</h5>")
        parts.append(
            "<table class='entries'><tr><th>Key</th><th>EN value</th>"
            f"<th>{html.escape(locale)} value</th><th>Missing</th><th>Extra</th></tr>",
        )
        for issue in sorted(report.placeholder_issues, key=lambda i: i.key):
            parts.append(
                f"<tr><td><code>{html.escape(issue.key)}</code></td>"
                f"<td>{html.escape(issue.en_value)}</td>"
                f"<td>{html.escape(issue.locale_value)}</td>"
                f"<td>{html.escape(', '.join(issue.missing))}</td>"
                f"<td>{html.escape(', '.join(issue.extra))}</td></tr>",
            )
        parts.append("</table></div>")

    if report.locale_only_elements:
        parts.append("<div class='locale-only'>")
        parts.append(
            f"<h5>Elements in {html.escape(locale)} not present in EN file</h5>",
        )
        parts.append("<ul class='files'>")
        for ekey in report.locale_only_elements:
            parts.append(f"<li><code>{html.escape(ekey)}</code></li>")
        parts.append("</ul></div>")

    parts.append("</div></div>")
    return "\n".join(parts)


def render_locale_report(locale_report: LocaleReport) -> str:
    loc = locale_report.locale
    parts = [
        "<details class='locale'>",
        "<summary class='locale-summary'>",
    ]
    parts.append(f"<span class='locale-code'>{html.escape(loc)}</span>")
    parts.append(
        f"<span class='locale-meta'>{locale_report.locale_count} files vs "
        f"{locale_report.en_count} EN</span>",
    )
    if locale_report.missing_files:
        parts.append(
            f"<span class='stat'>missing files: "
            f"{badge(len(locale_report.missing_files), 'missing')}</span>",
        )
    if locale_report.redundant_files:
        parts.append(
            f"<span class='stat'>redundant files: "
            f"{badge(len(locale_report.redundant_files), 'redundant')}</span>",
        )
    parts.append(
        f"<span class='stat'>missing strings: "
        f"{badge(locale_report.total_missing, 'missing')}</span>",
    )
    parts.append(
        f"<span class='stat'>redundant strings: "
        f"{badge(locale_report.total_redundant, 'redundant')}</span>",
    )
    if locale_report.total_extra_attrs:
        parts.append(
            f"<span class='stat'>bad attributes: "
            f"{badge(locale_report.total_extra_attrs, 'extra-attr')}</span>",
        )
    if locale_report.total_unchanged:
        parts.append(
            f"<span class='stat'>unchanged: "
            f"{badge(locale_report.total_unchanged, 'unchanged')}</span>",
        )
    if locale_report.total_placeholder_issues:
        parts.append(
            f"<span class='stat'>placeholders: "
            f"{badge(locale_report.total_placeholder_issues, 'placeholder')}</span>",
        )
    if locale_report.total_locale_only_elements:
        parts.append(
            f"<span class='stat'>locale-only widgets: "
            f"{badge(locale_report.total_locale_only_elements, 'locale-only')}</span>",
        )
    parts.append("</summary>")
    parts.append("<div class='locale-body'>")

    parts.append("<table class='mini-summary'>")
    parts.append("<tr>")
    for label, value in (
        ("EN XML files", locale_report.en_count),
        (f"{loc.upper()} XML files", locale_report.locale_count),
        ("Missing files", len(locale_report.missing_files)),
        ("Redundant files", len(locale_report.redundant_files)),
        ("Files with missing strings", locale_report.files_with_missing),
        ("Files with redundant strings", locale_report.files_with_redundant),
        ("Total missing entries", locale_report.total_missing),
        ("Total redundant entries", locale_report.total_redundant),
        ("Bad attributes (not on EN element)", locale_report.total_extra_attrs),
        ("Files with bad attributes", locale_report.files_with_extra_attrs),
        ("Unchanged (same as EN)", locale_report.total_unchanged),
        ("Placeholder mismatches", locale_report.total_placeholder_issues),
        ("Locale-only widgets", locale_report.total_locale_only_elements),
    ):
        parts.append(
            f"<tr><th>{html.escape(label)}</th><td>{value}</td></tr>",
        )
    parts.append("</table>")

    if locale_report.missing_files:
        parts.append(
            f"<h4 class='subsection-heading'>Missing translation files "
            f"({len(locale_report.missing_files)})</h4>",
        )
        parts.append("<ul class='files'>")
        for rel in locale_report.missing_files:
            parts.append(f"<li><code>{html.escape(rel)}</code></li>")
        parts.append("</ul>")

    if locale_report.redundant_files:
        parts.append(
            f"<h4 class='subsection-heading'>Redundant locale-only files "
            f"({len(locale_report.redundant_files)})</h4>",
        )
        parts.append("<ul class='files'>")
        for rel in locale_report.redundant_files:
            parts.append(f"<li><code>{html.escape(rel)}</code></li>")
        parts.append("</ul>")

    if locale_report.file_reports:
        parts.append(
            f"<h4 class='subsection-heading'>Per-file string differences "
            f"({len(locale_report.file_reports)})</h4>",
        )
        parts.append("<div class='file-reports'>")
        for file_report in locale_report.file_reports:
            parts.append(render_file_report(file_report, loc))
        parts.append("</div>")
    elif not locale_report.missing_files and not locale_report.redundant_files:
        parts.append("<p class='all-clear'>No string differences found.</p>")

    parts.append("</div></details>")
    return "\n".join(parts)


def render_skin_panel(skin_report: SkinReport, active: bool) -> str:
    skin = skin_report.skin
    visible = "" if active else " hidden"
    parts = [
        f"<section class='skin-panel{visible}' id='skin-{html.escape(skin)}' "
        f"data-skin='{html.escape(skin)}'>",
        f"<h2>Skin: <code>{html.escape(skin)}</code></h2>",
        f"<p class='path-note'>Base: "
        f"<code>indra/newview/skins/{html.escape(skin)}/xui/</code></p>",
    ]

    if not skin_report.locale_reports:
        parts.append(
            "<p class='note'>No non-EN locales found for this skin.</p>",
        )
    else:
        if skin_report.duplicate_names_by_file:
            total_dupes = skin_report.total_duplicate_names
            parts.append(
                f"<h4 class='subsection-heading'>Duplicate widget names in EN "
                f"({total_dupes} across {len(skin_report.duplicate_names_by_file)} files)</h4>",
            )
            parts.append(
                "<p class='note'>Multiple elements share the same <code>name</code> "
                "in one file; string matching may be unreliable for these.</p>",
            )
            parts.append("<table class='entries'>")
            parts.append("<tr><th>File</th><th>Widget name</th><th>Count</th></tr>")
            for rel in sorted(skin_report.duplicate_names_by_file):
                for dup in skin_report.duplicate_names_by_file[rel]:
                    parts.append(
                        f"<tr><td><code>{html.escape(rel)}</code></td>"
                        f"<td><code>{html.escape(dup.name)}</code></td>"
                        f"<td>{dup.count}</td></tr>",
                    )
            parts.append("</table>")

        for locale_report in skin_report.locale_reports:
            parts.append(render_locale_report(locale_report))

    parts.append("</section>")
    return "\n".join(parts)


def render_matrix(skin_reports: list[SkinReport]) -> str:
    all_locales: list[str] = []
    seen: set[str] = set()
    for skin_report in skin_reports:
        for locale_report in skin_report.locale_reports:
            if locale_report.locale not in seen:
                seen.add(locale_report.locale)
                all_locales.append(locale_report.locale)

    if not all_locales:
        return ""

    lookup: dict[tuple[str, str], LocaleReport] = {}
    for skin_report in skin_reports:
        for locale_report in skin_report.locale_reports:
            lookup[(skin_report.skin, locale_report.locale)] = locale_report

    parts = [
        "<h2>Overview matrix</h2>",
        "<p class='note'>Missing string counts per skin and locale. "
        "Click a skin tab for detail.</p>",
        "<div class='matrix-wrap'><table class='matrix'>",
        "<tr><th>Skin</th>",
    ]
    for loc in all_locales:
        parts.append(f"<th>{html.escape(loc)}</th>")
    parts.append("</tr>")

    for skin_report in skin_reports:
        parts.append(f"<tr><th><code>{html.escape(skin_report.skin)}</code></th>")
        for loc in all_locales:
            locale_report = lookup.get((skin_report.skin, loc))
            if locale_report is None:
                parts.append("<td class='na'>-</td>")
            elif not locale_report.has_issues:
                parts.append("<td class='ok'>0</td>")
            else:
                missing = locale_report.total_missing
                extra = (
                    locale_report.total_redundant
                    + locale_report.total_extra_attrs
                    + locale_report.total_unchanged
                    + locale_report.total_placeholder_issues
                    + locale_report.total_locale_only_elements
                )
                cell = f"{missing}"
                if extra:
                    cell += f" <span class='redundant-hint'>(+{extra})</span>"
                parts.append(f"<td class='issue'>{cell}</td>")
        parts.append("</tr>")

    parts.append("</table></div>")
    return "\n".join(parts)


def render_parse_errors(parse_errors: list[ParseErrorRecord]) -> str:
    if not parse_errors:
        return ""
    parts = [
        "<h2>XML parse errors</h2>",
        "<p class='note'>These files could not be parsed and were skipped "
        "for that locale comparison.</p>",
        "<table class='entries'>",
        "<tr><th>Skin</th><th>Locale</th><th>File</th><th>Error</th></tr>",
    ]
    for err in parse_errors:
        parts.append(
            f"<tr><td><code>{html.escape(err.skin)}</code></td>"
            f"<td>{html.escape(err.locale)}</td>"
            f"<td><code>{html.escape(err.rel)}</code></td>"
            f"<td>{html.escape(err.message)}</td></tr>",
        )
    parts.append("</table>")
    return "\n".join(parts)


def locale_issue_total(locale_report: LocaleReport) -> int:
    return (
        locale_report.total_missing
        + locale_report.total_redundant
        + locale_report.total_extra_attrs
        + locale_report.total_unchanged
        + locale_report.total_placeholder_issues
        + locale_report.total_locale_only_elements
    )


def skin_issue_total(skin_report: SkinReport) -> int:
    return (
        sum(locale_issue_total(lr) for lr in skin_report.locale_reports)
        + skin_report.total_duplicate_names
    )


def render_html(audit: AuditReport) -> str:
    skin_reports = audit.skin_reports
    audit_date = date.today().isoformat()
    total_missing = sum(
        lr.total_missing for sr in skin_reports for lr in sr.locale_reports
    )
    total_redundant = sum(
        lr.total_redundant for sr in skin_reports for lr in sr.locale_reports
    )
    total_extra_attrs = sum(
        lr.total_extra_attrs for sr in skin_reports for lr in sr.locale_reports
    )
    total_unchanged = sum(
        lr.total_unchanged for sr in skin_reports for lr in sr.locale_reports
    )
    total_placeholder = sum(
        lr.total_placeholder_issues for sr in skin_reports for lr in sr.locale_reports
    )
    total_locale_only = sum(
        lr.total_locale_only_elements for sr in skin_reports for lr in sr.locale_reports
    )
    total_duplicate_names = sum(sr.total_duplicate_names for sr in skin_reports)
    locale_count = sum(len(sr.locale_reports) for sr in skin_reports)
    skins_with_locales = sum(1 for sr in skin_reports if sr.locale_reports)

    parts = [
        "<!DOCTYPE html>",
        "<html lang='en'>",
        "<head>",
        "<meta charset='utf-8'>",
        "<meta name='viewport' content='width=device-width, initial-scale=1'>",
        "<title>XUI translation audit</title>",
        "<style>",
        ":root{",
        "  --bg:#f4f6f8;--surface:#fff;--text:#1a1a2e;--muted:#5c6370;",
        "  --border:#d8dee6;--accent:#2563eb;--accent-soft:#dbeafe;",
        "  --missing:#b91c1c;--missing-bg:#fef2f2;",
        "  --redundant:#b45309;--redundant-bg:#fffbeb;",
        "  --extra-attr:#7c3aed;--extra-attr-bg:#f5f3ff;",
        "  --unchanged:#0369a1;--unchanged-bg:#e0f2fe;",
        "  --placeholder:#c2410c;--placeholder-bg:#fff7ed;",
        "  --locale-only:#0f766e;--locale-only-bg:#ccfbf1;",
        "  --parse-error:#9f1239;--parse-error-bg:#fff1f2;",
        "  --ok:#15803d;--ok-bg:#f0fdf4;",
        "}",
        "*{box-sizing:border-box;}",
        "body{font-family:Segoe UI,system-ui,sans-serif;margin:0;background:var(--bg);"
        "color:var(--text);line-height:1.5;}",
        ".page{max-width:1280px;margin:0 auto;padding:24px;}",
        "header{background:linear-gradient(135deg,#1e3a5f,#2563eb);color:#fff;"
        "padding:28px 32px;border-radius:12px;margin-bottom:24px;}",
        "header h1{margin:0 0 8px;font-size:1.75rem;}",
        "header p{margin:0;opacity:.92;font-size:.95rem;}",
        "header code{background:#fff;color:#1e3a5f;padding:2px 8px;border-radius:4px;"
        "font-weight:500;}",
        "h2{margin:1.5em 0 .75em;font-size:1.25rem;}",
        "h5{margin:.75em 0 .35em;font-size:.9rem;}",
        ".note,.path-note{color:var(--muted);font-size:.9rem;}",
        ".path-note{margin-top:-.5em;margin-bottom:1em;}",
        ".global-summary{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));"
        "gap:12px;margin:20px 0;}",
        ".stat-card{background:var(--surface);border:1px solid var(--border);"
        "border-radius:10px;padding:14px 16px;}",
        ".stat-card .value{font-size:1.6rem;font-weight:700;line-height:1.2;}",
        ".stat-card .label{font-size:.8rem;color:var(--muted);margin-top:4px;}",
        ".skin-tabs{display:flex;flex-wrap:wrap;gap:8px;margin:20px 0 0;padding:0;}",
        ".skin-tab{border:1px solid var(--border);background:var(--surface);"
        "border-radius:999px;padding:8px 16px;cursor:pointer;font-size:.9rem;"
        "transition:background .15s,border-color .15s;}",
        ".skin-tab:hover{border-color:var(--accent);}",
        ".skin-tab.active{background:var(--accent);color:#fff;border-color:var(--accent);}",
        ".skin-tab .tab-count{opacity:.8;font-size:.8rem;margin-left:6px;}",
        ".skin-panel{background:var(--surface);border:1px solid var(--border);"
        "border-radius:12px;padding:20px 24px;margin-top:16px;}",
        ".skin-panel.hidden{display:none;}",
        "details.locale{border:1px solid var(--border);border-radius:8px;margin:12px 0;"
        "background:var(--surface);border-left:4px solid var(--accent);}",
        "details.locale>summary{cursor:pointer;padding:12px 16px;list-style:none;display:flex;"
        "flex-wrap:wrap;align-items:center;gap:8px 14px;}",
        "details.locale>summary::-webkit-details-marker{display:none;}",
        "details.locale>summary::before{content:'\\25B6';font-size:.7rem;color:var(--muted);"
        "transition:transform .15s;display:inline-block;margin-right:4px;}",
        "details.locale[open]>summary::before{transform:rotate(90deg);}",
        ".locale-code{font-weight:700;font-size:1rem;text-transform:uppercase;"
        "letter-spacing:.04em;}",
        ".locale-meta{color:var(--muted);font-size:.85rem;}",
        ".locale-body{padding:0 16px 16px;}",
        ".subsection-heading{margin:1.25em 0 .5em;font-size:.95rem;font-weight:600;}",
        ".file{border:1px solid var(--border);border-radius:6px;margin:10px 0;background:#fafbfc;}",
        ".file-heading{margin:0;padding:10px 12px;font-size:.9rem;font-weight:600;"
        "display:flex;flex-wrap:wrap;align-items:center;gap:8px;border-bottom:1px solid var(--border);"
        "background:#f8fafc;}",
        ".file-body{padding:8px 12px 12px;}",
        ".mini-summary{border-collapse:collapse;width:100%;max-width:520px;"
        "font-size:.85rem;margin-bottom:12px;}",
        ".mini-summary th,.mini-summary td{border:1px solid var(--border);padding:5px 10px;}",
        ".mini-summary th{background:#f8fafc;text-align:left;font-weight:600;}",
        ".badge{display:inline-block;min-width:1.6em;text-align:center;padding:1px 7px;"
        "border-radius:999px;font-size:.75rem;font-weight:600;}",
        ".badge-ok{background:var(--ok-bg);color:var(--ok);}",
        ".badge-missing{background:var(--missing-bg);color:var(--missing);}",
        ".badge-redundant{background:var(--redundant-bg);color:var(--redundant);}",
        ".badge-extra-attr{background:var(--extra-attr-bg);color:var(--extra-attr);}",
        ".badge-unchanged{background:var(--unchanged-bg);color:var(--unchanged);}",
        ".badge-placeholder{background:var(--placeholder-bg);color:var(--placeholder);}",
        ".badge-locale-only{background:var(--locale-only-bg);color:var(--locale-only);}",
        ".missing h5{color:var(--missing);}",
        ".redundant h5{color:var(--redundant);}",
        ".extra-attr h5{color:var(--extra-attr);}",
        ".unchanged h5{color:var(--unchanged);}",
        ".placeholder h5{color:var(--placeholder);}",
        ".locale-only h5{color:var(--locale-only);}",
        "table.entries{width:100%;border-collapse:collapse;font-size:.82rem;margin:8px 0;}",
        "table.entries th,table.entries td{border:1px solid var(--border);"
        "padding:4px 8px;vertical-align:top;}",
        "table.entries th{background:#f8fafc;text-align:left;}",
        "code{font-family:Consolas,monospace;font-size:.88em;background:#f1f5f9;"
        "padding:1px 5px;border-radius:4px;}",
        "ul.files{margin:8px 0;padding-left:24px;columns:2;column-gap:24px;}",
        ".all-clear{color:var(--ok);font-size:.9rem;margin:8px 0;}",
        ".matrix-wrap{overflow-x:auto;}",
        "table.matrix{border-collapse:collapse;font-size:.85rem;background:var(--surface);}",
        "table.matrix th,table.matrix td{border:1px solid var(--border);padding:6px 10px;}",
        "table.matrix th{background:#f8fafc;}",
        "table.matrix td.ok{color:var(--ok);text-align:center;}",
        "table.matrix td.na{color:var(--muted);text-align:center;}",
        "table.matrix td.issue{color:var(--missing);text-align:center;font-weight:600;}",
        ".redundant-hint{color:var(--redundant);font-weight:400;font-size:.8rem;}",
        ".file-reports{margin-top:8px;}",
        "</style>",
        "<script>",
        "function showSkin(skin){",
        "  document.querySelectorAll('.skin-tab').forEach(function(btn){",
        "    btn.classList.toggle('active', btn.dataset.skin === skin);",
        "  });",
        "  document.querySelectorAll('.skin-panel').forEach(function(panel){",
        "    panel.classList.toggle('hidden', panel.dataset.skin !== skin);",
        "  });",
        "}",
        "document.addEventListener('DOMContentLoaded', function(){",
        "  document.querySelectorAll('.skin-tab').forEach(function(btn){",
        "    btn.addEventListener('click', function(){ showSkin(btn.dataset.skin); });",
        "  });",
        "});",
        "</script>",
        "</head>",
        "<body>",
        "<div class='page'>",
        "<header>",
        "<h1>XUI translation audit</h1>",
        f"<p>All skins under <code>indra/newview/skins/*/xui/</code> compared "
        f"EN against every other locale present. Generated {audit_date}.</p>",
        "</header>",
        "<p class='note'>Entries are matched by widget <code>name</code> (viewer "
        "layered-XML rules). Audits <code>value</code> attributes and "
        "<code>panel.string</code> / <code>floater.string</code> bodies. Purely "
        "numeric <code>value</code> attributes (sizes, combo indices, padding) are "
        "ignored. Each locale element is also checked attribute-by-attribute against "
        "its EN counterpart. Elements with <code>translate=\"false\"</code> are skipped "
        "(direct content only, not children). Also flags unchanged EN copies, placeholder "
        "token mismatches, locale-only widgets, duplicate widget names, and XML parse "
        "errors.</p>",
        render_parse_errors(audit.parse_errors),
        "<div class='global-summary'>",
        f"<div class='stat-card'><div class='value'>{len(skin_reports)}</div>"
        "<div class='label'>Skins scanned</div></div>",
        f"<div class='stat-card'><div class='value'>{skins_with_locales}</div>"
        "<div class='label'>Skins with translations</div></div>",
        f"<div class='stat-card'><div class='value'>{locale_count}</div>"
        "<div class='label'>Locale comparisons</div></div>",
        f"<div class='stat-card'><div class='value'>{total_missing}</div>"
        "<div class='label'>Total missing strings</div></div>",
        f"<div class='stat-card'><div class='value'>{total_redundant}</div>"
        "<div class='label'>Total redundant strings</div></div>",
        f"<div class='stat-card'><div class='value'>{total_extra_attrs}</div>"
        "<div class='label'>Bad attributes (not on EN)</div></div>",
        f"<div class='stat-card'><div class='value'>{total_unchanged}</div>"
        "<div class='label'>Unchanged (same as EN)</div></div>",
        f"<div class='stat-card'><div class='value'>{total_placeholder}</div>"
        "<div class='label'>Placeholder mismatches</div></div>",
        f"<div class='stat-card'><div class='value'>{total_locale_only}</div>"
        "<div class='label'>Locale-only widgets</div></div>",
        f"<div class='stat-card'><div class='value'>{total_duplicate_names}</div>"
        "<div class='label'>Duplicate widget names</div></div>",
        f"<div class='stat-card'><div class='value'>{len(audit.parse_errors)}</div>"
        "<div class='label'>XML parse errors</div></div>",
        "</div>",
        render_matrix(skin_reports),
        "<h2>By skin</h2>",
        "<div class='skin-tabs' role='tablist'>",
    ]

    for index, skin_report in enumerate(skin_reports):
        skin = skin_report.skin
        issue_count = skin_issue_total(skin_report)
        active = " active" if index == 0 else ""
        parts.append(
            f"<button type='button' class='skin-tab{active}' data-skin='"
            f"{html.escape(skin)}' role='tab'>"
            f"{html.escape(skin)}"
            f"<span class='tab-count'>{issue_count} issues</span></button>",
        )

    parts.append("</div>")

    for index, skin_report in enumerate(skin_reports):
        parts.append(render_skin_panel(skin_report, active=index == 0))

    parts.extend(["</div>", "</body>", "</html>"])
    return "\n".join(parts)


def main() -> None:
    skins = discover_skins()
    if not skins:
        print(f"No skins with xui/en found under {SKINS_ROOT}")
        return

    parse_errors: list[ParseErrorRecord] = []
    skin_reports = [audit_skin(skin, parse_errors) for skin in skins]
    audit = AuditReport(skin_reports=skin_reports, parse_errors=parse_errors)
    output = render_html(audit)
    OUT_FILE.write_text(output, encoding="utf-8")
    print(
        f"Wrote {OUT_FILE} ({len(skins)} skins, "
        f"{len(parse_errors)} parse errors)",
    )


if __name__ == "__main__":
    main()
