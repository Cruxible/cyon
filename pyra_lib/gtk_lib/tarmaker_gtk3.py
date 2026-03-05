#!/usr/bin/env python3
# tarmaker_gtk3.py — part of pyra_lib / Cyon

import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk

import os
import tarfile
import shutil
import subprocess
from pathlib import Path

DEFAULT_DEST = str(Path.home() / "Downloads")

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
.status-warn { color: #ffaa00; }
entry {
    background-color: #0d0d15;
    color: #00ff99;
    font-family: monospace;
    font-size: 11px;
    border: 1px solid #1a2a20;
    border-radius: 0px;
}
entry:focus {
    border-color: #00ff99;
}
button {
    background-color: #0d0d15;
    color: #00cc77;
    font-family: monospace;
    font-size: 11px;
    border: 1px solid #1a2a20;
    border-radius: 0px;
    padding: 4px 12px;
}
button:hover {
    background-color: #003322;
    color: #00ff99;
    border-color: #00ff99;
}
notebook {
    background-color: #0a0a0f;
    border: 1px solid #1a2a20;
}
notebook header {
    background-color: #0d0d15;
}
notebook header tab {
    background-color: #0d0d15;
    color: #336655;
    font-family: monospace;
    font-size: 11px;
    padding: 4px 12px;
    border: none;
}
notebook header tab:checked {
    background-color: #003322;
    color: #00ff99;
    border-bottom: 2px solid #00ff99;
}
separator {
    background-color: #1a2a20;
}
"""

# ── TarMaker logic ────────────────────────────────────────────────────────────
class TarMaker:
    @staticmethod
    def make_tarfile(output_filename, source_dir, destination):
        with tarfile.open(output_filename, "w:gz") as tar:
            tar.add(source_dir, arcname=os.path.basename(source_dir))
        dest_path = Path(destination) / output_filename
        shutil.move(output_filename, dest_path)
        return str(dest_path)

    @staticmethod
    def enc_make_tarfile(output_filename, source_dir):
        with tarfile.open(output_filename, "w:gz") as tar:
            tar.add(source_dir, arcname=os.path.basename(source_dir))

    @staticmethod
    def encrypt_tarball(tar_filename, password, destination):
        encrypted_filename = tar_filename + ".gpg"
        subprocess.run(
            ["gpg", "--symmetric", "--cipher-algo", "AES256",
             "--batch", "--yes", "--passphrase", password, tar_filename],
            check=True,
        )
        dest_path = Path(destination) / os.path.basename(encrypted_filename)
        shutil.move(encrypted_filename, dest_path)
        os.remove(tar_filename)
        return str(dest_path)

    @staticmethod
    def decrypt_tarball(encrypted_filename, password, destination):
        decrypted_filename = encrypted_filename.replace(".gpg", "")
        with open(decrypted_filename, "wb") as f:
            subprocess.run(
                ["gpg", "--decrypt", "--batch", "--yes",
                 "--passphrase", password, encrypted_filename],
                stdout=f, check=True,
            )
        dest_path = Path(destination) / os.path.basename(decrypted_filename)
        shutil.move(decrypted_filename, dest_path)
        with tarfile.open(dest_path, "r:gz") as tar:
            tar.extractall(str(destination))
        return str(dest_path)

    @staticmethod
    def extract_tarball(tar_path, destination):
        with tarfile.open(tar_path, "r:*") as tar:
            tar.extractall(str(destination))


# ── Helpers ───────────────────────────────────────────────────────────────────
def pick_folder(parent, title):
    dialog = Gtk.FileChooserDialog(
        title=title, parent=parent,
        action=Gtk.FileChooserAction.SELECT_FOLDER,
    )
    dialog.add_buttons("_Cancel", Gtk.ResponseType.CANCEL, "_Select", Gtk.ResponseType.ACCEPT)
    path = dialog.get_filename() if dialog.run() == Gtk.ResponseType.ACCEPT else None
    dialog.destroy()
    return path

def pick_file(parent, title, patterns=None):
    dialog = Gtk.FileChooserDialog(
        title=title, parent=parent,
        action=Gtk.FileChooserAction.OPEN,
    )
    dialog.add_buttons("_Cancel", Gtk.ResponseType.CANCEL, "_Open", Gtk.ResponseType.ACCEPT)
    if patterns:
        f = Gtk.FileFilter()
        f.set_name("Tarballs")
        for p in patterns:
            f.add_pattern(p)
        dialog.add_filter(f)
    path = dialog.get_filename() if dialog.run() == Gtk.ResponseType.ACCEPT else None
    dialog.destroy()
    return path

def btn_label_box(btn, lbl):
    box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=6)
    box.pack_start(btn, False, False, 0)
    box.pack_start(lbl, True, True, 0)
    return box

def labeled_row(label_text, widget):
    row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
    lbl = Gtk.Label(label=label_text)
    lbl.set_width_chars(18)
    lbl.set_xalign(0)
    row.pack_start(lbl, False, False, 0)
    row.pack_start(widget, True, True, 0)
    return row


# ── Tabs ──────────────────────────────────────────────────────────────────────
class CreateTab(Gtk.Box):
    def __init__(self, status_fn):
        super().__init__(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        self.set_border_width(16)
        self.status = status_fn
        self.source_path = None
        self.dest_path = DEFAULT_DEST

        self.source_label = Gtk.Label(label="none selected")
        self.source_label.set_ellipsize(3)
        self.source_label.set_hexpand(True)
        src_btn = Gtk.Button(label="▸ Source Directory")
        src_btn.connect("clicked", lambda _: self._pick_source())
        self.pack_start(labeled_row("Source:", btn_label_box(src_btn, self.source_label)), False, False, 0)

        self.name_entry = Gtk.Entry()
        self.name_entry.set_placeholder_text("e.g. my_backup")
        self.pack_start(labeled_row("Tarball name:", self.name_entry), False, False, 0)

        self.dest_label = Gtk.Label(label=DEFAULT_DEST)
        self.dest_label.set_ellipsize(3)
        self.dest_label.set_hexpand(True)
        dest_btn = Gtk.Button(label="▸ Destination")
        dest_btn.connect("clicked", lambda _: self._pick_dest())
        self.pack_start(labeled_row("Destination:", btn_label_box(dest_btn, self.dest_label)), False, False, 0)

        go_btn = Gtk.Button(label="■ CREATE TARBALL")
        go_btn.connect("clicked", self.on_create)
        self.pack_start(go_btn, False, False, 8)

    def _pick_source(self):
        p = pick_folder(self.get_toplevel(), "Select source directory")
        if p: self.source_path = p; self.source_label.set_text(p)

    def _pick_dest(self):
        p = pick_folder(self.get_toplevel(), "Select destination")
        if p: self.dest_path = p; self.dest_label.set_text(p)

    def on_create(self, _):
        if not self.source_path: self.status("⚠ Please select a source directory.", "warn"); return
        name = self.name_entry.get_text().strip()
        if not name: self.status("⚠ Please enter a tarball name.", "warn"); return
        try:
            out = TarMaker.make_tarfile(f"{name}.tar.gz", self.source_path, self.dest_path)
            self.status(f"▸ Created: {out}", "ok")
        except Exception as e:
            self.status(f"▸ Error: {e}", "err")


class EncryptTab(Gtk.Box):
    def __init__(self, status_fn):
        super().__init__(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        self.set_border_width(16)
        self.status = status_fn
        self.source_path = None
        self.dest_path = DEFAULT_DEST

        self.source_label = Gtk.Label(label="none selected")
        self.source_label.set_ellipsize(3)
        self.source_label.set_hexpand(True)
        src_btn = Gtk.Button(label="▸ Source Directory")
        src_btn.connect("clicked", lambda _: self._pick_source())
        self.pack_start(labeled_row("Source:", btn_label_box(src_btn, self.source_label)), False, False, 0)

        self.name_entry = Gtk.Entry()
        self.name_entry.set_placeholder_text("e.g. secure_backup")
        self.pack_start(labeled_row("Tarball name:", self.name_entry), False, False, 0)

        self.dest_label = Gtk.Label(label=DEFAULT_DEST)
        self.dest_label.set_ellipsize(3)
        self.dest_label.set_hexpand(True)
        dest_btn = Gtk.Button(label="▸ Destination")
        dest_btn.connect("clicked", lambda _: self._pick_dest())
        self.pack_start(labeled_row("Destination:", btn_label_box(dest_btn, self.dest_label)), False, False, 0)

        self.pass_entry = Gtk.Entry()
        self.pass_entry.set_visibility(False)
        self.pass_entry.set_placeholder_text("Encryption password")
        self.pack_start(labeled_row("Password:", self.pass_entry), False, False, 0)

        go_btn = Gtk.Button(label="■ CREATE & ENCRYPT")
        go_btn.connect("clicked", self.on_encrypt)
        self.pack_start(go_btn, False, False, 8)

    def _pick_source(self):
        p = pick_folder(self.get_toplevel(), "Select source directory")
        if p: self.source_path = p; self.source_label.set_text(p)

    def _pick_dest(self):
        p = pick_folder(self.get_toplevel(), "Select destination")
        if p: self.dest_path = p; self.dest_label.set_text(p)

    def on_encrypt(self, _):
        if not self.source_path: self.status("⚠ Please select a source directory.", "warn"); return
        name = self.name_entry.get_text().strip()
        if not name: self.status("⚠ Please enter a tarball name.", "warn"); return
        password = self.pass_entry.get_text()
        if not password: self.status("⚠ Please enter a password.", "warn"); return
        try:
            tar_filename = f"{name}.tar.gz"
            TarMaker.enc_make_tarfile(tar_filename, self.source_path)
            out = TarMaker.encrypt_tarball(tar_filename, password, self.dest_path)
            self.status(f"▸ Encrypted: {out}", "ok")
        except Exception as e:
            self.status(f"▸ Error: {e}", "err")


class DecryptTab(Gtk.Box):
    def __init__(self, status_fn):
        super().__init__(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        self.set_border_width(16)
        self.status = status_fn
        self.enc_path = None
        self.dest_path = DEFAULT_DEST

        self.file_label = Gtk.Label(label="none selected")
        self.file_label.set_ellipsize(3)
        self.file_label.set_hexpand(True)
        file_btn = Gtk.Button(label="▸ Select .tar.gz.gpg")
        file_btn.connect("clicked", lambda _: self._pick_file())
        self.pack_start(labeled_row("Encrypted file:", btn_label_box(file_btn, self.file_label)), False, False, 0)

        self.dest_label = Gtk.Label(label=DEFAULT_DEST)
        self.dest_label.set_ellipsize(3)
        self.dest_label.set_hexpand(True)
        dest_btn = Gtk.Button(label="▸ Destination")
        dest_btn.connect("clicked", lambda _: self._pick_dest())
        self.pack_start(labeled_row("Destination:", btn_label_box(dest_btn, self.dest_label)), False, False, 0)

        self.pass_entry = Gtk.Entry()
        self.pass_entry.set_visibility(False)
        self.pass_entry.set_placeholder_text("Decryption password")
        self.pack_start(labeled_row("Password:", self.pass_entry), False, False, 0)

        go_btn = Gtk.Button(label="■ DECRYPT & EXTRACT")
        go_btn.connect("clicked", self.on_decrypt)
        self.pack_start(go_btn, False, False, 8)

    def _pick_file(self):
        p = pick_file(self.get_toplevel(), "Select encrypted tarball", ["*.gpg"])
        if p: self.enc_path = p; self.file_label.set_text(p)

    def _pick_dest(self):
        p = pick_folder(self.get_toplevel(), "Select destination")
        if p: self.dest_path = p; self.dest_label.set_text(p)

    def on_decrypt(self, _):
        if not self.enc_path: self.status("⚠ Please select an encrypted tarball.", "warn"); return
        password = self.pass_entry.get_text()
        if not password: self.status("⚠ Please enter a password.", "warn"); return
        try:
            TarMaker.decrypt_tarball(self.enc_path, password, self.dest_path)
            self.status(f"▸ Decrypted & extracted to: {self.dest_path}", "ok")
        except Exception as e:
            self.status(f"▸ Error: {e}", "err")


class ExtractTab(Gtk.Box):
    def __init__(self, status_fn):
        super().__init__(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        self.set_border_width(16)
        self.status = status_fn
        self.tar_path = None
        self.dest_path = DEFAULT_DEST

        self.file_label = Gtk.Label(label="none selected")
        self.file_label.set_ellipsize(3)
        self.file_label.set_hexpand(True)
        file_btn = Gtk.Button(label="▸ Select Tarball")
        file_btn.connect("clicked", lambda _: self._pick_file())
        self.pack_start(labeled_row("Tarball:", btn_label_box(file_btn, self.file_label)), False, False, 0)

        self.dest_label = Gtk.Label(label=DEFAULT_DEST)
        self.dest_label.set_ellipsize(3)
        self.dest_label.set_hexpand(True)
        dest_btn = Gtk.Button(label="▸ Destination")
        dest_btn.connect("clicked", lambda _: self._pick_dest())
        self.pack_start(labeled_row("Extract to:", btn_label_box(dest_btn, self.dest_label)), False, False, 0)

        go_btn = Gtk.Button(label="■ EXTRACT TARBALL")
        go_btn.connect("clicked", self.on_extract)
        self.pack_start(go_btn, False, False, 8)

    def _pick_file(self):
        p = pick_file(self.get_toplevel(), "Select tarball", ["*.tar.gz", "*.tar.bz2", "*.tar.xz", "*.tar"])
        if p: self.tar_path = p; self.file_label.set_text(p)

    def _pick_dest(self):
        p = pick_folder(self.get_toplevel(), "Select destination")
        if p: self.dest_path = p; self.dest_label.set_text(p)

    def on_extract(self, _):
        if not self.tar_path: self.status("⚠ Please select a tarball.", "warn"); return
        try:
            TarMaker.extract_tarball(self.tar_path, self.dest_path)
            self.status(f"▸ Extracted to: {self.dest_path}", "ok")
        except Exception as e:
            self.status(f"▸ Error: {e}", "err")


# ── Main window ───────────────────────────────────────────────────────────────
class TarMakerWindow(Gtk.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app, title="CYON // TARMAKER")
        self.set_default_size(520, 320)
        self.set_border_width(0)

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

        title = Gtk.Label(label="▸ TARMAKER // ARCHIVE CONTROL")
        title.get_style_context().add_class("title-label")
        title.set_halign(Gtk.Align.START)
        vbox.pack_start(title, False, False, 0)

        sep = Gtk.Separator()
        vbox.pack_start(sep, False, False, 8)

        notebook = Gtk.Notebook()
        vbox.pack_start(notebook, True, True, 0)
        notebook.append_page(CreateTab(self.set_status),  Gtk.Label(label="CREATE"))
        notebook.append_page(EncryptTab(self.set_status), Gtk.Label(label="ENCRYPT"))
        notebook.append_page(DecryptTab(self.set_status), Gtk.Label(label="DECRYPT"))
        notebook.append_page(ExtractTab(self.set_status), Gtk.Label(label="EXTRACT"))

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


class TarMakerApp(Gtk.Application):
    def __init__(self):
        super().__init__(application_id="com.pyra.tarmaker")
        self.connect("activate", self.on_activate)

    def on_activate(self, app):
        TarMakerWindow(app)


if __name__ == "__main__":
    app = TarMakerApp()
    app.run()
