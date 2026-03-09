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
from pathlib import Path

# ── pyra_env path setup ──────────────────────────────────────────────────────
PYRA_ENV = Path.home() / "pyra_env"
PYRA_LIB = Path.home() / "cyon" / "pyra_tool"
site_pkgs = list(PYRA_ENV.glob("lib/python3*/site-packages"))
if site_pkgs:
    sys.path.insert(0, str(site_pkgs[0]))
sys.path.append(str(PYRA_LIB))

NOTES_DIR = os.path.expanduser("~/Documents/pyra_dev_notes")

# ── Syntax highlighting ──────────────────────────────────────────────────────
HIGHLIGHT_KEYWORDS        = ["print", "for", "while", "if", "elif", "else"]
HIGHLIGHT_COLOR           = "#E8A020"  # amber — control flow

HIGHLIGHT_KEYWORDS_CYAN   = ["def", "class", "return", "import", "from"]
HIGHLIGHT_COLOR_CYAN      = "#00cccc"  # cyan — structure

HIGHLIGHT_KEYWORDS_STEEL  = ["try", "except"]
HIGHLIGHT_COLOR_STEEL     = "#7ec8e3"  # steel blue — error handling + operators + decorators
# NOTE: = and @staticmethod are handled with custom regex in _apply_highlighting

HIGHLIGHT_COLOR_LIME      = "#c8ff00"  # lime — class name (word after 'class')
# NOTE: class names use a capture-group regex, no keyword list needed

HIGHLIGHT_COLOR_CORAL     = "#ff9955"  # coral — reserved for future keywords

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
"""


def ensure_notes_dir():
    os.makedirs(NOTES_DIR, exist_ok=True)


class PyraNotesWindow(Gtk.Window):
    def __init__(self):
        super().__init__(title="▸ PYRA NOTES // TTS")
        self.set_default_size(800, 640)
        self.set_border_width(10)

        self.current_file  = None
        self.current_voice = "joe"
        self.text_size     = 15  # default px, matches CSS

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
            ("NEW",          self.on_new),
            ("LOAD",         self.on_load),
            ("SAVE",         self.on_save),
            (None,           None),
            ("DELETE FILE",  self.on_delete),
            (None,           None),
            ("TEXT  +",      self.on_text_size_increase),
            ("TEXT  −",      self.on_text_size_decrease),
        ]:
            if label is None:
                self._file_menu.append(Gtk.SeparatorMenuItem())
            else:
                item = Gtk.MenuItem(label=label)
                item.connect("activate", cb)
                self._file_menu.append(item)
        self._file_menu.show_all()

        menu_btn = Gtk.Button(label="FILE")
        menu_btn.get_style_context().add_class("btn-menu")
        menu_btn.connect("clicked", self._on_file_menu_clicked)
        file_bar.pack_start(menu_btn, False, False, 0)

        # ── Text editor ──────────────────────────────────────────────────
        scroll = Gtk.ScrolledWindow()
        scroll.set_min_content_height(340)
        self.text_view = Gtk.TextView()
        self.text_view.set_name("editor-view")
        self.text_view.set_wrap_mode(Gtk.WrapMode.WORD_CHAR)
        self.text_buffer = self.text_view.get_buffer()

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
        self.text_buffer.connect("changed", self._on_text_changed)
        scroll.add(self.text_view)
        outer.pack_start(scroll, True, True, 0)

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

    def _apply_highlighting(self, buf):
        import re
        start, end = buf.get_bounds()
        buf.remove_tag(self._kw_tag,       start, end)
        buf.remove_tag(self._kw_tag_cyan,  start, end)
        buf.remove_tag(self._kw_tag_steel, start, end)
        buf.remove_tag(self._kw_tag_lime,  start, end)
        buf.remove_tag(self._kw_tag_coral, start, end)
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

    # ── file menu ─────────────────────────────────────────────────────────

    def _on_file_menu_clicked(self, btn):
        self._file_menu.popup_at_widget(
            btn, Gdk.Gravity.SOUTH_WEST, Gdk.Gravity.NORTH_WEST, None
        )

    # ── text size ─────────────────────────────────────────────────────────

    def _apply_text_size(self):
        """Push the current text_size into the editor textview only."""
        css = f"#editor-view, #editor-view text {{ font-size: {self.text_size}px; }}".encode()
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
        name = self.filename_entry.get_text().strip()
        if not name:
            self.log("▸ ERROR: enter a filename first.")
            return
        path = self._resolve_path(name)
        start, end = self.text_buffer.get_bounds()
        try:
            with open(path, "w", encoding="utf-8") as f:
                f.write(self.text_buffer.get_text(start, end, True))
            self.current_file = path
            self.log(f"▸ Saved: {path}")
        except Exception as e:
            self.log(f"▸ ERROR saving: {e}")

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
