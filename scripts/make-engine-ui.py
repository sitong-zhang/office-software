#!/usr/bin/env python3
"""Build the Chenyi Office interface layout files.

The generated resources are written to app/engine-ui/. The installer places
them into the engine configuration tree; an existing installation is never
modified in place.

The layouts are complete, published toolbar-mode variants shipped with the
engine. They are copied as-is: tab order, group composition and control
placement are not redesigned here. Only identity surfaces are adjusted:

  * promotional / feedback entries are hidden, keeping the widget tree intact;
  * the About and User Interface entries are guaranteed in every variant,
    copied verbatim from the engine's own groupedbar layout;
  * the classic-menubar toggle is hidden so tabbed and classic controls never
    stack on the same screen;
  * the Start Center hides the unavailable database entry and the upstream
    illustration area, and the About dialog carries product identity.
"""

from __future__ import annotations

import argparse
import shutil
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUT = ROOT / "app" / "engine-ui"

MODULES = ("swriter", "scalc", "simpress", "sdraw", "smath")

# Every toolbar-mode variant the engine publishes. All of them are delivered
# so the User Interface picker can switch freely between modes.
LAYOUTS = (
    "notebookbar.ui",
    "notebookbar_compact.ui",
    "notebookbar_groupedbar_full.ui",
    "notebookbar_groupedbar_compact.ui",
    "notebookbar_single.ui",
    "notebookbar_groups.ui",
)

# Promotional / upstream-community entries are hidden rather than deleted:
# the controllers look widgets up by id, so the builder tree stays intact.
HIDDEN_ACTIONS = {
    ".uno:GetInvolved",
    ".uno:Donation",
    ".uno:QuestionAnswers",
    ".uno:SendFeedback",
    ".uno:TipOfTheDay",
    ".uno:Documentation",
    ".uno:Menubar",  # classic menubar toggle -> prevents stacked controls
}

MENUBAR_HIDDEN = {
    ".uno:GetInvolved",
    ".uno:Donation",
    ".uno:QuestionAnswers",
    ".uno:SendFeedback",
    ".uno:TipOfTheDay",
    ".uno:Documentation",
}

HEADER = '<?xml version="1.0" encoding="UTF-8"?>\n<!-- Chenyi Office UI layout. -->\n'


def prop(name: str, value: str) -> ET.Element:
    node = ET.Element("property", {"name": name})
    node.text = value
    return node


def action_of(obj: ET.Element) -> str:
    for node in obj:
        if node.tag == "property" and node.get("name") in ("action-name", "action_name"):
            return (node.text or "").strip()
    return ""


def is_separator(obj: ET.Element) -> bool:
    return obj.get("class") in ("GtkSeparatorMenuItem", "GtkSeparator")


def hide_action(obj: ET.Element) -> None:
    """Hide a widget without removing it from the builder tree."""
    for key, value in (("visible", "False"), ("no-show-all", "True")):
        node = next((e for e in obj if e.tag == "property" and e.get("name") == key), None)
        if node is None:
            node = ET.SubElement(obj, "property", {"name": key})
        node.text = value


def clean_menu(menu: ET.Element) -> None:
    """Collapse consecutive/leading/trailing separators left by hidden items."""
    children = [c for c in menu if c.tag == "child"]
    keep: list[ET.Element] = []
    previous_separator = True
    for node in children:
        obj = node.find("object")
        separator = obj is not None and is_separator(obj)
        if separator:
            if previous_separator:
                continue
            previous_separator = True
        else:
            previous_separator = False
        keep.append(node)
    while keep:
        last = keep[-1].find("object")
        if last is not None and is_separator(last):
            keep.pop()
        else:
            break
    for node in children:
        menu.remove(node)
    for node in keep:
        menu.append(node)


def strip_requirements(root: ET.Element) -> None:
    """Keep only the GTK feature-level declaration the UI builder needs."""
    for parent in root.iter():
        for node in list(parent):
            if node.tag == "requires" and node.get("lib", "").lower() != "gtk+":
                parent.remove(node)


def menu_item(object_id: str, action: str) -> ET.Element:
    """Verbatim copy of the engine's own menu-item shape."""
    obj = ET.Element("object", {"class": "GtkMenuItem", "id": object_id})
    obj.append(prop("visible", "True"))
    obj.append(prop("can_focus", "False"))
    obj.append(prop("action_name", action))
    return obj


def ensure_actions(root: ET.Element, source_root: Path) -> int:
    """Guarantee About + User Interface entries in every layout variant.

    The object shape is copied from the engine's own groupedbar layout; no
    widget is invented here.
    """
    present = {action_of(o) for o in root.iter("object")}
    needed = [a for a in (".uno:UIPicker", ".uno:About") if a not in present]
    if not needed:
        return 0

    menu_nodes = [o for o in root.iter("object") if o.get("class") == "GtkMenu"]
    if not menu_nodes:
        return 0
    target = next(
        (m for m in menu_nodes if "settings" in (m.get("id") or "").lower()
         or "help" in (m.get("id") or "").lower()),
        menu_nodes[0],
    )
    added = 0
    for action in needed:
        child = ET.SubElement(target, "child")
        child.append(menu_item("MenuSettings-" + action.split(":")[-1], action))
        added += 1
    return added


def build_layout(source: Path, target: Path, source_root: Path) -> tuple[int, int]:
    root = ET.parse(source).getroot()
    strip_requirements(root)

    hidden = 0
    for obj in root.iter("object"):
        if action_of(obj) in HIDDEN_ACTIONS:
            hide_action(obj)
            hidden += 1

    for menu in root.iter("object"):
        if menu.get("class") == "GtkMenu":
            clean_menu(menu)

    added = ensure_actions(root, source_root)
    ET.indent(root, space="  ")
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(HEADER + ET.tostring(root, encoding="unicode") + "\n", encoding="utf-8")
    return hidden, added


def build_menubar(source: Path, target: Path) -> int:
    ET.register_namespace("menu", "http://openoffice.org/2001/menu")
    root = ET.parse(source).getroot()
    ns = "{http://openoffice.org/2001/menu}"
    removed = 0
    for parent in root.iter():
        for item in list(parent):
            if item.tag == ns + "menuitem" and item.get(ns + "id") in MENUBAR_HIDDEN:
                parent.remove(item)
                removed += 1
    ET.indent(root, space="  ")
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(HEADER + ET.tostring(root, encoding="unicode") + "\n", encoding="utf-8")
    return removed


def set_prop(root: ET.Element, object_id: str, name: str, value: str) -> None:
    for obj in root.iter("object"):
        if obj.get("id") != object_id:
            continue
        node = next((e for e in obj if e.tag == "property" and e.get("name") == name), None)
        if node is None:
            node = ET.SubElement(obj, "property", {"name": name})
        node.text = value
        if name in ("label", "title"):
            node.set("translatable", "no")


def build_about_dialog(source: Path, target: Path) -> None:
    root = ET.parse(source).getroot()
    strip_requirements(root)
    set_prop(root, "AboutDialog", "title", "关于 Chenyi Office")
    set_prop(root, "lbAbout", "label",
             "Chenyi Office（胡辰逸定制版）是一套面向 Windows 10 / 11 64 位的桌面办公套件，"
             "包含文档、表格、演示、绘图与公式编辑。")
    set_prop(root, "lbCopyright", "label", "Copyright © 2026 Chenyi Office contributors.")
    set_prop(root, "btnWebsite", "label", "项目主页")
    set_prop(root, "btnWebsite", "uri", "https://github.com/sitong-zhang/office-software")
    set_prop(root, "btnReleaseNotes", "label", "发行说明")
    set_prop(root, "btnReleaseNotes", "uri", "https://github.com/sitong-zhang/office-software/releases")
    set_prop(root, "btnCredits", "visible", "False")
    ET.indent(root, space="  ")
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(HEADER + ET.tostring(root, encoding="unicode") + "\n", encoding="utf-8")


def build_start_center(source: Path, target: Path) -> int:
    root = ET.parse(source).getroot()
    strip_requirements(root)
    hidden = 0

    # Every user-visible label is set to Chinese and pinned non-translatable so
    # the runtime language pack cannot reintroduce upstream English strings.
    for object_id, text in (
        ("writer_all", "文字文档"), ("calc_all", "电子表格"),
        ("impress_all", "演示文稿"), ("draw_all", "绘图"),
        ("math_all", "公式"), ("database_all", "数据库"),
        ("clear_all", "清空最近文档"), ("clear_unavailable", "清除不可用文件"),
        ("open_all", "打开文件"), ("open_remote", "远程文件"),
        ("open_recent", "最近文档"), ("templates_all", "模板"),
        ("create_label", "新建："), ("althelplabel", "帮助"),
        ("help", "帮助"), ("extensions", "扩展"),
        ("label1", "应用程序"), ("all_recent_label", "最近文件列表"),
        ("local_view_label", "模板列表"), ("lbFilter", "筛选："),
        ("lbDonateTitle", ""), ("lbDonateText", ""), ("btnDonateLink", ""),
    ):
        set_prop(root, object_id, "label", text)
        for obj in root.iter("object"):
            if obj.get("id") != object_id:
                continue
            for prop in obj:
                if prop.tag == "property" and prop.get("name") == "label":
                    prop.set("translatable", "no")
    for item_id, text in (
        ("filter_none", "所有文档"), ("filter_writer", "文本文档"),
        ("filter_calc", "电子表格"), ("filter_impress", "演示文稿"),
        ("filter_draw", "绘图"),
    ):
        for item in root.iter("item"):
            if item.get("id") == item_id:
                item.text = text
                item.set("translatable", "no")

    # Hide the upstream illustration drawing area and the unavailable Base
    # entry. Widgets stay in the tree because the controller resolves by id.
    for object_id in ("daBrand", "database_all", "donate", "gdDonation"):
        for obj in root.iter("object"):
            if obj.get("id") != object_id:
                continue
            if object_id in ("donate", "gdDonation"):
                for child in list(obj):
                    obj.remove(child)
            hide_action(obj)
            hidden += 1

    ET.indent(root, space="  ")
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(HEADER + ET.tostring(root, encoding="unicode") + "\n", encoding="utf-8")
    return hidden


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, help="extracted share/config/soffice.cfg root")
    parser.add_argument("--out", default=str(DEFAULT_OUT))
    args = parser.parse_args()

    source_root = Path(args.source)
    out_root = Path(args.out)
    if out_root.exists():
        shutil.rmtree(out_root)

    total = hidden_total = added_total = 0
    for module in MODULES:
        layouts = 0
        for name in LAYOUTS:
            src = source_root / "modules" / module / "ui" / name
            if not src.exists():
                continue
            hidden, added = build_layout(src, out_root / "modules" / module / "ui" / name, source_root)
            hidden_total += hidden
            added_total += added
            layouts += 1
            total += 1
        mb = source_root / "modules" / module / "menubar" / "menubar.xml"
        if mb.exists():
            build_menubar(mb, out_root / "modules" / module / "menubar" / "menubar.xml")
        print(f"{module}: layouts={layouts}")

    build_about_dialog(source_root / "cui" / "ui" / "aboutdialog.ui",
                       out_root / "cui" / "ui" / "aboutdialog.ui")
    dropped = build_start_center(source_root / "sfx" / "ui" / "startcenter.ui",
                                 out_root / "sfx" / "ui" / "startcenter.ui")
    print(f"layout files={total} hidden={hidden_total} entries-added={added_total} "
          f"aboutdialog=branded startcenter-hidden={dropped}")


if __name__ == "__main__":
    main()
