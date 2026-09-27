#!/usr/bin/env python3
"""Compare Electron/React gui/ with native gui-qt/ for feature and API parity."""

from __future__ import annotations

import json
import re
import sys
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GUI = ROOT / "gui"
GUI_QT = ROOT / "gui-qt"
OUT_JSON = GUI_QT / "PARITY.json"
OUT_MD = GUI_QT / "PARITY.md"


@dataclass
class Item:
    id: str
    electron: bool
    qt: bool
    notes: str = ""

    @property
    def ok(self) -> bool:
        return self.electron == self.qt or (not self.electron and not self.qt)


@dataclass
class Report:
    generated_at: str = ""
    items: list[Item] = field(default_factory=list)

    def add(self, item: Item) -> None:
        self.items.append(item)

    def score(self) -> tuple[int, int]:
        relevant = [i for i in self.items if i.electron]
        if not relevant:
            return 0, 0
        done = sum(1 for i in relevant if i.qt)
        return done, len(relevant)


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8") if path.exists() else ""


def daemon_methods_from_ts() -> set[str]:
    text = read_text(GUI / "src/api/daemon.ts")
    return set(re.findall(r"async\s+(\w+)\s*\(", text))


def daemon_methods_from_cpp() -> set[str]:
    text = read_text(GUI_QT / "src/api/DaemonClient.hpp") + read_text(
        GUI_QT / "src/api/DaemonClient.cpp"
    )
    found: set[str] = set()
    for name in daemon_methods_from_ts():
        if re.search(rf"\b{name}\s*\(", text):
            found.add(name)
    return found


def collect_pages() -> tuple[set[str], set[str]]:
    electron = {p.stem for p in (GUI / "src/pages").glob("*.tsx") if not p.name.endswith(".test.tsx")}
    qt = {p.stem for p in (GUI_QT / "src/ui/pages").glob("*.cpp")}
    # Map Qt DashboardPage -> DashboardPage
    qt_norm = {n.replace("Page", "Page") for n in qt}
    return electron, qt_norm


def settings_categories() -> tuple[set[str], set[str]]:
    electron = set(
        re.findall(
            r'\|\s*"(\w+)"',
            read_text(GUI / "src/pages/SettingsPage.tsx"),
        )
    )
    settings_cpp = read_text(GUI_QT / "src/ui/pages/SettingsPage.cpp")
    qt = set(re.findall(r"make(\w+)Panel", settings_cpp))
    qt = {name.replace("Placeholder", "").lower() for name in qt if name != "Placeholder"}
    # makeGeneralPanel -> general
    qt = {n.removesuffix("panel") if n.endswith("panel") else n for n in qt}
    qt = {n.lower() for n in qt}
    return electron, qt


def i18n_key_counts() -> tuple[int, int]:
    en = GUI / "src/i18n/locales/en.json"
    if not en.exists():
        return 0, 0
    data = json.loads(en.read_text(encoding="utf-8"))

    def flatten(obj: object, prefix: str = "") -> set[str]:
        keys: set[str] = set()
        if isinstance(obj, dict):
            for k, v in obj.items():
                path = f"{prefix}.{k}" if prefix else k
                if isinstance(v, dict):
                    keys |= flatten(v, path)
                else:
                    keys.add(path)
        return keys

    electron_keys = flatten(data)
    tr_cpp = read_text(GUI_QT / "src/i18n/Translator.cpp")
    qt_keys = set(re.findall(r'QStringLiteral\("([^"]+)"\),\s*QStringLiteral', tr_cpp))
    return len(electron_keys), len(qt_keys)


def theme_tokens_match() -> bool:
    ts = read_text(GUI / "src/theme/themes.ts")
    cpp = read_text(GUI_QT / "src/theme/ThemeTokens.cpp")
    for hex_color in re.findall(r"#(?:[0-9a-fA-F]{6})", ts):
        if hex_color.lower() not in cpp.lower():
            return False
    return True


def electron_components() -> int:
    return len(list((GUI / "src/components").rglob("*.tsx")))


def build_report() -> Report:
    report = Report(generated_at=datetime.now(timezone.utc).isoformat())

    e_pages, q_pages = collect_pages()
    for page in sorted(e_pages):
        qt_has = page in q_pages
        report.add(Item(f"page:{page}", True, qt_has, "" if qt_has else "page missing in gui-qt/src/ui/pages"))

    e_settings, _ = settings_categories()
    nav_cats = set(
        re.findall(
            r'SettingsCategory::(\w+)',
            read_text(GUI_QT / "src/ui/widgets/SettingsSidebarNav.cpp"),
        )
    )
    nav_cats = {c.lower() for c in nav_cats}
    _, q_settings = settings_categories()
    for cat in sorted(e_settings):
        has_nav = cat in nav_cats
        full_panel = cat in q_settings
        report.add(
            Item(
                f"settings:{cat}",
                True,
                has_nav and full_panel,
                "nav only" if has_nav and not full_panel else "",
            )
        )

    e_methods = daemon_methods_from_ts()
    q_methods = daemon_methods_from_cpp()
    for method in sorted(e_methods):
        report.add(Item(f"daemon:{method}", True, method in q_methods, ""))

    e_keys, q_keys = i18n_key_counts()
    report.add(
        Item(
            "i18n:locale-en",
            True,
            q_keys >= min(50, e_keys // 10),
            f"electron={e_keys} keys, qt≈{q_keys} hardcoded",
        )
    )

    report.add(Item("theme:tokens", True, theme_tokens_match(), "colors from themes.ts"))
    report.add(
        Item(
            "layout:footer",
            True,
            "FooterBar" in read_text(GUI_QT / "src/ui/widgets/FooterBar.cpp"),
            "",
        )
    )
    report.add(
        Item(
            "layout:console",
            True,
            "ConsoleDock" in read_text(GUI_QT / "src/ui/widgets/ConsoleDock.cpp"),
            "",
        )
    )
    report.add(
        Item(
            "layout:session-selector",
            True,
            "SessionSelector" in read_text(GUI_QT / "src/ui/widgets/SessionSelector.cpp"),
            "",
        )
    )
    report.add(Item("sync:websocket-sse", True, False, "SSE path; WS optional"))
    report.add(Item("sync:snapshot-parser", True, False, "uses list RPC not stream snapshot"))
    report.add(
        Item(
            "desktop:tray",
            True,
            False,
            "Electron tray not in Qt",
        )
    )
    report.add(
        Item(
            "desktop:window-controls",
            True,
            "WindowTitleBar" in read_text(GUI_QT / "src/ui/DesktopShellWindow.cpp")
            or "FramelessWindowHint" in read_text(GUI_QT / "src/ui/DesktopShellWindow.cpp"),
            "frameless shell; full chrome optional",
        )
    )
    report.add(
        Item(
            "extension:bridge-parity",
            True,
            False,
            "C daemon stub; not full extension-bridge.cjs",
        )
    )

    comp_e = electron_components()
    comp_q = len(list((GUI_QT / "src/ui").rglob("*.cpp")))
    report.add(
        Item(
            "ui:component-surface",
            True,
            comp_q >= comp_e // 4,
            f"electron≈{comp_e} tsx, qt≈{comp_q} widgets",
        )
    )

    return report


def write_outputs(report: Report) -> None:
    done, total = report.score()
    pct = (100.0 * done / total) if total else 0.0
    payload = {
        "generated_at": report.generated_at,
        "score": {"done": done, "total": total, "percent": round(pct, 1)},
        "items": [
            {
                "id": i.id,
                "electron": i.electron,
                "qt": i.qt,
                "ok": i.ok,
                "notes": i.notes,
            }
            for i in report.items
        ],
        "gaps": [i.id for i in report.items if i.electron and not i.qt],
    }
    OUT_JSON.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

    lines = [
        "# Electron → Qt parity",
        "",
        f"Last audit: `{report.generated_at}`",
        "",
        f"**Score: {done}/{total} ({pct:.1f}%)** — auto-updated by `scripts/parity_audit.py`.",
        "",
        "## Gaps (Electron yes, Qt no)",
        "",
    ]
    for item in report.items:
        if item.electron and not item.qt:
            note = f" — {item.notes}" if item.notes else ""
            lines.append(f"- [ ] `{item.id}`{note}")
    lines.extend(
        [
            "",
            "## Matched",
            "",
        ]
    )
    for item in report.items:
        if item.electron and item.qt:
            lines.append(f"- [x] `{item.id}`")
    lines.extend(
        [
            "",
            "## Loop",
            "",
            "Run `./scripts/parity_audit.py` after each parity pass. ",
            "Agent loop: fix top gap, re-audit, repeat until score is 100%.",
            "",
        ]
    )
    OUT_MD.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    report = build_report()
    write_outputs(report)
    done, total = report.score()
    print(f"Parity: {done}/{total} ({100.0 * done / total:.1f}%)" if total else "Parity: n/a")
    gaps = [i.id for i in report.items if i.electron and not i.qt]
    print(f"Gaps: {len(gaps)}")
    for gap in gaps[:15]:
        print(f"  - {gap}")
    if len(gaps) > 15:
        print(f"  ... and {len(gaps) - 15} more (see PARITY.md)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
