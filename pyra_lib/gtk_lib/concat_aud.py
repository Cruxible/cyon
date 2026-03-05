#!/usr/bin/env python3
# Author: Ioannes Cruxibulum
# concat_aud.py — part of pyra_lib

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

from moviepy import AudioFileClip, concatenate_audioclips

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
separator {
    background-color: #1a2a20;
}
"""


class AudioStitcher:
    @staticmethod
    def get_music_dir():
        return str(Path.home() / "Music")

    @staticmethod
    def stitch(audio_files, output_path):
        clips = [AudioFileClip(f) for f in audio_files]
        final_clip = concatenate_audioclips(clips)
        final_clip.write_audiofile(output_path)


class FileChooser_Stitch_Audio(Gtk.ApplicationWindow):
    def __init__(self, app, treeview_window=None):
        super().__init__(application=app, title="CYON // STITCH AUDIO")
        self.set_default_size(480, 280)
        self.set_border_width(0)

        self.treeview_window = treeview_window
        self.audio_files = []

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

        title = Gtk.Label(label="▸ STITCH AUDIO")
        title.get_style_context().add_class("title-label")
        title.set_halign(Gtk.Align.START)
        outer.pack_start(title, False, False, 0)

        sep = Gtk.Separator()
        outer.pack_start(sep, False, False, 10)

        self.button1 = Gtk.Button(label="▸ CHOOSE FIRST AUDIO")
        self.button1.connect("clicked", self.on_file_clicked)
        outer.pack_start(self.button1, False, False, 4)

        self.button2 = Gtk.Button(label="▸ CHOOSE SECOND AUDIO")
        self.button2.connect("clicked", self.on_file_clicked)
        outer.pack_start(self.button2, False, False, 4)

        self.button3 = Gtk.Button(label="▸ SELECT BOTH FILES FIRST")
        self.button3.set_sensitive(False)
        self.button3.connect("clicked", self.on_stitch_clicked)
        outer.pack_start(self.button3, False, False, 8)

        sep2 = Gtk.Separator()
        outer.pack_start(sep2, False, False, 8)

        self.status_label = Gtk.Label(label="▸ Select two audio files to begin.")
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
        dialog = Gtk.FileChooserDialog(
            title="Choose an audio file", parent=self,
            action=Gtk.FileChooserAction.OPEN
        )
        dialog.set_current_folder(AudioStitcher.get_music_dir())
        dialog.add_buttons(
            Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
            Gtk.STOCK_OPEN, Gtk.ResponseType.OK,
        )
        response = dialog.run()
        if response == Gtk.ResponseType.OK:
            self.audio_files.append(dialog.get_filename())
            self._set_status(f"▸ Audio {len(self.audio_files)} loaded: {os.path.basename(dialog.get_filename())}")
            dialog.hide()
            if len(self.audio_files) == 2:
                self.button3.set_sensitive(True)
                self.button3.set_label("■ STITCH AUDIO")
        else:
            dialog.destroy()
            self._set_status("▸ No file selected.")

    def on_stitch_clicked(self, widget):
        try:
            save_dialog = Gtk.FileChooserDialog(
                title="Save stitched file", parent=self,
                action=Gtk.FileChooserAction.SAVE,
            )
            save_dialog.add_buttons(
                Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                Gtk.STOCK_SAVE, Gtk.ResponseType.OK,
            )
            save_dialog.set_current_folder(AudioStitcher.get_music_dir())

            response = save_dialog.run()
            if response == Gtk.ResponseType.OK:
                output_filename = save_dialog.get_filename()
                if not output_filename.endswith(".mp3"):
                    save_dialog.destroy()
                    self._set_status("▸ Error: output file must end with .mp3", "err")
                    return
                save_dialog.destroy()
                audio_files = list(self.audio_files)

                def stitch_audio_func():
                    GLib.idle_add(self._set_status, "▸ Stitching audio...")
                    AudioStitcher.stitch(audio_files, output_filename)
                    self.audio_files = []
                    GLib.idle_add(self.button3.set_sensitive, False)
                    GLib.idle_add(self._set_status, f"▸ Done: {output_filename}")

                threading.Thread(target=stitch_audio_func, daemon=True).start()
            else:
                save_dialog.destroy()
                self._set_status("▸ Save cancelled.")
        except Exception as e:
            self._set_status(f"▸ Error: {e}", "err")


class ConcatAudApp(Gtk.Application):
    def __init__(self, treeview_window=None):
        super().__init__(application_id="com.pyra.concataud")
        self.treeview_window = treeview_window
        self.connect("activate", self.on_activate)

    def on_activate(self, app):
        FileChooser_Stitch_Audio(app, treeview_window=self.treeview_window)


if __name__ == "__main__":
    app = ConcatAudApp()
    app.run()
