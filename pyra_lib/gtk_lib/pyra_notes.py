#!/usr/bin/env python3
# pyra_notes.py — CYON notes editor + Piper TTS
# Author: Ioannes Cruxibulum

import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, Gdk, GLib, Gio

import os
import sys
import subprocess
import threading
import configparser
from pathlib import Path
import re

# ── pyra_env path setup ──────────────────────────────────────────────────────
PYRA_ENV = Path.home() / "pyra_env"
PYRA_LIB = Path.home() / "cyon" / "pyra_tool"
site_pkgs = list(PYRA_ENV.glob("lib/python3*/site-packages"))
if site_pkgs:
    sys.path.insert(0, str(site_pkgs[0]))
sys.path.append(str(PYRA_LIB))

NOTES_DIR   = os.path.expanduser("~/Documents/pyra_dev_notes")
CONFIG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "pyra_notes.conf")

# ── Syntax highlighting defaults (written to pyra_notes.conf if absent) ─────
# Each highlight_group_N section needs: keywords = ... and color = #rrggbb
# Add as many groups as you like in pyra_notes.conf — no code changes needed.

HIGHLIGHT_GROUP_DEFAULTS = [
    {"section": "highlight_group_1", "keywords": "print for while if elif else with finally", "color": "#ffb000"},
    {"section": "highlight_group_2", "keywords": "def class return import from",              "color": "#5fd7ff"},
    {"section": "highlight_group_3", "keywords": "try except self",                           "color": "#8fd3ff"},
]

# Special (non-keyword) highlight defaults — still conf-driven but not generic groups
HIGHLIGHT_SPECIAL_DEFAULTS = {
    "color_lime": "#c8ff00",   # class name after 'class'
    "color_coral": "#ff9966",   # string contents
    "color_comment": "#6a5acd",   # # and // comments
    "color_dot_left": "#ffffff",   # object side of dot notation
    "color_dot_right": "#ffb000",   # method side of dot notation
}

VOICES = {
    "joe": {
        "label": "JOE  ♂",
        "model": "en_US-joe-medium.onnx",
        "config": "en_US-joe-medium.onnx.json",
    },
    "lessac": {
        "label": "LESSAC ♀",
        "model": "en_US-lessac-medium.onnx",
        "config": "en_US-lessac-medium.onnx.json",
    },
}

CSS = b"""
window, .background { background-color: #0a0a0f; }

label {
    color: #00cc77;
    font-family: monospace;
    font-size: 11px;
}
.title-label {
    color: #00ff99;
    font-family: monospace;
    font-size: 15px;
    font-weight: bold;
    letter-spacing: 3px;
    padding: 4px 0;
}
.section-label {
    color: #336655;
    font-family: monospace;
    font-size: 10px;
    letter-spacing: 2px;
}
.voice-label {
    color: #336655;
    font-family: monospace;
    font-size: 11px;
    min-width: 72px;
}
.voice-label-active {
    color: #00ff99;
    font-family: monospace;
    font-size: 11px;
    font-weight: bold;
    min-width: 72px;
}
.status-bar {
    color: #335544;
    font-family: monospace;
    font-size: 10px;
    padding: 2px 8px;
    background-color: #080810;
    border-top: 1px solid #1a2a20;
}
entry {
    background-color: #0d0d15;
    color: #00ff99;
    font-family: monospace;
    font-size: 13px;
    border: 1px solid #1a2a20;
    border-radius: 0px;
    padding: 4px;
}
entry:focus { border-color: #E8A020; }
button {
    background-color: #0d0d15;
    color: #00cc77;
    font-family: monospace;
    font-size: 12px;
    border: 1px solid #1a2a20;
    border-radius: 0px;
    padding: 5px 14px;
}
button:hover {
    background-color: #003322;
    color: #00ff99;
    border-color: #E8A020;
}
.btn-action {
    background-color: #003322;
    color: #00ff99;
    border: 1px solid #00ff99;
    font-family: monospace;
    font-size: 12px;
    padding: 5px 14px;
}
.btn-action:hover { background-color: #004433; }
.btn-danger {
    background-color: #1a0800;
    color: #E8A020;
    border: 1px solid #E8A020;
    font-family: monospace;
    font-size: 12px;
    padding: 5px 14px;
}
.btn-danger:hover { background-color: #2a1200; }
.btn-tts {
    background-color: #221100;
    color: #E8A020;
    border: 1px solid #E8A020;
    font-family: monospace;
    font-size: 12px;
    padding: 5px 14px;
}
.btn-tts:hover {
    background-color: #331a00;
    color: #ffb84d;
    border-color: #ffb84d;
}
.btn-menu {
    background-color: #0d0d15;
    color: #00cc77;
    font-family: monospace;
    font-size: 12px;
    border: 1px solid #1a2a20;
    border-radius: 0px;
    padding: 5px 14px;
}
.btn-menu:hover {
    background-color: #003322;
    color: #00ff99;
    border-color: #E8A020;
}
scrolledwindow { border: 1px solid #1a2a20; }
notebook > header {
    background-color: #080810;
    border-bottom: 1px solid #1a2a20;
    padding: 0;
}
notebook > header > tabs > tab {
    background-color: #0d0d15;
    color: #336655;
    font-family: monospace;
    font-size: 11px;
    padding: 4px 12px;
    border: 1px solid #1a2a20;
    border-bottom: none;
    margin-right: 2px;
}
notebook > header > tabs > tab:checked {
    background-color: #05050a;
    color: #00ff99;
    border-color: #E8A020;
}
.tab-close-x {
    color: #336655;
    font-size: 10px;
    padding: 0 2px;
}
.tab-close-x:hover { color: #ff4444; }
textview, textview text {
    background-color: #05050a;
    color: #00cc77;
    font-family: monospace;
    font-size: 15px;
}
separator { background-color: #1a2a20; }
#line-numbers {
    background-color: #05050a;
    color: #1a4a3a;
    font-family: monospace;
    font-size: 15px;
    padding-right: 6px;
    border-right: 1px solid #1a2a20;
}
.tree-panel {
    background-color: #08080e;
    border-left: 1px solid #1a2a20;
}
.tree-header {
    color: #336655;
    font-family: monospace;
    font-size: 10px;
    letter-spacing: 2px;
    padding: 4px 6px;
    background-color: #080810;
    border-bottom: 1px solid #1a2a20;
}
treeview {
    background-color: #08080e;
    color: #00cc77;
    font-family: monospace;
    font-size: 12px;
}
treeview:selected {
    background-color: #003322;
    color: #00ff99;
}
.btn-tree {
    background-color: #08080e;
    color: #336655;
    font-family: monospace;
    font-size: 10px;
    border: 1px solid #1a2a20;
    border-radius: 0px;
    padding: 3px 8px;
    letter-spacing: 1px;
}
.btn-tree:hover {
    background-color: #003322;
    color: #00ff99;
    border-color: #E8A020;
}
"""


def ensure_notes_dir():
    os.makedirs(NOTES_DIR, exist_ok=True)


def load_config():
    cfg = configparser.ConfigParser()
    cfg.read(CONFIG_PATH)
    changed = False

    # seed default keyword groups if missing
    for grp in HIGHLIGHT_GROUP_DEFAULTS:
        sec = grp["section"]
        if not cfg.has_section(sec):
            cfg.add_section(sec)
        if not cfg.has_option(sec, "keywords"):
            cfg.set(sec, "keywords", grp["keywords"])
            changed = True
        if not cfg.has_option(sec, "color"):
            cfg.set(sec, "color", grp["color"])
            changed = True

    # seed special (non-keyword) highlight values
    if not cfg.has_section("highlight_special"):
        cfg.add_section("highlight_special")
    for key, val in HIGHLIGHT_SPECIAL_DEFAULTS.items():
        if not cfg.has_option("highlight_special", key):
            cfg.set("highlight_special", key, val)
            changed = True

    if changed:
        save_config(cfg)
    return cfg


def save_config(cfg):
    with open(CONFIG_PATH, "w") as f:
        cfg.write(f)


class EditorTab:
    """All per-tab state: buffer, views, undo stack, current file path."""

    def __init__(self, hl_groups, hl_special):
        self._hl_groups  = hl_groups   # list of (keywords, color)
        self._hl_special = hl_special  # dict of role -> color

        self.current_file  = None
        self._undo_stack   = []
        self._redo_stack   = []
        self._undo_inhibit = False

        # line number view
        self.line_buf  = Gtk.TextBuffer()
        self.line_view = Gtk.TextView(buffer=self.line_buf)
        self.line_view.set_name("line-numbers")
        self.line_view.set_editable(False)
        self.line_view.set_cursor_visible(False)
        self.line_view.set_wrap_mode(Gtk.WrapMode.NONE)
        self.line_view.set_left_margin(6)
        self.line_view.set_right_margin(6)
        self.line_view.set_can_focus(False)

        # editor view
        self.text_view   = Gtk.TextView()
        self.text_view.set_name("editor-view")
        self.text_view.set_wrap_mode(Gtk.WrapMode.NONE)
        self.text_view.set_left_margin(6)
        self.text_buffer = self.text_view.get_buffer()

        # highlight tags (one set per buffer)
        self.kw_tags = []
        for keywords, color in self._hl_groups:
            tag = self.text_buffer.create_tag(None, foreground=color)
            self.kw_tags.append((keywords, tag))

        def _sp(key):
            return self._hl_special.get(key, "#ffffff")

        self.tag_lime    = self.text_buffer.create_tag(None, foreground=_sp("color_lime"))
        self.tag_coral   = self.text_buffer.create_tag(None, foreground=_sp("color_coral"))
        self.tag_comment = self.text_buffer.create_tag(None, foreground=_sp("color_comment"))
        self.tag_dot_l   = self.text_buffer.create_tag(None, foreground=_sp("color_dot_left"))
        self.tag_dot_r   = self.text_buffer.create_tag(None, foreground=_sp("color_dot_right"))

        # scrolled window
        self.scroll = Gtk.ScrolledWindow()
        self.scroll.set_min_content_height(340)
        self.scroll.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)

        editor_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
        editor_box.pack_start(self.line_view, False, False, 0)
        editor_box.pack_start(self.text_view, True,  True,  0)
        self.scroll.add(editor_box)

        self.scroll.get_vadjustment().connect("value-changed", self._sync_line_scroll)
        self.text_buffer.connect("changed", self._on_text_changed)
        self._update_line_numbers()

    def _update_line_numbers(self, *_):
        count = self.text_buffer.get_line_count()
        width = len(str(count))
        nums  = "\n".join(str(i).rjust(width) for i in range(1, count + 1))
        self.line_buf.set_text(nums)

    def _sync_line_scroll(self, *_):
        val  = self.scroll.get_vadjustment().get_value()
        ladj = self.line_view.get_vadjustment()
        if ladj:
            ladj.set_value(val)

    def _on_text_changed(self, buf):
        if not self._undo_inhibit:
            start, end = buf.get_bounds()
            snapshot = buf.get_text(start, end, True)
            if not self._undo_stack or self._undo_stack[-1] != snapshot:
                self._undo_stack.append(snapshot)
                if len(self._undo_stack) > 300:
                    self._undo_stack.pop(0)
                self._redo_stack.clear()
        self.apply_highlighting()
        self._update_line_numbers()

    def undo(self):
        if len(self._undo_stack) < 2:
            return False
        current = self._undo_stack.pop()
        self._redo_stack.append(current)
        self._undo_inhibit = True
        self.text_buffer.set_text(self._undo_stack[-1])
        self._undo_inhibit = False
        return True

    def redo(self):
        if not self._redo_stack:
            return False
        state = self._redo_stack.pop()
        self._undo_stack.append(state)
        self._undo_inhibit = True
        self.text_buffer.set_text(state)
        self._undo_inhibit = False
        return True

    def apply_highlighting(self):
        buf        = self.text_buffer
        start, end = buf.get_bounds()
        for _, tag in self.kw_tags:
            buf.remove_tag(tag, start, end)
        for tag in (self.tag_lime, self.tag_coral, self.tag_comment,
                    self.tag_dot_l, self.tag_dot_r):
            buf.remove_tag(tag, start, end)
        text = buf.get_text(start, end, True)
        for keywords, tag in self.kw_tags:
            for kw in keywords:
                for m in re.finditer(rf"\b{re.escape(kw)}\b", text):
                    buf.apply_tag(tag,
                        buf.get_iter_at_offset(m.start()),
                        buf.get_iter_at_offset(m.end()))
        if len(self.kw_tags) >= 3:
            steel_tag = self.kw_tags[2][1]
            for m in re.finditer(r"(?<![=!<>])=(?!=)", text):
                buf.apply_tag(steel_tag,
                    buf.get_iter_at_offset(m.start()),
                    buf.get_iter_at_offset(m.end()))
            for m in re.finditer(r"@staticmethod\b", text):
                buf.apply_tag(steel_tag,
                    buf.get_iter_at_offset(m.start()),
                    buf.get_iter_at_offset(m.end()))
        for m in re.finditer(r"\bclass\s+(\w+)", text):
            buf.apply_tag(self.tag_lime,
                buf.get_iter_at_offset(m.start(1)),
                buf.get_iter_at_offset(m.end(1)))
        for m in re.finditer(r'([\'"])(.*?)(\1)', text):
            content      = m.group(2)
            start_offset = m.start(2)
            for w in re.finditer(r'\S+', content):
                buf.apply_tag(self.tag_coral,
                    buf.get_iter_at_offset(start_offset + w.start()),
                    buf.get_iter_at_offset(start_offset + w.end()))
        for m in re.finditer(r'\b(\w+)\.(\w+)\b', text):
            buf.apply_tag(self.tag_dot_l,
                buf.get_iter_at_offset(m.start(1)),
                buf.get_iter_at_offset(m.end(1)))
            buf.apply_tag(self.tag_dot_r,
                buf.get_iter_at_offset(m.start(2)),
                buf.get_iter_at_offset(m.end(2)))
        for m in re.finditer(r'(#[^\n]*|//[^\n]*)', text):
            buf.apply_tag(self.tag_comment,
                buf.get_iter_at_offset(m.start()),
                buf.get_iter_at_offset(m.end()))

    def load_file(self, path):
        with open(path, "r", encoding="utf-8") as f:
            content = f.read()
        self._undo_inhibit = True
        self.text_buffer.set_text(content)
        self._undo_inhibit = False
        self._undo_stack   = [content]
        self._redo_stack   = []
        self.apply_highlighting()
        self._update_line_numbers()
        self.current_file = path

    @property
    def is_dirty(self):
        """True if buffer differs from the last saved snapshot (undo_stack[0])."""
        if not self._undo_stack:
            return False
        start, end = self.text_buffer.get_bounds()
        return self.text_buffer.get_text(start, end, True) != self._undo_stack[0]

    def mark_clean(self):
        """Reset the saved snapshot to current content (called after save)."""
        start, end = self.text_buffer.get_bounds()
        current = self.text_buffer.get_text(start, end, True)
        if self._undo_stack:
            self._undo_stack[0] = current
        else:
            self._undo_stack = [current]

    @property
    def display_name(self):
        if not self.current_file:
            return "untitled"
        basename = os.path.basename(self.current_file)
        return basename[:-4] if basename.endswith(".txt") else basename


class PyraNotesWindow(Gtk.Window):
    def __init__(self):
        super().__init__(title="▸ PYRA NOTES // TTS")
        self.set_default_size(900, 660)
        self.set_border_width(10)

        self.current_voice = "joe"
        self.text_size     = 15
        self._tree_monitor = None
        self._tts_procs    = []

        # ── load config ──────────────────────────────────────────────────
        self._cfg = load_config()
        self._tree_folder = self._cfg.get("ui", "tree_folder", fallback=NOTES_DIR)

        # build highlight group list (keywords, color) from conf
        self._hl_groups = []
        for sec in sorted(self._cfg.sections()):
            if not sec.startswith("highlight_group_"):
                continue
            color    = self._cfg.get(sec, "color",    fallback="#ffffff")
            keywords = self._cfg.get(sec, "keywords", fallback="").split()
            if keywords:
                self._hl_groups.append((keywords, color))

        def _sp(key):
            return self._cfg.get("highlight_special", key,
                                 fallback=HIGHLIGHT_SPECIAL_DEFAULTS[key])
        self._hl_special = {k: _sp(k) for k in HIGHLIGHT_SPECIAL_DEFAULTS}

        # ── CSS ──────────────────────────────────────────────────────────
        provider = Gtk.CssProvider()
        provider.load_from_data(CSS)
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(),
            provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION,
        )

        outer = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        self.add(outer)

        # ── Title ────────────────────────────────────────────────────────
        title = Gtk.Label(label="▸ PYRA DEV NOTES // TTS")
        title.get_style_context().add_class("title-label")
        title.set_halign(Gtk.Align.START)
        outer.pack_start(title, False, False, 0)

        outer.pack_start(Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL), False, False, 2)

        # ── File toolbar ─────────────────────────────────────────────────
        file_bar = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=6)
        outer.pack_start(file_bar, False, False, 0)

        self.filename_entry = Gtk.Entry()
        self.filename_entry.set_placeholder_text("filename  (no extension — .txt auto-added)")
        file_bar.pack_start(self.filename_entry, True, True, 0)

        # ── FILE dropdown menu ────────────────────────────────────────────
        self._file_menu = Gtk.Menu()

        for label, cb in [
            ("NEW TAB", self._on_new_tab_menu),
            ("NEW", self.on_new),
            ("LOAD", self.on_load),
            ("SAVE", self.on_save),
            ("SAVE AS", self.on_save_as),
            (None, None),
            ("UNDO  Ctrl+Z", self.on_undo),
            ("REDO  Ctrl+Y", self.on_redo),
            (None, None),
            ("DELETE FILE", self.on_delete),
            (None, None),
            ("TEXT  +", self.on_text_size_increase),
            ("TEXT  −", self.on_text_size_decrease),
            (None, None),
            ("HIDE TREE", self._on_tree_toggle),
        ]:
            if label is None:
                self._file_menu.append(Gtk.SeparatorMenuItem())
            else:
                item = Gtk.MenuItem(label=label)
                item.connect("activate", cb)
                self._file_menu.append(item)
                if label == "HIDE TREE":
                    self._tree_toggle_item = item  # keep ref to update label
        self._file_menu.show_all()

        menu_btn = Gtk.Button(label="FILE")
        menu_btn.get_style_context().add_class("btn-menu")
        menu_btn.connect("clicked", self._on_file_menu_clicked)
        file_bar.pack_start(menu_btn, False, False, 0)

        # ── Notebook (tabs) ───────────────────────────────────────────────
        self._notebook = Gtk.Notebook()
        self._notebook.set_scrollable(True)
        self._notebook.set_show_border(False)
        self._notebook.connect("switch-page", self._on_tab_switched)

        # open one blank tab to start
        self._new_tab()

        # ── File tree panel ───────────────────────────────────────────────
        self._tree_panel = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        self._tree_panel.get_style_context().add_class("tree-panel")
        tree_panel = self._tree_panel

        # header row: label + folder picker button
        tree_hdr = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=4)
        tree_hdr.set_border_width(4)
        tree_lbl = Gtk.Label(label="▸ FILES")
        tree_lbl.get_style_context().add_class("tree-header")
        tree_lbl.set_halign(Gtk.Align.START)
        tree_hdr.pack_start(tree_lbl, True, True, 0)

        btn_pick = Gtk.Button(label="⊞ FOLDER")
        btn_pick.get_style_context().add_class("btn-tree")
        btn_pick.connect("clicked", self._on_tree_pick_folder)
        tree_hdr.pack_start(btn_pick, False, False, 0)

        btn_refresh = Gtk.Button(label="⟳")
        btn_refresh.get_style_context().add_class("btn-tree")
        btn_refresh.connect("clicked", self._on_tree_refresh)
        tree_hdr.pack_start(btn_refresh, False, False, 0)
        tree_panel.pack_start(tree_hdr, False, False, 0)

        # TreeView
        self._tree_store = Gtk.TreeStore(str, str)  # col0=display name, col1=full path
        self._tree_view  = Gtk.TreeView(model=self._tree_store)
        self._tree_view.set_headers_visible(False)
        self._tree_view.set_enable_tree_lines(True)
        renderer = Gtk.CellRendererText()
        col = Gtk.TreeViewColumn("File", renderer, text=0)
        self._tree_view.append_column(col)
        self._tree_view.connect("row-activated", self._on_tree_row_activated)

        tree_scroll = Gtk.ScrolledWindow()
        tree_scroll.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)
        tree_scroll.add(self._tree_view)
        tree_panel.pack_start(tree_scroll, True, True, 0)

        # ── Paned: editor left, tree right ───────────────────────────────
        paned = Gtk.Paned(orientation=Gtk.Orientation.HORIZONTAL)
        paned.pack1(self._notebook, True, True)
        paned.pack2(tree_panel, False, False)
        paned.set_position(580)
        outer.pack_start(paned, True, True, 0)

        # populate tree with last used folder
        self._populate_tree(self._tree_folder)

        outer.pack_start(Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL), False, False, 2)

        # ── TTS row ──────────────────────────────────────────────────────
        tts_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        tts_row.set_border_width(2)
        outer.pack_start(tts_row, False, False, 0)

        tts_head = Gtk.Label(label="TTS ▸")
        tts_head.get_style_context().add_class("section-label")
        tts_row.pack_start(tts_head, False, False, 4)

        # JOE ◀switch▶ LESSAC
        self.lbl_joe = Gtk.Label(label="JOE  ♂")
        self.lbl_joe.get_style_context().add_class("voice-label-active")
        self.lbl_joe.set_halign(Gtk.Align.END)
        tts_row.pack_start(self.lbl_joe, False, False, 0)

        self.voice_switch = Gtk.Switch()
        self.voice_switch.set_active(False)
        self.voice_switch.connect("notify::active", self.on_voice_switched)
        tts_row.pack_start(self.voice_switch, False, False, 4)

        self.lbl_lessac = Gtk.Label(label="LESSAC ♀")
        self.lbl_lessac.get_style_context().add_class("voice-label")
        self.lbl_lessac.set_halign(Gtk.Align.START)
        tts_row.pack_start(self.lbl_lessac, False, False, 0)

        # push buttons to the right
        tts_row.pack_start(Gtk.Label(label=""), True, True, 0)

        src_lbl = Gtk.Label(label="SPEAK:")
        src_lbl.get_style_context().add_class("section-label")
        tts_row.pack_start(src_lbl, False, False, 0)

        btn_speak_sel = Gtk.Button(label="SELECTION")
        btn_speak_sel.get_style_context().add_class("btn-tts")
        btn_speak_sel.connect("clicked", self.on_speak_selection)
        tts_row.pack_start(btn_speak_sel, False, False, 0)

        btn_speak_all = Gtk.Button(label="■ ALL TEXT")
        btn_speak_all.get_style_context().add_class("btn-tts")
        btn_speak_all.connect("clicked", self.on_speak_all)
        tts_row.pack_start(btn_speak_all, False, False, 0)

        btn_stop = Gtk.Button(label="✕ STOP")
        btn_stop.get_style_context().add_class("btn-danger")
        btn_stop.connect("clicked", self.on_tts_stop)
        tts_row.pack_start(btn_stop, False, False, 0)

        # ── Log terminal ─────────────────────────────────────────────────
        log_scroll = Gtk.ScrolledWindow()
        log_scroll.set_min_content_height(72)
        self.log_view = Gtk.TextView()
        self.log_view.set_editable(False)
        self.log_view.set_cursor_visible(False)
        self.log_view.set_wrap_mode(Gtk.WrapMode.WORD_CHAR)
        self.log_buf = self.log_view.get_buffer()
        self._log_tag_amber = self.log_buf.create_tag("amber", foreground="#E8A020")
        log_scroll.add(self.log_view)
        outer.pack_start(log_scroll, False, False, 0)

        # ── Status bar ───────────────────────────────────────────────────
        bottom_bar = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
        self._ram_area = Gtk.DrawingArea()
        self._ram_area.set_size_request(180, 28)
        self._ram_area.connect("draw", self._on_ram_draw)
        bottom_bar.pack_start(self._ram_area, False, False, 0)
        self.status = Gtk.Label(label=f"dir: {NOTES_DIR}")
        self.status.get_style_context().add_class("status-bar")
        self.status.set_halign(Gtk.Align.START)
        bottom_bar.pack_start(self.status, True, True, 4)
        outer.pack_start(bottom_bar, False, False, 0)

        self._ram_samples = []
        self._ram_timer   = None
        self.connect("destroy", self._on_destroy)
        self.show_all()
        self._ram_timer = GLib.timeout_add_seconds(1, self._ram_tick)
        GLib.idle_add(self._boot_sequence)

    # ── tab management ───────────────────────────────────────────────────

    def _tab(self, index=None):
        """Return the EditorTab for the given page index (default: current)."""
        if index is None:
            index = self._notebook.get_current_page()
        return self._notebook.get_nth_page(index)._tab

    def _new_tab(self, path=None):
        """Create a new EditorTab, add it to the notebook, return the tab."""
        tab = EditorTab(self._hl_groups, self._hl_special)
        tab.text_view.connect("key-press-event", self._on_key_press)
        tab.text_view.connect("scroll-event",    self._on_scroll_zoom)

        # tab label: filename + close button
        label_box  = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=4)
        label_text = Gtk.Label(label="untitled")
        label_text.set_name("tab-label-text")
        btn_close  = Gtk.Button()
        btn_close.set_relief(Gtk.ReliefStyle.NONE)
        btn_close.set_focus_on_click(False)
        x_lbl = Gtk.Label(label="✕")
        x_lbl.get_style_context().add_class("tab-close-x")
        btn_close.add(x_lbl)
        label_box.pack_start(label_text, True, True, 0)
        label_box.pack_start(btn_close,  False, False, 0)
        label_box.show_all()

        # the page widget carries a reference to the tab object
        tab.scroll._tab = tab
        page_idx = self._notebook.append_page(tab.scroll, label_box)
        self._notebook.set_tab_reorderable(tab.scroll, True)
        tab._label_text = label_text

        btn_close.connect("clicked", self._on_tab_close, tab.scroll)

        if path:
            tab.load_file(path)
            label_text.set_text(tab.display_name)

        self._notebook.set_current_page(page_idx)
        self._notebook.show_all()

        if path:
            self.filename_entry.set_text(tab.display_name)
        else:
            self.filename_entry.set_text("")

        GLib.idle_add(tab.text_view.grab_focus)
        return tab

    def _update_tab_label(self, index, tab):
        label = tab._label_text
        label.set_text(tab.display_name)

    def _on_tab_switched(self, notebook, page, index):
        tab = self._tab(index)
        self.filename_entry.set_text(tab.display_name if tab.current_file else "")

    def _on_tab_close(self, btn, page_widget):
        idx = self._notebook.page_num(page_widget)
        tab = page_widget._tab
        if not self._confirm_close(tab):
            return
        if self._notebook.get_n_pages() == 1:
            # last tab — clear instead of removing
            tab.current_file = None
            tab._undo_stack  = []
            tab._redo_stack  = []
            tab.text_buffer.set_text("")
            tab._label_text.set_text("untitled")
            self.filename_entry.set_text("")
            self.log("▸ Tab cleared.")
        else:
            self._notebook.remove_page(idx)

    def _confirm_close(self, tab):
        """If tab has unsaved changes, ask user. Returns True if safe to proceed."""
        if not tab.is_dirty:
            return True
        name = tab.display_name
        dlg = Gtk.MessageDialog(
            transient_for=self, flags=0,
            message_type=Gtk.MessageType.WARNING,
            buttons=Gtk.ButtonsType.NONE,
            text=f"▸ '{name}' has unsaved changes.",
        )
        dlg.format_secondary_text("Save before closing?")
        dlg.add_button("Discard", Gtk.ResponseType.REJECT)
        dlg.add_button("Cancel",  Gtk.ResponseType.CANCEL)
        dlg.add_button("Save",    Gtk.ResponseType.ACCEPT)
        dlg.set_default_response(Gtk.ResponseType.ACCEPT)
        resp = dlg.run()
        dlg.destroy()
        if resp == Gtk.ResponseType.ACCEPT:
            self.on_save(None)
            return True
        if resp == Gtk.ResponseType.REJECT:
            return True
        return False  # Cancel

    # ── file tree ─────────────────────────────────────────────────────────

    def _populate_tree(self, folder):
        """Build the TreeStore from the given folder path."""
        self._tree_store.clear()
        if not os.path.isdir(folder):
            return
        self._tree_folder = folder
        # persist tree_folder — re-read conf first to avoid wiping user edits
        fresh = configparser.ConfigParser()
        fresh.read(CONFIG_PATH)
        if not fresh.has_section("ui"):
            fresh.add_section("ui")
        fresh.set("ui", "tree_folder", folder)
        save_config(fresh)

        root_label = os.path.basename(folder) or folder
        root_iter  = self._tree_store.append(None, [f"📁 {root_label}", folder])
        self._add_tree_children(root_iter, folder)
        self._tree_view.expand_row(
            self._tree_store.get_path(root_iter), False
        )

        # ── start/restart file system monitor ────────────────────────────
        if self._tree_monitor:
            self._tree_monitor.cancel()
        gfile = Gio.File.new_for_path(folder)
        self._tree_monitor = gfile.monitor_directory(
            Gio.FileMonitorFlags.WATCH_MOVES, None
        )
        self._tree_monitor.connect("changed", self._on_folder_changed)

    def _add_tree_children(self, parent_iter, folder):
        """Recursively add dirs then files under parent_iter."""
        try:
            entries = sorted(os.scandir(folder), key=lambda e: (not e.is_dir(), e.name.lower()))
        except PermissionError:
            return
        for entry in entries:
            if entry.name.startswith("."):
                continue
            if entry.is_dir(follow_symlinks=False):
                child = self._tree_store.append(parent_iter, [f"▸ {entry.name}", entry.path])
                self._add_tree_children(child, entry.path)
            else:
                self._tree_store.append(parent_iter, [entry.name, entry.path])

    def _on_tree_refresh(self, _btn):
        self._populate_tree(self._tree_folder)
        self.log(f"▸ Tree refreshed: {self._tree_folder}")

    def _on_folder_changed(self, _monitor, _file, _other, _event_type):
        """Debounced auto-refresh: wait 400 ms after last event before redrawing."""
        if hasattr(self, "_refresh_timer") and self._refresh_timer:
            GLib.source_remove(self._refresh_timer)
        self._refresh_timer = GLib.timeout_add(400, self._debounced_tree_refresh)

    def _debounced_tree_refresh(self):
        self._refresh_timer = None
        self._populate_tree(self._tree_folder)
        return False  # don't repeat

    def _on_tree_pick_folder(self, _btn):
        dialog = Gtk.FileChooserDialog(
            title="Choose Folder", parent=self,
            action=Gtk.FileChooserAction.SELECT_FOLDER,
        )
        dialog.add_buttons(
            Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
            Gtk.STOCK_OPEN,   Gtk.ResponseType.OK,
        )
        dialog.set_current_folder(self._tree_folder)
        if dialog.run() == Gtk.ResponseType.OK:
            self._populate_tree(dialog.get_filename())
        dialog.destroy()

    def _on_tree_row_activated(self, tree_view, path, _col):
        it   = self._tree_store.get_iter(path)
        fpath = self._tree_store.get_value(it, 1)
        if os.path.isfile(fpath):
            try:
                tab = self._new_tab(fpath)
                self.log(f"▸ Loaded: {fpath}")
            except Exception as e:
                self.log(f"▸ ERROR loading: {e}")

    def _on_tree_toggle(self, _item):
        if self._tree_panel.get_visible():
            self._tree_panel.hide()
            self._tree_toggle_item.set_label("SHOW TREE")
        else:
            self._tree_panel.show()
            self._tree_toggle_item.set_label("HIDE TREE")

    # ── file menu ─────────────────────────────────────────────────────────

    def _on_file_menu_clicked(self, btn):
        self._file_menu.popup_at_widget(
            btn, Gdk.Gravity.SOUTH_WEST, Gdk.Gravity.NORTH_WEST, None
        )

    # ── keyboard shortcuts ────────────────────────────────────────────────

    def _on_key_press(self, widget, event):
        ctrl = event.state & Gdk.ModifierType.CONTROL_MASK

        # ── Ctrl+S — save ─────────────────────────────────────────────────
        if ctrl and event.keyval == Gdk.KEY_s:
            self.on_save(None)
            return True

        # ── Ctrl+T — new tab ──────────────────────────────────────────────
        if ctrl and event.keyval == Gdk.KEY_t:
            self._new_tab()
            self.log("▸ New tab.")
            self.filename_entry.grab_focus()
            return True

        # ── Ctrl+Z — undo ─────────────────────────────────────────────────
        if ctrl and event.keyval == Gdk.KEY_z:
            self.on_undo(None)
            return True

        # ── Ctrl+Y — redo ─────────────────────────────────────────────────
        if ctrl and event.keyval == Gdk.KEY_y:
            self.on_redo(None)
            return True

        # ── Ctrl++ / Ctrl+= — text size up ────────────────────────────────
        if ctrl and event.keyval in (Gdk.KEY_plus, Gdk.KEY_equal, Gdk.KEY_KP_Add):
            self.on_text_size_increase(None)
            return True

        # ── Ctrl+- — text size down ───────────────────────────────────────
        if ctrl and event.keyval in (Gdk.KEY_minus, Gdk.KEY_KP_Subtract):
            self.on_text_size_decrease(None)
            return True

        # ── Tab — insert 4 spaces ─────────────────────────────────────────
        if event.keyval == Gdk.KEY_Tab:
            self._tab().text_buffer.insert_at_cursor("    ")
            return True

        # ── Auto-close brackets & quotes ──────────────────────────────────
        _PAIRS = {
            Gdk.KEY_parenleft:    ("(", ")"),
            Gdk.KEY_bracketleft:  ("[", "]"),
            Gdk.KEY_braceleft:    ("{", "}"),
            Gdk.KEY_apostrophe:   ("'", "'"),
            Gdk.KEY_quotedbl:     ('"', '"'),
        }
        if not ctrl and event.keyval in _PAIRS:
            open_ch, close_ch = _PAIRS[event.keyval]
            buf    = self._tab().text_buffer
            cursor = buf.get_iter_at_mark(buf.get_insert())
            # if next char is already the closing pair, just skip over it
            next_iter = cursor.copy()
            if not next_iter.is_end():
                next_iter.forward_char()
                next_ch = buf.get_text(cursor, next_iter, False)
                if next_ch == close_ch:
                    buf.place_cursor(next_iter)
                    return True
            # insert both chars and place cursor between them
            buf.insert_at_cursor(open_ch + close_ch)
            new_cursor = buf.get_iter_at_mark(buf.get_insert())
            new_cursor.backward_char()
            buf.place_cursor(new_cursor)
            return True

        # ── Backspace — remove auto-closed pair if cursor is between them ─
        if event.keyval == Gdk.KEY_BackSpace:
            buf    = self._tab().text_buffer
            cursor = buf.get_iter_at_mark(buf.get_insert())
            prev   = cursor.copy()
            if prev.backward_char():
                nxt = cursor.copy()
                if not nxt.is_end():
                    nxt.forward_char()
                    left  = buf.get_text(prev, cursor, False)
                    right = buf.get_text(cursor, nxt,    False)
                    if (left, right) in [("(", ")"), ("[", "]"),
                                         ("{", "}"), ("'", "'"), ('"', '"')]:
                        buf.delete(prev, nxt)
                        return True

        # ── Enter — smart indent ──────────────────────────────────────────
        if event.keyval in (Gdk.KEY_Return, Gdk.KEY_KP_Enter):
            buf    = self._tab().text_buffer
            cursor = buf.get_iter_at_mark(buf.get_insert())

            line_start = cursor.copy()
            line_start.set_line_offset(0)
            line_text = buf.get_text(line_start, cursor, False)

            stripped     = line_text.lstrip()
            base_indent  = len(line_text) - len(stripped)
            indent       = " " * base_indent

            INDENT_TRIGGERS = {"if", "elif", "else", "except", "def",
                               "for", "while", "with", "try", "finally",
                               "class"}
            first_word = stripped.split()[0].rstrip("(") if stripped.split() else ""
            if stripped.endswith(":") and first_word in INDENT_TRIGGERS:
                indent += "    "

            buf.insert_at_cursor("\n" + indent)
            return True

        return False

    # ── text size ─────────────────────────────────────────────────────────

    def _apply_text_size(self):
        """Push the current text_size into the editor textview only."""
        css = f"""
#editor-view, #editor-view text {{ font-size: {self.text_size}px; }}
#line-numbers, #line-numbers text {{ font-size: {self.text_size}px; }}
""".encode()
        if not hasattr(self, "_size_provider"):
            self._size_provider = Gtk.CssProvider()
            Gtk.StyleContext.add_provider_for_screen(
                Gdk.Screen.get_default(),
                self._size_provider,
                Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION + 1,
            )
        self._size_provider.load_from_data(css)
        self.log(f"▸ Text size: {self.text_size}px")

    def on_text_size_increase(self, _item):
        self.text_size = min(self.text_size + 2, 36)
        self._apply_text_size()

    def on_text_size_decrease(self, _item):
        self.text_size = max(self.text_size - 2, 8)
        self._apply_text_size()

    # ── undo / redo ───────────────────────────────────────────────────────

    def on_undo(self, _item):
        if self._tab().undo():
            self.log("▸ Undo")
        else:
            self.log("▸ Nothing to undo.")

    def on_redo(self, _item):
        if self._tab().redo():
            self.log("▸ Redo")
        else:
            self.log("▸ Nothing to redo.")

    # ── log / status ─────────────────────────────────────────────────────

    def log(self, msg):
        """Append a line to the log terminal and also update the status bar."""
        end = self.log_buf.get_end_iter()
        self.log_buf.insert(end, msg + "\n")
        self._log_scroll()
        self.status.set_text(msg)

    def _log_scroll(self):
        mark = self.log_buf.get_mark("log-end")
        if not mark:
            mark = self.log_buf.create_mark("log-end",
                                            self.log_buf.get_end_iter(), False)
        else:
            self.log_buf.move_mark(mark, self.log_buf.get_end_iter())
        GLib.idle_add(self._log_scroll_now, mark)

    def _log_scroll_now(self, mark):
        self.log_view.scroll_to_mark(mark, 0.0, True, 0.0, 1.0)
        return False

    def _log_amber(self, msg):
        start_offset = self.log_buf.get_char_count()
        end = self.log_buf.get_end_iter()
        self.log_buf.insert(end, msg + "\n")
        start_it = self.log_buf.get_iter_at_offset(start_offset)
        end_it   = self.log_buf.get_end_iter()
        self.log_buf.apply_tag(self._log_tag_amber, start_it, end_it)
        self._log_scroll()
        self.status.set_text(msg)

    # ── RAM graph ─────────────────────────────────────────────────────────

    def _ram_tick(self):
        mb = 0.0
        try:
            with open("/proc/self/status") as f:
                for line in f:
                    if line.startswith("VmRSS:"):
                        mb = float(line.split()[1]) / 1024.0
                        break
        except OSError:
            pass
        self._ram_samples.append(mb)
        if len(self._ram_samples) > 120:
            self._ram_samples.pop(0)
        self._ram_area.queue_draw()
        return True

    def _on_ram_draw(self, widget, cr):
        import cairo
        w = widget.get_allocated_width()
        h = widget.get_allocated_height()
        cr.set_source_rgb(0.012, 0.012, 0.02)
        cr.paint()
        n = len(self._ram_samples)
        if n < 2:
            return False
        mn = min(self._ram_samples)
        mx = max(self._ram_samples)
        rng = max(mx - mn, 1.0)
        def yx(i):
            v = self._ram_samples[i]
            x = i / (n - 1) * w
            y = h - ((v - mn) / rng) * (h - 4) - 2
            return x, y
        # grid
        cr.set_line_width(0.5)
        cr.set_source_rgba(0.0, 0.6, 0.2, 0.18)
        for g in range(1, 4):
            gy = h - (h * g / 4.0)
            cr.move_to(0, gy); cr.line_to(w, gy); cr.stroke()
        # glow
        cr.set_line_width(3.0)
        cr.set_line_join(cairo.LINE_JOIN_ROUND)
        cr.set_line_cap(cairo.LINE_CAP_ROUND)
        cr.set_source_rgba(0.91, 0.63, 0.125, 0.25)
        for i in range(n):
            x, y = yx(i)
            cr.move_to(x, y) if i == 0 else cr.line_to(x, y)
        cr.stroke()
        # main line
        cr.set_line_width(1.5)
        cr.set_source_rgb(0.91, 0.63, 0.125)
        for i in range(n):
            x, y = yx(i)
            cr.move_to(x, y) if i == 0 else cr.line_to(x, y)
        cr.stroke()
        # label
        cr.set_source_rgb(0.0, 1.0, 0.4)
        cr.select_font_face("monospace", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_NORMAL)
        cr.set_font_size(9.0)
        cr.move_to(4, h - 4)
        cr.show_text(f"{self._ram_samples[-1]:.1f} MB")
        return False

    # ── boot sequence ─────────────────────────────────────────────────────

    _BOOT_QUOTES = [
        "▸ no bugs found. (we stopped looking.)",
        "▸ piper is ready. whether you are is your problem.",
        "▸ still compiling. always compiling. somewhere.",
        "▸ GTK3: because GTK4 is someone else's problem.",
        "▸ syntax highlighting is always on. unlike some editors.",
        "▸ this editor replaced Sublime. Sublime doesn't know yet.",
        "▸ 5 years in the making. 0 regrets. maybe 1.",
        "▸ warning: may cause productivity.",
        "▸ pyra_notes.conf: edit it. you'll know what to do.",
        "▸ undo stack: 300 deep. if you need more, call someone.",
        "▸ TTS voices: joe (male) lessac (female) you (optional).",
        "▸ welcome back. the code missed you. probably.",
        "▸ pyra is underneath. do not go there unless you must.",
        "▸ ctrl+z is in there. you're welcome. use it wisely.",
        "▸ linux only. other OSes: not our problem.",
        "▸ file saved. probably. hit Ctrl+S anyway.",
        "▸ cyon: built from scratch. held together by stubbornness.",
        "▸ the venv is fine. do not look at the venv.",
        "▸ python3-gi must be apt. pip tried. we don't talk about it.",
        "▸ ctrl+/ to comment. ctrl+d to duplicate. you're welcome.",
    ]

    _BOOT_STEPS = [
        ("initializing pyra_notes",         2),
        ("loading config",                  3),
        ("warming up GTK",                  4),
        ("checking for bugs",               5),
        ("none found (we stopped looking)", 6),
        ("mounting file tree",              7),
        ("arming syntax engine",            8),
        ("restoring session",               9),
        ("bribing the log terminal",        10),
    ]

    @staticmethod
    def _make_bar(fill, total=10):
        return "[" + "█" * fill + "░" * (total - fill) + "]"

    def _boot_sequence(self):
        self.log("▸ PYRA NOTES // booting...")
        self.log(" ")
        self._boot_step(0)
        return False

    def _boot_step(self, step):
        if step < len(self._BOOT_STEPS):
            label, fill = self._BOOT_STEPS[step]
            self.log(f"  {self._make_bar(fill)}  {label}")
            GLib.timeout_add(120, self._boot_step, step + 1)
        else:
            self.log(f"  {self._make_bar(10)}  done.")
            self.log(" ")
            import random
            self._log_amber(random.choice(self._BOOT_QUOTES))
            self.log(" ")
            self.log("▸ Ready.")
            self.log(" ")
            last = self._cfg.get("ui", "last_file", fallback=None)
            if last and os.path.isfile(last):
                try:
                    tab = self._tab()
                    tab.load_file(last)
                    self.filename_entry.set_text(tab.display_name)
                    self._update_tab_label(self._notebook.get_current_page(), tab)
                    self.log(f"▸ Restored: {last}")
                    GLib.idle_add(tab.text_view.grab_focus)
                except Exception as e:
                    self.log(f"▸ Could not restore last file: {e}")
        return False

    # ── destroy — persist session state ──────────────────────────────────

    def _on_destroy(self, _win):
        if self._tree_monitor:
            self._tree_monitor.cancel()
        if self._ram_timer:
            GLib.source_remove(self._ram_timer)
            self._ram_timer = None
        # Re-read the conf fresh so any manual edits the user made
        # (e.g. new highlight groups) are preserved — we only write ui keys on top.
        fresh = configparser.ConfigParser()
        fresh.read(CONFIG_PATH)
        if not fresh.has_section("ui"):
            fresh.add_section("ui")
        tab_file = self._tab().current_file if self._notebook.get_n_pages() > 0 else None
        if tab_file:
            fresh.set("ui", "last_file", tab_file)
        elif fresh.has_option("ui", "last_file"):
            fresh.remove_option("ui", "last_file")
        if self._tree_folder:
            fresh.set("ui", "tree_folder", self._tree_folder)
        save_config(fresh)
        Gtk.main_quit()

    # ── Ctrl+scroll wheel — text size ────────────────────────────────────

    def _on_scroll_zoom(self, widget, event):
        if event.state & Gdk.ModifierType.CONTROL_MASK:
            if event.direction == Gdk.ScrollDirection.UP:
                self.on_text_size_increase(None)
                return True
            if event.direction == Gdk.ScrollDirection.DOWN:
                self.on_text_size_decrease(None)
                return True
            # smooth-scroll devices send SMOOTH direction with delta_y
            if event.direction == Gdk.ScrollDirection.SMOOTH:
                _, _, dy = event.get_scroll_deltas()
                if dy < 0:
                    self.on_text_size_increase(None)
                elif dy > 0:
                    self.on_text_size_decrease(None)
                return True
        return False

    def _filter_executable(self, info, _data):
        """Accept files with no extension (likely bash scripts / executables)."""
        filename = info.get_filename() or ""
        return "." not in os.path.basename(filename)

    # ── helpers ──────────────────────────────────────────────────────────

    def _resolve_path(self, name):
        if not name:
            return None
        # Only auto-add .txt when there is no extension AND the file doesn't
        # already exist without one (covers extensionless bash/executables).
        _, ext = os.path.splitext(name)
        if not ext:
            candidate = name if os.path.isabs(name) else os.path.join(NOTES_DIR, name)
            if not os.path.exists(candidate):
                name += ".txt"   # brand-new name with no ext → plain text
        if os.path.isabs(name):
            return name
        return os.path.join(NOTES_DIR, name)

    # ── voice switch ─────────────────────────────────────────────────────

    def on_voice_switched(self, switch, _gparam):
        if switch.get_active():
            self.current_voice = "lessac"
            self.lbl_joe.get_style_context().remove_class("voice-label-active")
            self.lbl_joe.get_style_context().add_class("voice-label")
            self.lbl_lessac.get_style_context().remove_class("voice-label")
            self.lbl_lessac.get_style_context().add_class("voice-label-active")
        else:
            self.current_voice = "joe"
            self.lbl_lessac.get_style_context().remove_class("voice-label-active")
            self.lbl_lessac.get_style_context().add_class("voice-label")
            self.lbl_joe.get_style_context().remove_class("voice-label")
            self.lbl_joe.get_style_context().add_class("voice-label-active")
        self.log(f"▸ Voice: {VOICES[self.current_voice]['label']}")

    # ── TTS ──────────────────────────────────────────────────────────────

    def on_speak_all(self, _btn):
        start, end = self._tab().text_buffer.get_bounds()
        text = self._tab().text_buffer.get_text(start, end, True).strip()
        if not text:
            self.log("▸ TTS: no text in editor.")
            return
        self._dispatch_tts(text)

    def on_speak_selection(self, _btn):
        bounds = self._tab().text_buffer.get_selection_bounds()
        if not bounds:
            self.log("▸ TTS: no selection — highlight some text first.")
            return
        text = self._tab().text_buffer.get_text(bounds[0], bounds[1], True).strip()
        if text:
            self._dispatch_tts(text)

    def _dispatch_tts(self, text):
        voice_info = VOICES[self.current_voice]
        preview = text[:60] + ("…" if len(text) > 60 else "")
        self.log(f"▸ TTS [{voice_info['label']}]: {preview}")
        threading.Thread(
            target=self._run_piper, args=(text, self.current_voice), daemon=True
        ).start()

    def _run_piper(self, text, voice_key):
        voice_info = VOICES[voice_key]
        _piper_locations = [
            Path.home() / "pyra_env" / "bin" / "piper",
            Path.home() / "cyon" / "piper" / "piper",
            Path.home() / "cyon" / "piper_models" / "piper",
        ]
        PIPER_EXE  = next((str(p) for p in _piper_locations if p.is_file()), "piper")
        MODELS_DIR = Path.home() / "cyon" / "piper_models"
        OUTPUT     = str(MODELS_DIR / "voice.wav")
        try:
            piper_proc = subprocess.Popen(
                [
                    PIPER_EXE,
                    "--model",       str(MODELS_DIR / voice_info["model"]),
                    "--config",      str(MODELS_DIR / voice_info["config"]),
                    "--output_file", OUTPUT,
                ],
                stdin=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
            )
            self._tts_procs.append(piper_proc)
            piper_proc.communicate(input=text.encode("utf-8"))
            self._tts_procs = [p for p in self._tts_procs if p.poll() is None]
            if piper_proc.returncode != 0:
                return
            GLib.idle_add(self.log, f"▸ TTS saved: {OUTPUT}")
            aplay_proc = subprocess.Popen(
                ["aplay", OUTPUT], stderr=subprocess.DEVNULL
            )
            self._tts_procs.append(aplay_proc)
            aplay_proc.wait()
            self._tts_procs = [p for p in self._tts_procs if p.poll() is None]
            GLib.idle_add(self.log, f"▸ TTS playback done.")
        except Exception as e:
            GLib.idle_add(self.log, f"▸ Piper error: {e}")

    def on_tts_stop(self, _btn):
        if not self._tts_procs:
            self.log("▸ TTS: nothing running.")
            return
        for p in self._tts_procs:
            try:
                p.terminate()
            except Exception:
                pass
        self._tts_procs.clear()
        self.log("▸ TTS stopped.")

    # ── file ops ─────────────────────────────────────────────────────────

    def _on_new_tab_menu(self, _item):
        self._new_tab()
        self.log("▸ New tab.")
        self.filename_entry.grab_focus()

    def on_new(self, _btn):
        tab = self._tab()
        if not self._confirm_close(tab):
            return
        tab.current_file = None
        tab._undo_stack  = []
        tab._redo_stack  = []
        tab.text_buffer.set_text("")
        tab._label_text.set_text("untitled")
        self.filename_entry.set_text("")
        self.log("▸ New note — enter filename and hit SAVE.")
        self.filename_entry.grab_focus()

    def on_load(self, _btn):
        dialog = Gtk.FileChooserDialog(
            title="Open Note", parent=self,
            action=Gtk.FileChooserAction.OPEN,
        )
        dialog.add_buttons(
            Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
            Gtk.STOCK_OPEN,   Gtk.ResponseType.OK,
        )
        dialog.set_current_folder(NOTES_DIR)

        # ── filters ──────────────────────────────────────────────────────
        filt_all_code = Gtk.FileFilter()
        filt_all_code.set_name("All supported (txt, py, c, sh, *)")
        for pat in ("*.txt", "*.py", "*.c", "*.h", "*.sh"):
            filt_all_code.add_pattern(pat)
        filt_all_code.add_custom(Gtk.FileFilterFlags.FILENAME, self._filter_executable, None)
        dialog.add_filter(filt_all_code)

        for name, pat in [
            ("Text files (*.txt)", "*.txt"),
            ("Python files (*.py)", "*.py"),
            ("C/C++ files (*.c *.h)", "*.c"),
            ("Shell scripts (*.sh)", "*.sh"),
        ]:
            f = Gtk.FileFilter()
            f.set_name(name)
            f.add_pattern(pat)
            if "C/C++" in name:
                f.add_pattern("*.h")
            dialog.add_filter(f)

        filt_exec = Gtk.FileFilter()
        filt_exec.set_name("Executables / no extension")
        filt_exec.add_custom(Gtk.FileFilterFlags.FILENAME, self._filter_executable, None)
        dialog.add_filter(filt_exec)

        filt_any = Gtk.FileFilter()
        filt_any.set_name("All files (*)")
        filt_any.add_pattern("*")
        dialog.add_filter(filt_any)

        if dialog.run() == Gtk.ResponseType.OK:
            path = dialog.get_filename()
            try:
                tab = self._new_tab(path)
                self.log(f"▸ Loaded: {path}")
            except Exception as e:
                self.log(f"▸ ERROR loading: {e}")
        dialog.destroy()

    def on_save(self, _btn):
        tab = self._tab()
        if not tab.current_file:
            self.on_save_as(_btn)
            return
        start, end = tab.text_buffer.get_bounds()
        try:
            with open(tab.current_file, "w", encoding="utf-8") as f:
                f.write(tab.text_buffer.get_text(start, end, True))
            tab.mark_clean()
            self._update_tab_label(self._notebook.get_current_page(), tab)
            self.filename_entry.set_text(tab.display_name)
            self.log(f"▸ Saved: {tab.current_file}")
        except Exception as e:
            self.log(f"▸ ERROR saving: {e}")

    def on_save_as(self, _btn):
        dialog = Gtk.FileChooserDialog(
            title="Save As", parent=self,
            action=Gtk.FileChooserAction.SAVE,
        )
        dialog.add_buttons(
            Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
            Gtk.STOCK_SAVE,   Gtk.ResponseType.OK,
        )
        dialog.set_do_overwrite_confirmation(True)
        tab = self._tab()
        dialog.set_current_folder(
            os.path.dirname(tab.current_file) if tab.current_file else NOTES_DIR
        )
        current_name = self.filename_entry.get_text().strip()
        if current_name:
            dialog.set_current_name(current_name if "." in current_name else current_name + ".txt")

        if dialog.run() == Gtk.ResponseType.OK:
            path  = dialog.get_filename()
            start, end = tab.text_buffer.get_bounds()
            try:
                with open(path, "w", encoding="utf-8") as f:
                    f.write(tab.text_buffer.get_text(start, end, True))
                tab.current_file = path
                basename = os.path.basename(path)
                display  = basename[:-4] if basename.endswith(".txt") else basename
                self.filename_entry.set_text(display)
                self._update_tab_label(self._notebook.get_current_page(), tab)
                self.log(f"▸ Saved as: {path}")
            except Exception as e:
                self.log(f"▸ ERROR saving: {e}")
        dialog.destroy()

    def on_delete(self, _btn):
        name = self.filename_entry.get_text().strip()
        path = self._tab().current_file or self._resolve_path(name)
        if not path or not os.path.exists(path):
            self.log("▸ ERROR: no file to delete.")
            return
        dlg = Gtk.MessageDialog(
            transient_for=self, flags=0,
            message_type=Gtk.MessageType.WARNING,
            buttons=Gtk.ButtonsType.OK_CANCEL,
            text=f"Delete {os.path.basename(path)}?",
        )
        resp = dlg.run()
        dlg.destroy()
        if resp == Gtk.ResponseType.OK:
            try:
                fname = os.path.basename(path)
                os.remove(path)
                self.on_new(None)
                self.log(f"▸ Deleted: {fname}")
            except Exception as e:
                self.log(f"▸ ERROR deleting: {e}")


def main():
    ensure_notes_dir()
    win = PyraNotesWindow()
    Gtk.main()


if __name__ == "__main__":
    main()
