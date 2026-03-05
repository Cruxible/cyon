#!/usr/bin/env python3
# gtk_convert.py — part of pyra_lib / Cyon

import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, Gdk
from PIL import Image
from urllib.parse import unquote
import os

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
combobox button {
    background-color: #0d0d15;
    color: #00cc77;
    border: 1px solid #1a2a20;
    border-radius: 0px;
    font-family: monospace;
    font-size: 11px;
}
combobox button:hover {
    background-color: #003322;
    color: #00ff99;
    border-color: #00ff99;
}
separator { background-color: #1a2a20; }
"""


class ImageConverterApp(Gtk.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app, title="CYON // IMAGE CONVERTER")
        self.set_default_size(440, 240)
        self.set_border_width(0)
        self.input_path = None

        # Apply CSS
        provider = Gtk.CssProvider()
        provider.load_from_data(CSS)
        Gtk.StyleContext.add_provider_for_screen(
            self.get_screen(), provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )

        # Drag and drop
        self.drag_dest_set(Gtk.DestDefaults.ALL, [], Gdk.DragAction.COPY)
        self.drag_dest_add_uri_targets()
        self.connect("drag-data-received", self.on_drag_data_received)

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        vbox.set_margin_top(16)
        vbox.set_margin_bottom(12)
        vbox.set_margin_start(16)
        vbox.set_margin_end(16)
        self.add(vbox)

        title = Gtk.Label(label="▸ IMAGE CONVERTER")
        title.get_style_context().add_class("title-label")
        title.set_halign(Gtk.Align.START)
        vbox.pack_start(title, False, False, 0)

        drop_hint = Gtk.Label(label="drop an image anywhere on this window")
        drop_hint.get_style_context().add_class("status-label")
        drop_hint.set_halign(Gtk.Align.START)
        vbox.pack_start(drop_hint, False, False, 2)

        sep = Gtk.Separator()
        vbox.pack_start(sep, False, False, 10)

        # File row
        file_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        open_btn = Gtk.Button(label="▸ Open Image")
        open_btn.connect("clicked", self.on_open_clicked)
        self.input_label = Gtk.Label(label="no file selected")
        self.input_label.set_ellipsize(3)
        self.input_label.set_hexpand(True)
        self.input_label.set_xalign(0)
        file_row.pack_start(open_btn, False, False, 0)
        file_row.pack_start(self.input_label, True, True, 0)
        vbox.pack_start(file_row, False, False, 6)

        # Format row
        fmt_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        fmt_label = Gtk.Label(label="Output format:")
        fmt_label.set_width_chars(16)
        fmt_label.set_xalign(0)
        self.format_combo = Gtk.ComboBoxText()
        for fmt in ["PNG", "JPG", "BMP", "WEBP"]:
            self.format_combo.append_text(fmt)
        self.format_combo.set_active(0)
        fmt_row.pack_start(fmt_label, False, False, 0)
        fmt_row.pack_start(self.format_combo, False, False, 0)
        vbox.pack_start(fmt_row, False, False, 6)

        self.convert_btn = Gtk.Button(label="■ CONVERT")
        self.convert_btn.set_sensitive(False)
        self.convert_btn.connect("clicked", self.on_convert_clicked)
        vbox.pack_start(self.convert_btn, False, False, 8)

        sep2 = Gtk.Separator()
        vbox.pack_start(sep2, False, False, 6)

        self.status_label = Gtk.Label(label="▸ Ready.")
        self.status_label.get_style_context().add_class("status-label")
        self.status_label.set_xalign(0)
        vbox.pack_start(self.status_label, False, False, 0)

        self.show_all()

    def set_status(self, msg, kind="ok"):
        ctx = self.status_label.get_style_context()
        for c in ["status-ok", "status-err", "status-warn"]:
            ctx.remove_class(c)
        ctx.add_class(f"status-{kind}")
        self.status_label.set_text(msg)

    def on_drag_data_received(self, widget, drag_context, x, y, data, info, time):
        uris = data.get_uris()
        if not uris:
            return
        path = uris[0]
        if path.startswith("file://"):
            path = path[7:]
        path = unquote(path).strip()
        supported = (".png", ".jpg", ".jpeg", ".webp", ".bmp")
        if not path.lower().endswith(supported):
            self.set_status("▸ Unsupported file type.", "err")
            return
        self.input_path = path
        self.input_label.set_text(os.path.basename(path))
        self.convert_btn.set_sensitive(True)
        self.set_status("▸ File loaded via drag & drop.")

    def on_open_clicked(self, btn):
        dialog = Gtk.FileChooserDialog(
            title="Select an Image", parent=self,
            action=Gtk.FileChooserAction.OPEN,
        )
        dialog.add_buttons("_Cancel", Gtk.ResponseType.CANCEL,
                           "_Open", Gtk.ResponseType.ACCEPT)
        f = Gtk.FileFilter()
        f.set_name("Images")
        for mime in ["image/jpeg", "image/png", "image/webp", "image/bmp"]:
            f.add_mime_type(mime)
        dialog.add_filter(f)
        if dialog.run() == Gtk.ResponseType.ACCEPT:
            self.input_path = dialog.get_filename()
            self.input_label.set_text(os.path.basename(self.input_path))
            self.convert_btn.set_sensitive(True)
            self.set_status("▸ File loaded.")
        dialog.destroy()

    def on_convert_clicked(self, btn):
        if not self.input_path:
            self.set_status("▸ No file selected.", "warn")
            return
        fmt = self.format_combo.get_active_text().lower()
        try:
            with Image.open(self.input_path) as img:
                base = os.path.splitext(self.input_path)[0]
                output_path = f"{base}.{fmt}"
                pil_fmt = "JPEG" if fmt == "jpg" else fmt.upper()
                if fmt == "jpg" and img.mode in ("RGBA", "P"):
                    img = img.convert("RGB")
                img.save(output_path, pil_fmt)
                self.set_status(f"▸ Saved: {os.path.basename(output_path)}", "ok")
        except Exception as e:
            self.set_status(f"▸ Failed: {e}", "err")


class ImageConverterApp2(Gtk.Application):
    def __init__(self):
        super().__init__(application_id="com.pyra.imageconverter")
        self.connect("activate", self.on_activate)

    def on_activate(self, app):
        ImageConverterApp(app)


if __name__ == "__main__":
    app = ImageConverterApp2()
    app.run()
