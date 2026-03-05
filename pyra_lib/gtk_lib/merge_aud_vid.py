#!/usr/bin/env python3
# Author: Ioannes Cruxibulum
# merge_aud_vid.py — part of pyra_lib

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

from moviepy import VideoFileClip, AudioFileClip

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
.status-label { color: #336655; font-family: monospace; font-size: 10px; }
.status-ok    { color: #00ff99; }
.status-err   { color: #ff3355; }
.status-warn  { color: #ffaa00; }
button {
    background-color: #0d0d15;
    color: #00cc77;
    font-family: monospace;
    font-size: 11px;
    border: 1px solid #1a2a20;
    border-radius: 0px;
    padding: 6px 16px;
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
separator { background-color: #1a2a20; }
"""


class AudioVideoMerger:
    @staticmethod
    def get_video_dir():
        return str(Path.home() / "Videos")

    @staticmethod
    def get_music_dir():
        return str(Path.home() / "Music")

    @staticmethod
    def merge(video_path, audio_path, output_path):
        video = VideoFileClip(video_path)
        audio = AudioFileClip(audio_path)
        final_clip = video.with_audio(audio)
        final_clip.write_videofile(output_path)


class FileChooser_Merge(Gtk.ApplicationWindow):
    def __init__(self, app, treeview_window=None):
        super().__init__(application=app, title="CYON // MERGE AUDIO/VIDEO")
        self.set_default_size(460, 280)
        self.set_border_width(0)
        self.treeview_window = treeview_window
        self.video_file = None
        self.audio_file = None
        self.connect("delete-event", self.on_delete_event)

        # Apply CSS
        provider = Gtk.CssProvider()
        provider.load_from_data(CSS)
        Gtk.StyleContext.add_provider_for_screen(
            self.get_screen(), provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        vbox.set_margin_top(16)
        vbox.set_margin_bottom(12)
        vbox.set_margin_start(16)
        vbox.set_margin_end(16)
        self.add(vbox)

        title = Gtk.Label(label="▸ MERGE AUDIO / VIDEO")
        title.get_style_context().add_class("title-label")
        title.set_halign(Gtk.Align.START)
        vbox.pack_start(title, False, False, 0)

        sep = Gtk.Separator()
        vbox.pack_start(sep, False, False, 10)

        self.button1 = Gtk.Button(label="▸ Choose Video")
        self.button1.set_name("first_button")
        self.button1.connect("clicked", self.on_file_clicked)
        vbox.pack_start(self.button1, False, False, 4)

        self.button2 = Gtk.Button(label="▸ Choose Audio")
        self.button2.set_name("second_button")
        self.button2.connect("clicked", self.on_file_clicked)
        vbox.pack_start(self.button2, False, False, 4)

        self.button3 = Gtk.Button(label="■ MERGE FILES")
        self.button3.set_name("third_button")
        self.button3.set_sensitive(False)
        self.button3.connect("clicked", self.on_merge_clicked)
        vbox.pack_start(self.button3, False, False, 8)

        sep2 = Gtk.Separator()
        vbox.pack_start(sep2, False, False, 6)

        self.message_label = Gtk.Label(label="▸ Ready.")
        self.message_label.get_style_context().add_class("status-label")
        self.message_label.set_xalign(0)
        vbox.pack_start(self.message_label, False, False, 0)

        self.show_all()

    def on_delete_event(self, widget, event):
        self.destroy()
        if self.treeview_window is not None:
            self.treeview_window.show_all()
        return False

    def set_status(self, msg, kind="ok"):
        ctx = self.message_label.get_style_context()
        for c in ["status-ok", "status-err", "status-warn"]:
            ctx.remove_class(c)
        ctx.add_class(f"status-{kind}")
        self.message_label.set_text(msg)

    def on_file_clicked(self, widget):
        dialog = Gtk.FileChooserDialog(
            title="Please choose a file", parent=self,
            action=Gtk.FileChooserAction.OPEN
        )
        if widget.get_label() == "▸ Choose Video":
            dialog.set_current_folder(AudioVideoMerger.get_video_dir())
        elif widget.get_label() == "▸ Choose Audio":
            dialog.set_current_folder(AudioVideoMerger.get_music_dir())
        dialog.add_buttons(Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                           Gtk.STOCK_OPEN, Gtk.ResponseType.OK)
        response = dialog.run()
        if response == Gtk.ResponseType.CANCEL:
            self.set_status("▸ Cancelled.", "warn")
        elif response == Gtk.ResponseType.OK:
            if widget.get_label() == "▸ Choose Video":
                self.video_file = dialog.get_filename()
                self.set_status(f"▸ Video: {os.path.basename(self.video_file)}")
            elif widget.get_label() == "▸ Choose Audio":
                self.audio_file = dialog.get_filename()
                self.set_status(f"▸ Audio: {os.path.basename(self.audio_file)}")
                if self.video_file is not None:
                    self.button3.set_sensitive(True)
        dialog.destroy()

    def on_merge_clicked(self, widget):
        try:
            save_dialog = Gtk.FileChooserDialog(
                title="Save merged file", parent=self,
                action=Gtk.FileChooserAction.SAVE,
            )
            save_dialog.add_buttons(Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                                    Gtk.STOCK_SAVE, Gtk.ResponseType.OK)
            save_dialog.set_current_folder(AudioVideoMerger.get_video_dir())
            response = save_dialog.run()
            if response == Gtk.ResponseType.CANCEL:
                save_dialog.destroy()
                self.set_status("▸ Cancelled.", "warn")
            elif response == Gtk.ResponseType.OK:
                output_filename = save_dialog.get_filename()
                if not output_filename.endswith(".mp4"):
                    self.set_status("▸ Output filename must end with .mp4", "err")
                else:
                    save_dialog.destroy()
                    video_path = self.video_file
                    audio_path = self.audio_file

                    def merge_func():
                        GLib.idle_add(self.set_status, "▸ Merging...", "warn")
                        AudioVideoMerger.merge(video_path, audio_path, output_filename)
                        self.video_file = None
                        self.audio_file = None
                        GLib.idle_add(self.button3.set_sensitive, False)
                        GLib.idle_add(self.set_status, f"▸ Done: {output_filename}", "ok")

                    threading.Thread(target=merge_func, daemon=True).start()
            else:
                self.set_status("▸ No file selected.", "warn")
        except ValueError as e:
            self.set_status(f"▸ Error: {e}", "err")


class MergeAudVidApp(Gtk.Application):
    def __init__(self, treeview_window=None):
        super().__init__(application_id="com.pyra.mergeaudvid")
        self.treeview_window = treeview_window
        self.connect("activate", self.on_activate)

    def on_activate(self, app):
        FileChooser_Merge(app, treeview_window=self.treeview_window)


if __name__ == "__main__":
    app = MergeAudVidApp()
    app.run()
