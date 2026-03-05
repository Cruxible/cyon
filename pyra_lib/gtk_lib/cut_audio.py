#!/usr/bin/env python3
# Author: Ioannes Cruxibulum
# cut_audio.py — part of pyra_lib
import os
import sys
import threading
import random
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
from pyra_shared import Input, main_logo, HonerableMentions
from moviepy import AudioFileClip
# ---------------- CYON PS1 STYLE ---------------- #
CSS = b"""
window, .background {
    background-color: #0a0a0f;
}
label {
    color: #00cc77;
    font-family: monospace;
    font-size: 11px;
    letter-spacing: 1px;
}
.title-label {
    color: #00ff99;
    font-family: monospace;
    font-size: 14px;
    font-weight: bold;
    letter-spacing: 4px;
    text-shadow: 0 0 3px #00ff99;
}
.flicker {
    text-shadow: 0 0 6px #00ff99;
}
.status-ok   { color: #00ff99; }
.status-err  { color: #ff3355; }
button {
    background-color: #0d0d15;
    color: #00cc77;
    font-family: monospace;
    font-size: 12px;
    border: 1px solid #1a2a20;
    border-radius: 0px;
    padding: 6px 18px;
}
button:hover {
    background-color: #003322;
    color: #00ff99;
    border-color: #00ff99;
    box-shadow: 0 0 4px #00ff99;
}
button:disabled {
    background-color: #0a0a0f;
    color: #1a2a20;
    border-color: #1a2a20;
}
/* --- PROPER PS1 SLIDER STYLING --- */
scale trough {
    background-color: #0d0d15;
    border: 1px solid #1a2a20;
    min-height: 4px;
}
scale highlight {
    background-color: #00ff99;
}
scale slider {
    background-color: #00cc77;
    border: 1px solid #00ff99;
    border-radius: 0px;
    min-width: 12px;
    min-height: 12px;
}
separator {
    background-color: #1a2a20;
}
"""
# ---------------- LOGIC ---------------- #
class AudioCutter:
    @staticmethod
    def get_duration(filepath):
        clip = AudioFileClip(filepath)
        duration = clip.duration
        clip.close()
        return duration

    @staticmethod
    def cut(input_path, output_path, start_time, end_time):
        clip = AudioFileClip(input_path)
        try:
            segment = clip.subclipped(start_time, end_time)
            segment.write_audiofile(output_path)
        finally:
            clip.close()

    @staticmethod
    def get_music_dir():
        return str(Path.home() / "Music")


class FileChooser_Cut_Audio(Gtk.ApplicationWindow):
    def __init__(self, app, treeview_window=None):
        super().__init__(application=app, title="CYON // AUDIO CUTTER")
        self.set_default_size(480, 360)
        self.set_resizable(False)
        self.treeview_window = treeview_window
        self.dialog = None
        # Apply CSS
        provider = Gtk.CssProvider()
        provider.load_from_data(CSS)
        Gtk.StyleContext.add_provider_for_screen(
            self.get_screen(), provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )
        self.connect("delete-event", self.on_delete_event)
        # Outer layout
        outer = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        outer.set_margin_top(16)
        outer.set_margin_bottom(12)
        outer.set_margin_start(16)
        outer.set_margin_end(16)
        self.add(outer)
        # Title
        self.title = Gtk.Label(label="▸ AUDIO CUTTER // TRIM")
        self.title.get_style_context().add_class("title-label")
        self.title.set_halign(Gtk.Align.START)
        outer.pack_start(self.title, False, False, 0)
        sep = Gtk.Separator()
        outer.pack_start(sep, False, False, 10)
        self.button1 = Gtk.Button(label="▸ CHOOSE FILE")
        self.button1.connect("clicked", self.on_file_clicked)
        outer.pack_start(self.button1, False, False, 4)
        # Start time
        start_lbl = Gtk.Label(label="START TIME")
        start_lbl.set_halign(Gtk.Align.START)
        outer.pack_start(start_lbl, False, False, 6)
        self.start_time_scale = Gtk.Scale.new_with_range(
            Gtk.Orientation.HORIZONTAL, 0, 10, 1
        )
        outer.pack_start(self.start_time_scale, False, False, 0)
        # End time
        end_lbl = Gtk.Label(label="END TIME")
        end_lbl.set_halign(Gtk.Align.START)
        outer.pack_start(end_lbl, False, False, 6)
        self.end_time_scale = Gtk.Scale.new_with_range(
            Gtk.Orientation.HORIZONTAL, 0, 10, 1
        )
        outer.pack_start(self.end_time_scale, False, False, 0)
        self.button2 = Gtk.Button(label="▸ SELECT A FILE FIRST")
        self.button2.set_sensitive(False)
        self.button2.connect("clicked", self.on_button2_clicked)
        outer.pack_start(self.button2, False, False, 8)
        sep2 = Gtk.Separator()
        outer.pack_start(sep2, False, False, 8)
        self.status_label = Gtk.Label(label="▸ Select an audio file to begin.")
        self.status_label.set_xalign(0)
        self.status_label.set_line_wrap(True)
        outer.pack_start(self.status_label, False, False, 0)
        self.start_flicker()
        self.show_all()

    # -------- PS1 Flicker -------- #
    def start_flicker(self):
        def toggle():
            ctx = self.title.get_style_context()
            if ctx.has_class("flicker"):
                ctx.remove_class("flicker")
            else:
                ctx.add_class("flicker")
            GLib.timeout_add(random.randint(380, 520), toggle)
            return False
        GLib.timeout_add(400, toggle)

    # -------- Events -------- #
    def on_delete_event(self, widget, event):
        self.destroy()
        if self.treeview_window is not None:
            self.treeview_window.show_all()
        return False

    def on_file_clicked(self, widget):
        self.dialog = Gtk.FileChooserDialog(
            title="Choose Audio File", parent=self, action=Gtk.FileChooserAction.OPEN
        )
        self.dialog.set_current_folder(AudioCutter.get_music_dir())
        self.dialog.add_buttons(
            Gtk.STOCK_CANCEL,
            Gtk.ResponseType.CANCEL,
            Gtk.STOCK_OPEN,
            Gtk.ResponseType.OK,
        )
        filter_mp3 = Gtk.FileFilter()
        filter_mp3.set_name("MP3 files")
        filter_mp3.add_mime_type("audio/mpeg")
        self.dialog.add_filter(filter_mp3)
        response = self.dialog.run()
        if response == Gtk.ResponseType.OK:
            duration = AudioCutter.get_duration(self.dialog.get_filename())
            self.start_time_scale.set_range(0, duration)
            self.end_time_scale.set_range(0, duration)
            self.button2.set_sensitive(True)
            self.button2.set_label("■ CUT AUDIO")
            self.dialog.hide()
        else:
            self.dialog.destroy()

    def on_button2_clicked(self, widget):
        try:
            start_time = int(self.start_time_scale.get_value())
            end_time = int(self.end_time_scale.get_value())

            # Validate range before even opening save dialog
            if start_time >= end_time:
                self.status_label.set_text("▸ Error: Start time must be before end time.")
                self.status_label.get_style_context().add_class("status-err")
                return

            filename = self.dialog.get_filename()
            if not filename:
                self.status_label.set_text("▸ Select a file first.")
                return

            save_dialog = Gtk.FileChooserDialog(
                title="Save Output",
                parent=self,
                action=Gtk.FileChooserAction.SAVE,
            )
            save_dialog.set_current_folder(AudioCutter.get_music_dir())
            save_dialog.add_buttons(
                Gtk.STOCK_CANCEL,
                Gtk.ResponseType.CANCEL,
                Gtk.STOCK_SAVE,
                Gtk.ResponseType.OK,
            )
            response = save_dialog.run()
            if response == Gtk.ResponseType.OK:
                output_filename = save_dialog.get_filename()
                save_dialog.destroy()

                if not output_filename.endswith(".mp3"):
                    output_filename += ".mp3"

                self.status_label.get_style_context().remove_class("status-err")
                self.status_label.set_text("▸ Cutting audio...")
                self.button2.set_sensitive(False)
                self.button1.set_sensitive(False)

                def cut_audio_func():
                    try:
                        AudioCutter.cut(filename, output_filename, start_time, end_time)
                        GLib.idle_add(self._on_cut_success, output_filename)
                    except Exception as e:
                        GLib.idle_add(self._on_cut_error, str(e))

                threading.Thread(target=cut_audio_func, daemon=True).start()
            else:
                save_dialog.destroy()

        except ValueError as e:
            self.status_label.set_text(f"▸ Error: {e}")

    def _on_cut_success(self, output_filename):
        self.status_label.get_style_context().remove_class("status-err")
        self.status_label.get_style_context().add_class("status-ok")
        self.status_label.set_text(f"▸ Done: {output_filename}")
        self.button2.set_sensitive(True)
        self.button1.set_sensitive(True)

    def _on_cut_error(self, error_msg):
        self.status_label.get_style_context().add_class("status-err")
        self.status_label.set_text(f"▸ Error: {error_msg}")
        self.button2.set_sensitive(True)
        self.button1.set_sensitive(True)


class CutAudioApp(Gtk.Application):
    def __init__(self, treeview_window=None):
        super().__init__(application_id="com.pyra.cutaudio")
        self.treeview_window = treeview_window
        self.connect("activate", self.on_activate)

    def on_activate(self, app):
        FileChooser_Cut_Audio(app, treeview_window=self.treeview_window)


if __name__ == "__main__":
    app = CutAudioApp()
    app.run()
