#!/usr/bin/env python3
"""Build the Chenyi Office engine UI layout files.

The files are generated from the stock .ui/.xml resources shipped with the
engine and written to app/engine-ui/. The installer copies them into the
engine's share/config/soffice.cfg tree for engines that Chenyi Office installs
itself. A pre-existing engine is never touched.

Layout changes:
  * Grouped tab order for Writer, Calc, Impress, Draw and Math.
  * An "Editing" group (Find & Replace / Select All) at the end of Home.
  * Getting-involved, promotional, feedback and About entries removed
    from the notebookbar Help menu and the classic menu bar.
  * The Start Center promotional panel and button removed.
"""

from __future__ import annotations

import argparse
import shutil
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUT = ROOT / "app" / "engine-ui"

# Math (smath) is included so its menubar receives the same
# menu-pruning treatment as the other modules. Math ships a classic
# menubar and may or may not carry a notebookbar; build_notebookbar()
# skips modules without one, and reorder_tabs() preserves source order
# when a module has no TAB_ORDER entry.
MODULES = ("swriter", "scalc", "simpress", "sdraw", "smath")

# Grouped tab order. Keys are derived from the tab label text after removing
# the mnemonic marker and lower-casing: "Reference~s" -> "references".
TAB_ORDER = {
    "swriter": [
        "file", "home", "insert", "layout", "references", "review", "view",
        "tools", "extension", "table", "image", "draw", "object", "media",
        "print", "form",
    ],
    "scalc": [
        "file", "home", "insert", "layout", "functions", "data", "review",
        "view", "tools", "extension", "image", "draw", "object", "media",
        "print", "form",
    ],
    "simpress": [
        "file", "home", "insert", "design", "layout", "slideshow", "review",
        "view", "tools", "extension", "table", "image", "draw", "object",
        "media", "form", "master", "outline", "3d",
    ],
    "sdraw": [
        "file", "home", "insert", "layout", "review", "view", "tools",
        "extension", "table", "image", "draw", "object", "media", "3d",
        "master", "form",
    ],
}

# Menu entries that expose promotional, feedback or About content.
# ShowLicense is kept because the licence must remain reachable.
REMOVED_ACTIONS = {
    ".uno:GetInvolved",
    ".uno:Donation",
    ".uno:About",
    ".uno:QuestionAnswers",
    ".uno:SendFeedback",
    ".uno:TipOfTheDay",
    ".uno:Documentation",
}


def prop(name: str, value: str) -> ET.Element:
    node = ET.Element("property", {"name": name})
    node.text = value
    return node


def child(obj: ET.Element, packing: list[tuple[str, str]]) -> ET.Element:
    node = ET.Element("child")
    node.append(obj)
    pack = ET.SubElement(node, "packing")
    for key, value in packing:
        pack.append(prop(key, value))
    return node


def make_button(object_id: str, action: str) -> ET.Element:
    obj = ET.Element("object", {"class": "GtkToolButton", "id": object_id})
    obj.append(prop("visible", "True"))
    obj.append(prop("can-focus", "False"))
    obj.append(prop("action-name", action))
    return child(obj, [("expand", "False"), ("homogeneous", "True")])


def make_editing_group() -> ET.Element:
    grid = ET.Element("object", {"class": "GtkGrid", "id": "gdHomeEditing"})
    for key, value in (
        ("visible", "True"),
        ("can-focus", "False"),
        ("margin-start", "6"),
        ("margin-end", "3"),
        ("margin-top", "6"),
        ("margin-bottom", "6"),
        ("row-spacing", "3"),
        ("column-spacing", "3"),
    ):
        grid.append(prop(key, value))

    label = ET.Element("object", {"class": "GtkLabel", "id": "lbHomeEditing"})
    for key, value in (
        ("visible", "True"),
        ("can-focus", "False"),
        ("valign", "end"),
        ("vexpand", "True"),
    ):
        label.append(prop(key, value))
    text = prop("label", "编辑")
    text.set("translatable", "no")
    label.append(text)
    attrs = ET.SubElement(label, "attributes")
    ET.SubElement(attrs, "attribute", {"name": "style", "value": "italic"})
    ET.SubElement(attrs, "attribute", {"name": "scale", "value": "0.9"})
    accessibility = ET.SubElement(label, "accessibility")
    ET.SubElement(accessibility, "relation", {"type": "label-for", "target": "gdHomeEditing"})
    grid.append(
        child(
            label,
            [("left-attach", "0"), ("top-attach", "2"), ("width", "2")],
        )
    )

    box = ET.Element("object", {"class": "sfxlo-NotebookbarToolBox", "id": "bxHomeEditing"})
    for key, value in (
        ("visible", "True"),
        ("can-focus", "False"),
        ("valign", "center"),
        ("vexpand", "True"),
        ("show-arrow", "False"),
        ("icon_size", "3"),
    ):
        box.append(prop(key, value))
    box.append(make_button("btnHomeFindReplace", ".uno:FindReplace"))
    box.append(make_button("btnHomeSelectAll", ".uno:SelectAll"))
    grid.append(
        child(
            box,
            [("left-attach", "0"), ("top-attach", "0"), ("height", "2")],
        )
    )
    return grid


def tab_key(label: ET.Element) -> str:
    for node in label:
        if node.tag == "property" and node.get("name") == "label":
            return (node.text or "").replace("~", "").replace(" ", "").strip().lower()
    return ""


def action_of(obj: ET.Element) -> str:
    for node in obj:
        if node.tag == "property" and node.get("name") in ("action-name", "action_name"):
            return (node.text or "").strip()
    return ""


def is_separator(obj: ET.Element) -> bool:
    return obj.get("class") in ("GtkSeparatorMenuItem", "GtkSeparator")


def clean_menu(menu: ET.Element) -> None:
    """Drop consecutive, leading and trailing separators inside a menu."""
    children = [c for c in menu if c.tag == "child"]
    keep: list[ET.Element] = []
    previous_separator = True  # suppress a leading separator
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


def strip_notebookbar(root: ET.Element) -> int:
    removed = 0
    for menu in root.iter("object"):
        if menu.get("class") != "GtkMenu":
            continue
        for node in list(menu):
            if node.tag != "child":
                continue
            obj = node.find("object")
            if obj is not None and action_of(obj) in REMOVED_ACTIONS:
                menu.remove(node)
                removed += 1
        clean_menu(menu)
    return removed


def reorder_tabs(root: ET.Element, module: str) -> list[str]:
    control = None
    for obj in root.iter("object"):
        if obj.get("class") == "sfxlo-NotebookbarTabControl":
            control = obj
            break
    if control is None:
        raise RuntimeError(f"{module}: tab control not found")

    pairs: list[tuple[str, list[ET.Element]]] = []
    non_children = [node for node in control if node.tag != "child"]
    pending: ET.Element | None = None
    for node in list(control):
        if node.tag != "child":
            continue
        obj = node.find("object")
        if obj is not None and obj.get("class") == "sfxlo-PriorityHBox":
            pending = node
            continue
        if obj is not None and obj.get("class") == "GtkLabel" and pending is not None:
            pairs.append((tab_key(obj), [pending, node]))
            pending = None
            continue
        pairs.append(("", [node]))
    if pending is not None:
        pairs.append(("", [pending]))

    order = TAB_ORDER.get(module)
    if order is None:
        # No preferred tab order for this module (e.g. Math); keep source order
        # instead of raising KeyError.
        return [key for key, _ in pairs]
    indexed = {key: index for index, key in enumerate(order)}
    ordered = sorted(
        enumerate(pairs),
        key=lambda item: (indexed.get(item[1][0], len(order) + item[0])),
    )
    for node in list(control):
        control.remove(node)
    for node in non_children:
        control.append(node)
    for _, (_, nodes) in ordered:
        for node in nodes:
            control.append(node)
    return [key for _, (key, _) in ordered]


def add_editing_group(root: ET.Element, module: str) -> bool:
    control = None
    for obj in root.iter("object"):
        if obj.get("class") == "sfxlo-NotebookbarTabControl":
            control = obj
            break
    if control is None:
        return False

    home = None
    pending = None
    for node in control:
        if node.tag != "child":
            continue
        obj = node.find("object")
        if obj is None:
            continue
        if obj.get("class") == "sfxlo-PriorityHBox":
            pending = obj
        elif obj.get("class") == "GtkLabel" and pending is not None:
            if tab_key(obj) == "home":
                home = pending
                break
            pending = None
    if home is None:
        return False

    merged = None
    for node in home:
        if node.tag == "child":
            obj = node.find("object")
            if obj is not None and obj.get("class") == "sfxlo-PriorityMergedHBox":
                merged = obj
                break
    if merged is None:
        return False

    new_child = child(
        make_editing_group(),
        [("expand", "False"), ("fill", "True"), ("position", "0")],
    )
    insert_at = len(list(merged))
    if module == "swriter":
        # Keep the wide style gallery on the right edge; the compact Editing
        # group sits before it so its caption is not squeezed at narrow widths.
        for index, node in enumerate(list(merged)):
            obj = node.find("object")
            if obj is not None and obj.get("id") == "gdHomeStyle":
                insert_at = index
                break
    merged.insert(insert_at, new_child)

    index = 0
    for node in merged:
        if node.tag != "child":
            continue
        pack = node.find("packing")
        if pack is None:
            pack = ET.SubElement(node, "packing")
        position = next(
            (entry for entry in pack if entry.get("name") == "position"), None
        )
        if position is None:
            position = ET.SubElement(pack, "property", {"name": "position"})
        position.text = str(index)
        index += 1
    return True


def strip_requirements(root: ET.Element) -> None:
    """Drop the UI builder requirement tag.

    The UI builder only needs the GTK feature level to parse the file; the
    layout itself works without that requirement tag.
    """
    for node in list(root):
        if node.tag == "requires" and node.get("lib") == "LibreOffice":
            root.remove(node)


def build_notebookbar(source: Path, target: Path, module: str) -> tuple[int, list[str], bool]:
    tree = ET.parse(source)
    root = tree.getroot()
    strip_requirements(root)
    removed = strip_notebookbar(root)
    order = reorder_tabs(root, module)
    added = add_editing_group(root, module)

    ET.indent(root, space="  ")
    body = ET.tostring(root, encoding="unicode")
    header = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        "<!-- Chenyi Office UI layout. Modified from Mozilla Public License 2.0\n"
        "     source files; the corresponding source is this repository. -->\n"
    )
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(header + body + "\n", encoding="utf-8")
    return removed, order, added


def build_menubar(source: Path, target: Path) -> int:
    ET.register_namespace("menu", "http://openoffice.org/2001/menu")
    tree = ET.parse(source)
    root = tree.getroot()
    removed = 0

    def walk(node: ET.Element) -> None:
        nonlocal removed
        for item in list(node):
            if item.tag.endswith("menuitem"):
                if item.get("{http://openoffice.org/2001/menu}id") in REMOVED_ACTIONS:
                    node.remove(item)
                    removed += 1
                    continue
            walk(item)
        if node.tag.endswith("menupopup"):
            entries = list(node)
            previous_separator = True
            keep: list[ET.Element] = []
            for entry in entries:
                separator = entry.tag.endswith("menuseparator")
                if separator and previous_separator:
                    continue
                previous_separator = separator
                keep.append(entry)
            while keep and keep[-1].tag.endswith("menuseparator"):
                keep.pop()
            for entry in entries:
                node.remove(entry)
            for entry in keep:
                node.append(entry)

    walk(root)
    ET.indent(root, space="  ")
    body = ET.tostring(root, encoding="unicode")
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        "<!-- Chenyi Office UI layout. Modified from Mozilla Public License 2.0\n"
        "     source files; the corresponding source is this repository. -->\n"
        + body
        + "\n",
        encoding="utf-8",
    )
    return removed


def build_about_dialog(source: Path, target: Path) -> None:
    tree = ET.parse(source)
    root = tree.getroot()
    strip_requirements(root)

    def set_prop(object_id: str, name: str, value: str) -> None:
        for obj in root.iter("object"):
            if obj.get("id") != object_id:
                continue
            node = next(
                (entry for entry in obj if entry.tag == "property" and entry.get("name") == name),
                None,
            )
            if node is None:
                node = ET.SubElement(obj, "property", {"name": name})
            node.text = value
            if name in ("label", "title"):
                node.set("translatable", "no")

    set_prop("AboutDialog", "title", "关于 Chenyi Office")
    set_prop(
        "lbAbout",
        "label",
        "Chenyi Office（胡辰逸定制版）是一套面向 Windows 10 / 11 64 位的桌面办公套件，"
        "包含文档、表格、演示、绘图与公式编辑。",
    )
    set_prop("lbCopyright", "label", "Copyright © 2026 Chenyi Office contributors.")
    set_prop("btnWebsite", "label", "项目主页")
    set_prop("btnWebsite", "uri", "https://github.com/sitong-zhang/office-software")
    set_prop("btnReleaseNotes", "label", "发行说明")
    set_prop("btnReleaseNotes", "uri", "https://github.com/sitong-zhang/office-software/releases")
    # The credits page is generated from the engine's contributor list.
    set_prop("btnCredits", "visible", "False")

    ET.indent(root, space="  ")
    body = ET.tostring(root, encoding="unicode")
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        "<!-- Chenyi Office UI layout. Modified from Mozilla Public License 2.0\n"
        "     source files; the corresponding source is this repository. -->\n"
        + body
        + "\n",
        encoding="utf-8",
    )


def build_start_center(source: Path, target: Path) -> int:
    tree = ET.parse(source)
    root = tree.getroot()
    strip_requirements(root)
    hidden = 0

    def set_label(object_id: str, text: str) -> None:
        for obj in root.iter("object"):
            if obj.get("id") != object_id:
                continue
            label = next(
                (node for node in obj if node.tag == "property" and node.get("name") == "label"),
                None,
            )
            if label is None:
                label = ET.SubElement(obj, "property", {"name": "label"})
            label.text = text
            label.set("translatable", "no")

    def set_item(item_id: str, text: str) -> None:
        for item in root.iter("item"):
            if item.get("id") == item_id:
                item.text = text
                item.set("translatable", "no")

    for object_id, text in (
        ("writer_all", "文字文档"),
        ("calc_all", "电子表格"),
        ("impress_all", "演示文稿"),
        ("draw_all", "绘图"),
        ("math_all", "公式"),
        ("database_all", "数据库"),
    ):
        set_label(object_id, text)
    for item_id, text in (
        ("filter_none", "所有文档"),
        ("filter_writer", "文本文档"),
        ("filter_calc", "电子表格"),
        ("filter_impress", "演示文稿"),
        ("filter_draw", "绘图"),
    ):
        set_item(item_id, text)

    def hide_object(object_id: str) -> None:
        nonlocal hidden
        for obj in root.iter("object"):
            if obj.get("id") != object_id:
                continue
            visible = next(
                (node for node in obj if node.tag == "property" and node.get("name") == "visible"),
                None,
            )
            if visible is None:
                visible = ET.SubElement(obj, "property", {"name": "visible"})
            visible.text = "False"
            hidden += 1

    # Keep the widgets in the tree (the Start Center controller looks them up
    # by name) but hide the promotional button and panel.
    hide_object("donate")
    hide_object("gdDonation")

    ET.indent(root, space="  ")
    body = ET.tostring(root, encoding="unicode")
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        "<!-- Chenyi Office UI layout. Modified from Mozilla Public License 2.0\n"
        "     source files; the corresponding source is this repository. -->\n"
        + body
        + "\n",
        encoding="utf-8",
    )
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

    for module in MODULES:
        nb_source = source_root / "modules" / module / "ui" / "notebookbar.ui"
        if nb_source.exists():
            removed, order, added = build_notebookbar(
                nb_source,
                out_root / "modules" / module / "ui" / "notebookbar.ui",
                module,
            )
        else:
            # Module (e.g. Math) ships only a classic menubar; skip the
            # notebookbar pass but keep it in the consistency report.
            removed, order, added = 0, [], False
            print(f"{module}: no notebookbar.ui; menubar only")
        build_menubar(
            source_root / "modules" / module / "menubar" / "menubar.xml",
            out_root / "modules" / module / "menubar" / "menubar.xml",
        )
        print(
            f"{module}: tabs={','.join(order)} menu-items-removed={removed} "
            f"editing-group={'yes' if added else 'no'}"
        )

    build_about_dialog(
        source_root / "cui" / "ui" / "aboutdialog.ui",
        out_root / "cui" / "ui" / "aboutdialog.ui",
    )
    print("aboutdialog: branded")

    dropped = build_start_center(
        source_root / "sfx" / "ui" / "startcenter.ui",
        out_root / "sfx" / "ui" / "startcenter.ui",
    )
    print(f"startcenter: objects-removed={dropped}")


if __name__ == "__main__":
    main()
