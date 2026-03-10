#!/usr/bin/env python3
# pyra_notes.py — CYON notes editor + Piper TTS
# Author: Ioannes Cruxibulum

import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, Gdk, GLib

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

# ── Syntax highlighting ─────────────────────────────────────────

HIGHLIGHT_KEYWORDS        = ["print", "for", "while", "if", "elif", "else", "with", "finally"]
HIGHLIGHT_COLOR           = "#ffb000"   # hacker amber — control flow

HIGHLIGHT_KEYWORDS_CYAN   = ["def", "class", "return", "import", "from"]
HIGHLIGHT_COLOR_CYAN      = "#5fd7ff"   # softer cyan — structure

HIGHLIGHT_KEYWORDS_STEEL  = ["try", "except", "self"]
HIGHLIGHT_COLOR_STEEL     = "#8fd3ff"   # pale blue — operators / error handling

HIGHLIGHT_COLOR_LIME      = "#c8ff00"   # neon lime — class names

HIGHLIGHT_COLOR_CORAL     = "#ff9966"   # warm orange — spare keyword group

HIGHLIGHT_COLOR_COMMENT   = "#6a5acd"   # muted violet — comments

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
entry:focus { border-color: #00ff99; }
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
    border-color: #00ff99;
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
    border-color: #00ff99;
}
scrolledwindow { border: 1px solid #1a2a20; }
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
    border-color: #00ff99;
}
"""


def ensure_notes_dir():
    os.makedirs(NOTES_DIR, exist_ok=True)


def load_config():
    cfg = configparser.ConfigParser()
    cfg.read(CONFIG_PATH)
    return cfg


def save_config(cfg):
    with open(CONFIG_PATH, "w") as f:
        cfg.write(f)


class PyraNotesWindow(Gtk.Window):
    def __init__(self):
        super().__init__(title="▸ PYRA NOTES // TTS")
        self.set_default_size(800, 640)
        self.set_border_width(10)

        self.current_file  = None
        self.current_voice = "joe"
        self.text_size     = 15  # default px, matches CSS

        # ── load config ──────────────────────────────────────────────────
        self._cfg = load_config()
        self._tree_folder = self._cfg.get("ui", "tree_folder", fallback=NOTES_DIR)

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
            ("NEW", self.on_new),
            ("LOAD", self.on_load),
            ("SAVE", self.on_save),
            ("SAVE AS", self.on_save_as),
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

        # ── Text editor + line numbers ────────────────────────────────────
        # Outer scroll contains a horizontal box: [line_nums | editor]
        # Both share the same vertical adjustment so they scroll in sync.
        scroll = Gtk.ScrolledWindow()
        scroll.set_min_content_height(340)
        scroll.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)

        editor_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)

        # Line number view (read-only, non-interactive)
        self._line_buf  = Gtk.TextBuffer()
        self._line_view = Gtk.TextView(buffer=self._line_buf)
        self._line_view.set_name("line-numbers")
        self._line_view.set_editable(False)
        self._line_view.set_cursor_visible(False)
        self._line_view.set_wrap_mode(Gtk.WrapMode.NONE)
        self._line_view.set_left_margin(6)
        self._line_view.set_right_margin(6)
        self._line_view.set_can_focus(False)
        editor_box.pack_start(self._line_view, False, False, 0)

        self.text_view = Gtk.TextView()
        self.text_view.set_name("editor-view")
        self.text_view.set_wrap_mode(Gtk.WrapMode.WORD_CHAR)
        self.text_view.set_left_margin(6)
        self.text_buffer = self.text_view.get_buffer()
        editor_box.pack_start(self.text_view, True, True, 0)

        scroll.add(editor_box)

        # ── Syntax highlight tags ─────────────────────────────────────────
        self._kw_tag = self.text_buffer.create_tag(
            "keyword", foreground=HIGHLIGHT_COLOR
        )
        self._kw_tag_cyan = self.text_buffer.create_tag(
            "keyword_cyan", foreground=HIGHLIGHT_COLOR_CYAN
        )
        self._kw_tag_steel = self.text_buffer.create_tag(
            "keyword_steel", foreground=HIGHLIGHT_COLOR_STEEL
        )
        self._kw_tag_lime = self.text_buffer.create_tag(
            "keyword_lime", foreground=HIGHLIGHT_COLOR_LIME
        )
        self._kw_tag_coral = self.text_buffer.create_tag(
            "keyword_coral", foreground=HIGHLIGHT_COLOR_CORAL
        )
        self._kw_tag_comment = self.text_buffer.create_tag(
            "comment", foreground=HIGHLIGHT_COLOR_COMMENT
        )
        self.text_buffer.connect("changed", self._on_text_changed)
        self.text_view.connect("key-press-event", self._on_key_press)

        # Keep line numbers scrolled in sync with the editor
        scroll.get_vadjustment().connect(
            "value-changed", lambda adj: self._sync_line_scroll(adj)
        )
        self._update_line_numbers()

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
        paned.pack1(scroll, True, True)
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

        # ── Log terminal ─────────────────────────────────────────────────
        log_scroll = Gtk.ScrolledWindow()
        log_scroll.set_min_content_height(72)
        self.log_view = Gtk.TextView()
        self.log_view.set_editable(False)
        self.log_view.set_cursor_visible(False)
        self.log_view.set_wrap_mode(Gtk.WrapMode.WORD_CHAR)
        self.log_buf = self.log_view.get_buffer()
        log_scroll.add(self.log_view)
        outer.pack_start(log_scroll, False, False, 0)

        # ── Status bar ───────────────────────────────────────────────────
        self.status = Gtk.Label(label=f"dir: {NOTES_DIR}")
        self.status.get_style_context().add_class("status-bar")
        self.status.set_halign(Gtk.Align.START)
        outer.pack_start(self.status, False, False, 0)

        self.connect("destroy", Gtk.main_quit)
        self.show_all()

        self.log(f"▸ Ready. Notes dir: {NOTES_DIR}")

    # ── syntax highlighting ───────────────────────────────────────────────

    def _on_text_changed(self, buf):
        self._apply_highlighting(buf)
        self._update_line_numbers()

    def _apply_highlighting(self, buf):
        import re
        start, end = buf.get_bounds()
        buf.remove_tag(self._kw_tag,       start, end)
        buf.remove_tag(self._kw_tag_cyan,  start, end)
        buf.remove_tag(self._kw_tag_steel, start, end)
        buf.remove_tag(self._kw_tag_lime,  start, end)
        buf.remove_tag(self._kw_tag_coral, start, end)
        buf.remove_tag(self._kw_tag_comment, start, end)
        text = buf.get_text(start, end, True)

        # amber — control flow keywords
        for kw in HIGHLIGHT_KEYWORDS:
            for m in re.finditer(rf"\b{re.escape(kw)}\b", text):
                buf.apply_tag(self._kw_tag,
                    buf.get_iter_at_offset(m.start()),
                    buf.get_iter_at_offset(m.end()))

        # cyan — structure keywords
        for kw in HIGHLIGHT_KEYWORDS_CYAN:
            for m in re.finditer(rf"\b{re.escape(kw)}\b", text):
                buf.apply_tag(self._kw_tag_cyan,
                    buf.get_iter_at_offset(m.start()),
                    buf.get_iter_at_offset(m.end()))

        # steel blue — try/except keywords
        for kw in HIGHLIGHT_KEYWORDS_STEEL:
            for m in re.finditer(rf"\b{re.escape(kw)}\b", text):
                buf.apply_tag(self._kw_tag_steel,
                    buf.get_iter_at_offset(m.start()),
                    buf.get_iter_at_offset(m.end()))

        # steel blue — = operator (standalone, not == or !=  or <=  or >=)
        for m in re.finditer(r"(?<![=!<>])=(?!=)", text):
            buf.apply_tag(self._kw_tag_steel,
                buf.get_iter_at_offset(m.start()),
                buf.get_iter_at_offset(m.end()))

        # steel blue — @staticmethod decorator (includes the @)
        for m in re.finditer(r"@staticmethod\b", text):
            buf.apply_tag(self._kw_tag_steel,
                buf.get_iter_at_offset(m.start()),
                buf.get_iter_at_offset(m.end()))

        # lime — class name: the identifier immediately after 'class'
        for m in re.finditer(r"\bclass\s+(\w+)", text):
            buf.apply_tag(self._kw_tag_lime,
                buf.get_iter_at_offset(m.start(1)),
                buf.get_iter_at_offset(m.end(1)))
        
        for m in re.finditer(r'([\'"])(.*?)(\1)', text):  # match opening quote, content, closing quote
            content = m.group(2)
            start_offset = m.start(2)
            for w in re.finditer(r'\S+', content):  # highlight only non-space sequences
                buf.apply_tag(self._kw_tag_coral,
                    buf.get_iter_at_offset(start_offset + w.start()),
                    buf.get_iter_at_offset(start_offset + w.end()))

        # bright purple — # comments (Python/bash) and // comments (C)
        # matches from # or // to end of line; painted last to override everything
        for m in re.finditer(r'(#[^\n]*|//[^\n]*)', text):
            buf.apply_tag(self._kw_tag_comment,
                buf.get_iter_at_offset(m.start()),
                buf.get_iter_at_offset(m.end()))

    # ── line numbers ──────────────────────────────────────────────────────

    def _update_line_numbers(self, *_):
        count = self.text_buffer.get_line_count()
        # right-align numbers to width of highest line number
        width  = len(str(count))
        nums   = "\n".join(str(i).rjust(width) for i in range(1, count + 1))
        self._line_buf.set_text(nums)

    def _sync_line_scroll(self, *_):
        """Mirror the editor's vertical scroll position onto the line view."""
        adj = self.text_view.get_parent().get_parent() if False else None
        # Use the text_view's own vadjustment via its scrolled window
        sw = self.text_view.get_parent()  # editor_box
        if sw:
            sw2 = sw.get_parent()  # ScrolledWindow
            if sw2 and isinstance(sw2, Gtk.ScrolledWindow):
                val = sw2.get_vadjustment().get_value()
                ladj = self._line_view.get_vadjustment()
                if ladj:
                    ladj.set_value(val)

    # ── file tree ─────────────────────────────────────────────────────────

    def _populate_tree(self, folder):
        """Build the TreeStore from the given folder path."""
        self._tree_store.clear()
        if not os.path.isdir(folder):
            return
        self._tree_folder = folder
        # persist to config
        if not self._cfg.has_section("ui"):
            self._cfg.add_section("ui")
        self._cfg.set("ui", "tree_folder", folder)
        save_config(self._cfg)

        root_label = os.path.basename(folder) or folder
        root_iter  = self._tree_store.append(None, [f"📁 {root_label}", folder])
        self._add_tree_children(root_iter, folder)
        self._tree_view.expand_row(
            self._tree_store.get_path(root_iter), False
        )

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
                with open(fpath, "r", encoding="utf-8") as f:
                    self.text_buffer.set_text(f.read())
                self._apply_highlighting(self.text_buffer)
                self._update_line_numbers()
                self.current_file = fpath
                basename = os.path.basename(fpath)
                display  = basename[:-4] if basename.endswith(".txt") else basename
                self.filename_entry.set_text(display)
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
            self.text_buffer.insert_at_cursor("    ")
            return True

        # ── Enter — smart indent ──────────────────────────────────────────
        if event.keyval in (Gdk.KEY_Return, Gdk.KEY_KP_Enter):
            buf    = self.text_buffer
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

    # ── log / status ─────────────────────────────────────────────────────

    def log(self, msg):
        """Append a line to the log terminal and also update the status bar."""
        end = self.log_buf.get_end_iter()
        self.log_buf.insert(end, msg + "\n")
        self.log_view.scroll_to_iter(self.log_buf.get_end_iter(), 0, False, 0, 0)
        self.status.set_text(msg)

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
        start, end = self.text_buffer.get_bounds()
        text = self.text_buffer.get_text(start, end, True).strip()
        if not text:
            self.log("▸ TTS: no text in editor.")
            return
        self._dispatch_tts(text)

    def on_speak_selection(self, _btn):
        bounds = self.text_buffer.get_selection_bounds()
        if not bounds:
            self.log("▸ TTS: no selection — highlight some text first.")
            return
        text = self.text_buffer.get_text(bounds[0], bounds[1], True).strip()
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
            subprocess.run(
                [
                    PIPER_EXE,
                    "--model",       str(MODELS_DIR / voice_info["model"]),
                    "--config",      str(MODELS_DIR / voice_info["config"]),
                    "--output_file", OUTPUT,
                ],
                input=text.encode("utf-8"),
                check=True,
                stderr=subprocess.DEVNULL,
            )
            GLib.idle_add(self.log, f"▸ TTS saved: {OUTPUT}")
            subprocess.run(["aplay", OUTPUT], check=True, stderr=subprocess.DEVNULL)
            GLib.idle_add(self.log, f"▸ TTS playback done.")
        except Exception as e:
            GLib.idle_add(self.log, f"▸ Piper error: {e}")

    # ── file ops ─────────────────────────────────────────────────────────

    def on_new(self, _btn):
        self.current_file = None
        self.filename_entry.set_text("")
        self.text_buffer.set_text("")
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
                with open(path, "r", encoding="utf-8") as f:
                    self.text_buffer.set_text(f.read())
                self._apply_highlighting(self.text_buffer)
                self._update_line_numbers()
                self.current_file = path
                basename = os.path.basename(path)
                # Strip .txt for display; keep .py/.c/.sh/etc. visible
                display = basename[:-4] if basename.endswith(".txt") else basename
                self.filename_entry.set_text(display)
                self.log(f"▸ Loaded: {path}")
            except Exception as e:
                self.log(f"▸ ERROR loading: {e}")
        dialog.destroy()

    def on_save(self, _btn):
        # No known file path — open Save As dialog instead
        if not self.current_file:
            self.on_save_as(_btn)
            return
        start, end = self.text_buffer.get_bounds()
        try:
            with open(self.current_file, "w", encoding="utf-8") as f:
                f.write(self.text_buffer.get_text(start, end, True))
            self.log(f"▸ Saved: {self.current_file}")
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
        dialog.set_current_folder(
            os.path.dirname(self.current_file) if self.current_file else NOTES_DIR
        )
        # pre-fill current filename if we have one
        current_name = self.filename_entry.get_text().strip()
        if current_name:
            dialog.set_current_name(current_name if "." in current_name else current_name + ".txt")

        if dialog.run() == Gtk.ResponseType.OK:
            path  = dialog.get_filename()
            start, end = self.text_buffer.get_bounds()
            try:
                with open(path, "w", encoding="utf-8") as f:
                    f.write(self.text_buffer.get_text(start, end, True))
                self.current_file = path
                basename = os.path.basename(path)
                display  = basename[:-4] if basename.endswith(".txt") else basename
                self.filename_entry.set_text(display)
                self.log(f"▸ Saved as: {path}")
            except Exception as e:
                self.log(f"▸ ERROR saving: {e}")
        dialog.destroy()

    def on_delete(self, _btn):
        name = self.filename_entry.get_text().strip()
        path = self.current_file or self._resolve_path(name)
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
