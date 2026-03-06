#!/usr/bin/env python3
# Author: Ioannes Cruxibulum
# pyra_tts.py — part of pyra_lib
import os
import sys
import subprocess
import threading
from pathlib import Path
import gi
gi.require_version("Gtk", "3.0")
gi.require_version("Gdk", "3.0")
from gi.repository import Gtk, Gdk, GLib
PYRA_ENV = Path.home() / "pyra_env"
PYRA_LIB = Path.home() / "cyon" / "pyra_lib"
site_pkgs = list(PYRA_ENV.glob("lib/python3*/site-packages"))
if site_pkgs:
    sys.path.insert(0, str(site_pkgs[0]))
sys.path.append(str(PYRA_LIB))
CSS = b"""
window, .background {
    background-color: #0a0a0f;
}
label {
    color: #00cc77;
    font-family: monospace;
    font-size: 11px;
}
.title-label {
    color: #00ff99;
    font-family: monospace;
    font-size: 14px;
    font-weight: bold;
    letter-spacing: 3px;
}
.status-label {
    color: #336655;
    font-family: monospace;
    font-size: 10px;
}
.voice-label {
    color: #00cc77;
    font-family: monospace;
    font-size: 11px;
    min-width: 80px;
}
.voice-label-active {
    color: #00ff99;
    font-family: monospace;
    font-size: 11px;
    font-weight: bold;
    min-width: 80px;
}
status-ok   { color: #00ff99; }
status-err  { color: #ff3355; }
entry {
    background-color: #0d0d15;
    color: #00ff99;
    font-family: monospace;
    font-size: 12px;
    border: 1px solid #1a2a20;
    border-radius: 0px;
    padding: 4px;
}
entry:focus {
    border-color: #00ff99;
}
button {
    background-color: #0d0d15;
    color: #00cc77;
    font-family: monospace;
    font-size: 12px;
    border: 1px solid #1a2a20;
    border-radius: 0px;
    padding: 6px 14px;
}
button:hover {
    background-color: #003322;
    color: #00ff99;
    border-color: #00ff99;
}
scrolledwindow {
    border: 1px solid #1a2a20;
}
textview, textview text {
    background-color: #05050a;
    color: #00cc77;
    font-family: monospace;
    font-size: 11px;
}
separator {
    background-color: #1a2a20;
}
"""

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

class PiperTTSWindow(Gtk.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app, title="CYON // TTS")
        self.set_default_size(500, 320)
        self.set_border_width(12)
        self.current_voice = "joe"  # default

        # Apply CSS
        provider = Gtk.CssProvider()
        provider.load_from_data(CSS)
        Gtk.StyleContext.add_provider_for_screen(
            self.get_screen(), provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )

        outer = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        self.add(outer)

        # Title
        title = Gtk.Label(label="▸ TTS // Piper Voice Generator")
        title.get_style_context().add_class("title-label")
        title.set_halign(Gtk.Align.START)
        outer.pack_start(title, False, False, 0)

        # Voice toggle row
        voice_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        voice_row.set_halign(Gtk.Align.CENTER)

        voice_head = Gtk.Label(label="VOICE ▸")
        voice_head.get_style_context().add_class("status-label")
        voice_row.pack_start(voice_head, False, False, 4)

        self.lbl_joe = Gtk.Label(label="JOE  ♂")
        self.lbl_joe.get_style_context().add_class("voice-label-active")
        self.lbl_joe.set_halign(Gtk.Align.END)
        voice_row.pack_start(self.lbl_joe, False, False, 0)

        self.toggle_switch = Gtk.Switch()
        self.toggle_switch.set_active(False)   # False = joe, True = lessac
        self.toggle_switch.connect("notify::active", self.on_voice_switched)
        voice_row.pack_start(self.toggle_switch, False, False, 4)

        self.lbl_lessac = Gtk.Label(label="LESSAC ♀")
        self.lbl_lessac.get_style_context().add_class("voice-label")
        self.lbl_lessac.set_halign(Gtk.Align.START)
        voice_row.pack_start(self.lbl_lessac, False, False, 0)

        outer.pack_start(voice_row, False, False, 0)

        # Input field
        self.text_entry = Gtk.Entry()
        self.text_entry.set_placeholder_text("Type text to speak...")
        outer.pack_start(self.text_entry, False, False, 0)

        # Generate button
        btn_generate = Gtk.Button(label="■ GENERATE VOICE")
        btn_generate.connect("clicked", self.on_generate_clicked)
        outer.pack_start(btn_generate, False, False, 0)

        # Log / output area
        scroll = Gtk.ScrolledWindow()
        scroll.set_min_content_height(120)
        self.log_view = Gtk.TextView()
        self.log_view.set_editable(False)
        self.log_view.set_cursor_visible(False)
        self.log_view.set_wrap_mode(Gtk.WrapMode.WORD_CHAR)
        self.log_buffer = self.log_view.get_buffer()
        scroll.add(self.log_view)
        outer.pack_start(scroll, True, True, 0)

        self.show_all()
        self.log("▸ Ready. Enter text and press GENERATE.")
        self.log(f"▸ Active voice: {VOICES[self.current_voice]['label']}")

    def on_voice_switched(self, switch, gparam):
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
        self.log(f"▸ Voice switched to: {VOICES[self.current_voice]['label']}")

    def log(self, msg):
        end = self.log_buffer.get_end_iter()
        self.log_buffer.insert(end, msg + "\n")
        self.log_view.scroll_to_iter(self.log_buffer.get_end_iter(), 0, False, 0, 0)

    def on_generate_clicked(self, widget):
        text = self.text_entry.get_text().strip()
        if not text:
            self.log("▸ Please enter some text.")
            return
        voice_info = VOICES[self.current_voice]
        self.log(f"▸ [{voice_info['label']}] Generating: {text}")
        threading.Thread(target=self.run_piper, args=(text, self.current_voice), daemon=True).start()

    def run_piper(self, text, voice_key):
        """Run Piper TTS to generate voice.wav"""
        voice_info = VOICES[voice_key]
        PIPER_EXE    = str(Path.home() / "cyon" / "piper" / "piper")
        MODELS_DIR   = Path.home() / "cyon/piper_models"
        PIPER_MODEL  = str(MODELS_DIR / voice_info["model"])
        PIPER_CONFIG = str(MODELS_DIR / voice_info["config"])
        VOICE_OUTPUT = str(MODELS_DIR / "voice.wav")
        try:
            subprocess.run(
                [
                    PIPER_EXE,
                    "--model",  PIPER_MODEL,
                    "--config", PIPER_CONFIG,
                    "--output_file", VOICE_OUTPUT,
                ],
                input=text.encode("utf-8"),
                check=True,
                stderr=subprocess.DEVNULL,
            )
            GLib.idle_add(self.log, f"▸ Voice saved: {VOICE_OUTPUT}")
            subprocess.run(
                ["aplay", VOICE_OUTPUT], check=True, stderr=subprocess.DEVNULL
            )
        except Exception as e:
            GLib.idle_add(self.log, f"▸ Piper Error: {e}")


class PiperApp(Gtk.Application):
    def __init__(self):
        super().__init__(application_id="com.pyra.tts")
        self.connect("activate", self.on_activate)

    def on_activate(self, app):
        PiperTTSWindow(app)


if __name__ == "__main__":
    app = PiperApp()
    app.run()
