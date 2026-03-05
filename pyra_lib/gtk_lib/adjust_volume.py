#!/usr/bin/env python3
# Author: Ioannes Cruxibulum
# adjust_volume.py — part of pyra_lib

import os
import sys
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
from pyra_shared import Input, main_logo, HonerableMentions

from moviepy import AudioFileClip
from moviepy.audio.fx import MultiplyVolume

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
.status-ok  { color: #00ff99; }
.status-err { color: #ff3355; }
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
}
button:disabled {
    background-color: #0a0a0f;
    color: #1a2a20;
    border-color: #1a2a20;
}
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


class VolumeAdjuster:
    @staticmethod
    def get_music_dir():
        return str(Path.home() / "Music")

    @staticmethod
    def adjust(input_path, output_path, volume_level):
        audio = AudioFileClip(input_path)
        audio = audio.with_effects([MultiplyVolume(volume_level)])
        audio.write_audiofile(output_path)


class FileChooser_Adjust_Vol(Gtk.ApplicationWindow):
    def __init__(self, app, treeview_window=None):
        super().__init__(application=app, title="CYON // ADJUST VOLUME")
        self.set_default_size(480, 300)
        self.set_border_width(0)

        self.treeview_window = treeview_window
        self.audio_file = None
        self.dialog = None

        provider = Gtk.CssProvider()
        provider.load_from_data(CSS)
        Gtk.StyleContext.add_provider_for_screen(
            self.get_screen(), provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )

        self.connect("delete-event", self.on_delete_event)

        outer = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        outer.set_margin_top(16)
        outer.set_margin_bottom(12)
        outer.set_margin_start(16)
        outer.set_margin_end(16)
        self.add(outer)

        title = Gtk.Label(label="▸ ADJUST VOLUME")
        title.get_style_context().add_class("title-label")
        title.set_halign(Gtk.Align.START)
        outer.pack_start(title, False, False, 0)

        sep = Gtk.Separator()
        outer.pack_start(sep, False, False, 10)

        self.button1 = Gtk.Button(label="▸ CHOOSE AUDIO")
        self.button1.connect("clicked", self.on_file_clicked)
        outer.pack_start(self.button1, False, False, 4)

        vol_lbl = Gtk.Label(label="VOLUME LEVEL  ( 0 = mute  ·  10 = original  ·  20 = 2x )")
        vol_lbl.set_halign(Gtk.Align.START)
        outer.pack_start(vol_lbl, False, False, 8)

        self.scale = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 0, 20, 1)
        self.scale.set_value(10)
        self.scale.set_draw_value(True)
        outer.pack_start(self.scale, False, False, 0)

        self.button2 = Gtk.Button(label="▸ SELECT A FILE FIRST")
        self.button2.set_sensitive(False)
        self.button2.connect("clicked", self.on_adjust_clicked)
        outer.pack_start(self.button2, False, False, 8)

        sep2 = Gtk.Separator()
        outer.pack_start(sep2, False, False, 8)

        self.status_label = Gtk.Label(label="▸ Select an audio file to begin.")
        self.status_label.get_style_context().add_class("status-label")
        self.status_label.set_xalign(0)
        self.status_label.set_line_wrap(True)
        outer.pack_start(self.status_label, False, False, 0)

        self.show_all()

    def _set_status(self, msg, kind="ok"):
        ctx = self.status_label.get_style_context()
        for c in ["status-ok", "status-err"]:
            ctx.remove_class(c)
        ctx.add_class(f"status-{kind}")
        self.status_label.set_text(msg)

    def on_delete_event(self, widget, event):
        self.destroy()
        if self.treeview_window is not None:
            self.treeview_window.show_all()
        return False

    def on_file_clicked(self, widget):
        self.dialog = Gtk.FileChooserDialog(
            title="Choose an audio file", parent=self,
            action=Gtk.FileChooserAction.OPEN
        )
        self.dialog.set_current_folder(VolumeAdjuster.get_music_dir())
        self.dialog.add_buttons(
            Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
            Gtk.STOCK_OPEN, Gtk.ResponseType.OK,
        )
        response = self.dialog.run()
        if response == Gtk.ResponseType.OK:
            self.audio_file = self.dialog.get_filename()
            self.button2.set_sensitive(True)
            self.button2.set_label("■ ADJUST VOLUME")
            self._set_status(f"▸ Selected: {self.audio_file}")
            self.dialog.hide()
        else:
            self.dialog.destroy()
            self._set_status("▸ No file selected.")

    def on_adjust_clicked(self, widget):
        try:
            volume_level = float(self.scale.get_value())
        except ValueError:
            self._set_status("▸ Error: volume level must be a number.", "err")
            return

        try:
            save_dialog = Gtk.FileChooserDialog(
                title="Save output file", parent=self,
                action=Gtk.FileChooserAction.SAVE,
            )
            save_dialog.add_buttons(
                Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                Gtk.STOCK_SAVE, Gtk.ResponseType.OK,
            )
            save_dialog.set_current_folder(VolumeAdjuster.get_music_dir())

            response = save_dialog.run()
            if response == Gtk.ResponseType.OK:
                output_filename = save_dialog.get_filename()
                if not output_filename.endswith(".mp3"):
                    save_dialog.destroy()
                    self._set_status("▸ Error: output file must end with .mp3", "err")
                    return
                save_dialog.destroy()
                audio_path = self.audio_file

                def adjust_vol_func():
                    GLib.idle_add(self._set_status, "▸ Adjusting volume...")
                    VolumeAdjuster.adjust(audio_path, output_filename, volume_level)
                    self.audio_file = None
                    GLib.idle_add(self.button2.set_sensitive, False)
                    GLib.idle_add(self._set_status, f"▸ Done: {output_filename}")

                threading.Thread(target=adjust_vol_func, daemon=True).start()
            else:
                save_dialog.destroy()
                self._set_status("▸ Save cancelled.")
        except Exception as e:
            self._set_status(f"▸ Error: {e}", "err")


class AdjustVolumeApp(Gtk.Application):
    def __init__(self, treeview_window=None):
        super().__init__(application_id="com.pyra.adjustvolume")
        self.treeview_window = treeview_window
        self.connect("activate", self.on_activate)

    def on_activate(self, app):
        FileChooser_Adjust_Vol(app, treeview_window=self.treeview_window)


if __name__ == "__main__":
    app = AdjustVolumeApp()
    app.run()
