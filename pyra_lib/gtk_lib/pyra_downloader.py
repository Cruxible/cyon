#!/usr/bin/env python3
# Author: Ioannes Cruxibulum
# pyra_downloader.py — part of pyra_lib

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
from pyra_shared import Input, main_logo, HonerableMentions

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
.status-ok   { color: #00ff99; }
.status-err  { color: #ff3355; }
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


class Downloader:
    @staticmethod
    def get_video_dir():
        return str(Path.home() / "Videos")

    @staticmethod
    def get_music_dir():
        return str(Path.home() / "Music")

    @staticmethod
    def fetch_formats(url):
        result = subprocess.run(["yt-dlp", "-F", url], capture_output=True, text=True)
        lines = result.stdout.split("\n")
        formats = [
            line for line in lines
            if ("mp4" in line and "video only" not in line and "audio only" not in line)
        ]
        return "\n".join(formats) if formats else "No formats found."

    @staticmethod
    def download_video(url, format_code, output_filename, on_update):
        process = subprocess.Popen(
            ["yt-dlp", "-f", format_code, "-o", output_filename, url],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        )
        while True:
            output = process.stdout.readline()
            if output == "" and process.poll() is not None:
                break
            if output:
                GLib.idle_add(on_update, output.strip())
        GLib.idle_add(on_update, f"▸ Saved: {output_filename}")

    @staticmethod
    def download_audio(url, output_filename, on_update):
        process = subprocess.Popen(
            ["yt-dlp", "-x", "--audio-format=mp3", "-o", output_filename, url],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        )
        while True:
            output = process.stdout.readline()
            if output == "" and process.poll() is not None:
                break
            if output:
                GLib.idle_add(on_update, output.strip())
        GLib.idle_add(on_update, f"▸ Saved: {output_filename}")


class DownloaderWindow(Gtk.ApplicationWindow):
    def __init__(self, app, treeview_window=None):
        super().__init__(application=app, title="CYON // DOWNLOADER")
        self.set_default_size(500, 380)
        self.set_border_width(0)
        self.treeview_window = treeview_window
        self.connect("delete-event", self.on_delete_event)

        # Apply CSS
        provider = Gtk.CssProvider()
        provider.load_from_data(CSS)
        Gtk.StyleContext.add_provider_for_screen(
            self.get_screen(), provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )

        outer = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        outer.set_margin_top(16)
        outer.set_margin_bottom(12)
        outer.set_margin_start(16)
        outer.set_margin_end(16)
        self.add(outer)

        # Title
        title = Gtk.Label(label="▸ DOWNLOADER // MEDIA FETCH")
        title.get_style_context().add_class("title-label")
        title.set_halign(Gtk.Align.START)
        outer.pack_start(title, False, False, 0)

        sep = Gtk.Separator()
        outer.pack_start(sep, False, False, 8)

        # URL entry
        self.url_entry = Gtk.Entry()
        self.url_entry.set_placeholder_text("Paste YouTube URL here...")
        outer.pack_start(self.url_entry, False, False, 4)

        # Buttons row
        btn_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        outer.pack_start(btn_box, False, False, 4)

        btn_audio = Gtk.Button(label="▸ AUDIO MP3")
        btn_audio.connect("clicked", self.on_aud_download_button_clicked)
        btn_box.pack_start(btn_audio, True, True, 0)

        btn_formats = Gtk.Button(label="▸ FETCH FORMATS")
        btn_formats.connect("clicked", self.on_button_clicked)
        btn_box.pack_start(btn_formats, True, True, 0)

        # Format code entry + download
        fmt_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        outer.pack_start(fmt_box, False, False, 4)

        self.format_entry = Gtk.Entry()
        self.format_entry.set_placeholder_text("Format code...")
        fmt_box.pack_start(self.format_entry, True, True, 0)

        btn_download = Gtk.Button(label="■ DOWNLOAD VIDEO")
        btn_download.connect("clicked", self.on_download_button_clicked)
        fmt_box.pack_start(btn_download, False, False, 0)

        sep2 = Gtk.Separator()
        outer.pack_start(sep2, False, False, 6)

        # Log / output area
        scroll = Gtk.ScrolledWindow()
        scroll.set_min_content_height(140)
        self.log_view = Gtk.TextView()
        self.log_view.set_editable(False)
        self.log_view.set_cursor_visible(False)
        self.log_view.set_wrap_mode(Gtk.WrapMode.WORD_CHAR)
        self.log_buffer = self.log_view.get_buffer()
        scroll.add(self.log_view)
        outer.pack_start(scroll, True, True, 0)

        self.log("▸ Ready. Paste a URL and fetch formats or download directly.")
        self.show_all()

    def log(self, msg):
        end = self.log_buffer.get_end_iter()
        self.log_buffer.insert(end, msg + "\n")
        self.log_view.scroll_to_iter(self.log_buffer.get_end_iter(), 0, False, 0, 0)

    def on_delete_event(self, widget, event):
        self.destroy()
        if self.treeview_window is not None:
            self.treeview_window.show_all()
        return False

    def on_button_clicked(self, widget):
        url = self.url_entry.get_text().strip()
        if not url:
            self.log("▸ Please enter a URL.")
            return
        self.log("▸ Fetching formats...")
        formats = Downloader.fetch_formats(url)
        self.log(formats)

    def on_download_button_clicked(self, widget):
        try:
            url = self.url_entry.get_text().strip()
            format_code = self.format_entry.get_text().strip()
            save_dialog = Gtk.FileChooserDialog(
                title="Save Video", parent=self, action=Gtk.FileChooserAction.SAVE
            )
            save_dialog.add_buttons(
                Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                Gtk.STOCK_SAVE,   Gtk.ResponseType.OK,
            )
            save_dialog.set_current_folder(Downloader.get_video_dir())
            response = save_dialog.run()
            if response == Gtk.ResponseType.OK:
                output_filename = save_dialog.get_filename()
                save_dialog.destroy()
                if not output_filename.endswith(".mp4"):
                    self.log("▸ Error: filename must end with .mp4")
                else:
                    self.log(f"▸ Downloading to {output_filename}...")
                    threading.Thread(
                        target=Downloader.download_video,
                        args=(url, format_code, output_filename, self.log),
                        daemon=True
                    ).start()
            else:
                save_dialog.destroy()
                self.log("▸ Download cancelled.")
        except Exception as e:
            self.log(f"▸ Error: {e}")

    def on_aud_download_button_clicked(self, widget):
        try:
            url = self.url_entry.get_text().strip()
            save_dialog = Gtk.FileChooserDialog(
                title="Save Audio", parent=self, action=Gtk.FileChooserAction.SAVE
            )
            save_dialog.set_current_folder(Downloader.get_music_dir())
            save_dialog.add_buttons(
                Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                Gtk.STOCK_SAVE,   Gtk.ResponseType.OK,
            )
            response = save_dialog.run()
            if response == Gtk.ResponseType.OK:
                output_filename = save_dialog.get_filename()
                save_dialog.destroy()
                if not output_filename.endswith(".mp3"):
                    self.log("▸ Error: filename must end with .mp3")
                else:
                    self.log(f"▸ Downloading audio to {output_filename}...")
                    threading.Thread(
                        target=Downloader.download_audio,
                        args=(url, output_filename, self.log),
                        daemon=True
                    ).start()
            else:
                save_dialog.destroy()
                self.log("▸ Download cancelled.")
        except Exception as e:
            self.log(f"▸ Error: {e}")


class PyraDownloaderApp(Gtk.Application):
    def __init__(self, treeview_window=None):
        super().__init__(application_id="com.pyra.downloader")
        self.treeview_window = treeview_window
        self.connect("activate", self.on_activate)

    def on_activate(self, app):
        DownloaderWindow(app, treeview_window=self.treeview_window)


if __name__ == "__main__":
    app = PyraDownloaderApp()
    app.run()
