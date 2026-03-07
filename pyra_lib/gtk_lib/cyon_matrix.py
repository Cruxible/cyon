#!/usr/bin/env python3
# cmatrix_gtk3.py — part of pyra_lib / Cyon
# Matrix rain rendered in a resizable GTK3 DrawingArea, cyan palette.
#
# Run:
#   python3 cmatrix_gtk3.py
#
# Requires:
#   sudo apt install python3-gi python3-gi-cairo gir1.2-gtk-3.0

import gi
gi.require_version("Gtk", "3.0")
gi.require_version("Gdk", "3.0")
from gi.repository import Gtk, Gdk, GLib

import random
import math
import cairo

# ── tuneable constants ─────────────────────────────────────────────────────────
CELL        = 16          # px — font size & grid cell size
SPEED_MS    = 55          # timer interval (ms)
TRAIL_MIN   = 6
TRAIL_MAX   = 22
SPEED_MIN   = 1           # col moves every N ticks (1 = fastest)
SPEED_MAX   = 3

# Cyon palette — cyan-green on near-black
COL_BG      = (0.039, 0.039, 0.059)        # #0a0a0f
COL_HEAD    = (0.85,  1.00,  1.00)         # near-white head
COL_HOT     = (0.00,  1.00,  0.60)         # #00ff99  bright body
COL_MID     = (0.00,  0.80,  0.47)         # #00cc77  mid body
COL_DIM     = (0.00,  0.40,  0.25)         # dim tail
COL_FADE    = (0.00,  0.15,  0.10)         # ghost tail

# Glyphs: half-width katakana + digits + symbols
_KATA = [chr(c) for c in range(0xFF66, 0xFF9E)]
_SYMS = list("0123456789ZTXMEOB:=+*|<>!?#@")
GLYPHS = _KATA + _SYMS

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
    font-size: 13px;
    font-weight: bold;
    letter-spacing: 3px;
}
.status-label {
    color: #336655;
    font-family: monospace;
    font-size: 10px;
}
button {
    background-color: #0d0d15;
    color: #00cc77;
    font-family: monospace;
    font-size: 11px;
    border: 1px solid #1a2a20;
    border-radius: 0px;
    padding: 3px 10px;
}
button:hover {
    background-color: #003322;
    color: #00ff99;
    border-color: #00ff99;
}
separator {
    background-color: #1a2a20;
    min-height: 1px;
}
"""

# ── Column ─────────────────────────────────────────────────────────────────────
class Column:
    __slots__ = ("row", "length", "speed", "tick", "active", "wait", "glyphs")

    def __init__(self, n_rows):
        self.glyphs = [random.choice(GLYPHS) for _ in range(TRAIL_MAX)]
        self.reset(n_rows, cold=True)

    def reset(self, n_rows, cold=False):
        self.length = random.randint(TRAIL_MIN, TRAIL_MAX)
        self.speed  = random.randint(SPEED_MIN, SPEED_MAX)
        self.tick   = 0
        if cold:
            self.active = False
            self.wait   = random.randint(0, 80)
            self.row    = 0
        else:
            self.active = True
            self.wait   = 0
            self.row    = -(random.randint(0, self.length))

    def advance(self, n_rows):
        if not self.active:
            self.wait -= 1
            if self.wait <= 0:
                self.reset(n_rows)
            return
        self.tick += 1
        if self.tick < self.speed:
            return
        self.tick = 0
        # randomly mutate a glyph
        if random.random() < 0.25:
            self.glyphs[random.randrange(TRAIL_MAX)] = random.choice(GLYPHS)
        self.row += 1
        if self.row - self.length >= n_rows:
            self.active = False
            self.wait   = random.randint(8, 60)


# ── CMatrixWidget ──────────────────────────────────────────────────────────────
class CMatrixWidget(Gtk.DrawingArea):
    def __init__(self):
        super().__init__()
        self._cols: list[Column] = []
        self._n_cols = 0
        self._n_rows = 0
        self._font   = "monospace"

        self.connect("draw", self._on_draw)
        self.connect("size-allocate", self._on_size_allocate)
        self._timer = GLib.timeout_add(SPEED_MS, self._on_tick)

    # ── resize ─────────────────────────────────────────────────────────────────
    def _on_size_allocate(self, _widget, alloc):
        new_cols = max(1, alloc.width  // CELL)
        new_rows = max(1, alloc.height // CELL)
        if new_cols == self._n_cols and new_rows == self._n_rows:
            return
        self._n_cols = new_cols
        self._n_rows = new_rows
        # grow or shrink column list
        while len(self._cols) < new_cols:
            self._cols.append(Column(new_rows))
        self._cols = self._cols[:new_cols]

    # ── timer ──────────────────────────────────────────────────────────────────
    def _on_tick(self):
        for c in self._cols:
            c.advance(self._n_rows)
        self.queue_draw()
        return GLib.SOURCE_CONTINUE

    # ── draw ───────────────────────────────────────────────────────────────────
    def _on_draw(self, _widget, cr: cairo.Context):
        w = self.get_allocated_width()
        h = self.get_allocated_height()

        # background
        cr.set_source_rgb(*COL_BG)
        cr.rectangle(0, 0, w, h)
        cr.fill()

        cr.select_font_face(self._font,
                            cairo.FONT_SLANT_NORMAL,
                            cairo.FONT_WEIGHT_NORMAL)
        cr.set_font_size(CELL - 1)

        for col_idx, col in enumerate(self._cols):
            if not col.active:
                continue
            x = col_idx * CELL
            head = col.row

            for i in range(col.length):
                row = head - i
                if row < 0 or row >= self._n_rows:
                    continue
                y = row * CELL + CELL  # baseline

                # colour by position in trail
                if i == 0:
                    r, g, b = COL_HEAD
                elif i == 1:
                    r, g, b = COL_HOT
                elif i < col.length // 3:
                    t = i / (col.length // 3)
                    r = COL_HOT[0] * (1 - t) + COL_MID[0] * t
                    g = COL_HOT[1] * (1 - t) + COL_MID[1] * t
                    b = COL_HOT[2] * (1 - t) + COL_MID[2] * t
                elif i < col.length * 2 // 3:
                    t = (i - col.length // 3) / (col.length // 3)
                    r = COL_MID[0] * (1 - t) + COL_DIM[0] * t
                    g = COL_MID[1] * (1 - t) + COL_DIM[1] * t
                    b = COL_MID[2] * (1 - t) + COL_DIM[2] * t
                else:
                    t = (i - col.length * 2 // 3) / max(1, col.length // 3)
                    r = COL_DIM[0] * (1 - t) + COL_FADE[0] * t
                    g = COL_DIM[1] * (1 - t) + COL_FADE[1] * t
                    b = COL_DIM[2] * (1 - t) + COL_FADE[2] * t

                cr.set_source_rgb(r, g, b)
                glyph = col.glyphs[i % TRAIL_MAX]
                cr.move_to(x, y)
                cr.show_text(glyph)

    def stop(self):
        if self._timer:
            GLib.source_remove(self._timer)
            self._timer = None


# ── Main window ────────────────────────────────────────────────────────────────
class CMatrixWindow(Gtk.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app, title="CYON // CMATRIX")
        self.set_default_size(900, 560)
        self.set_resizable(True)
        self.set_border_width(0)

        # CSS
        provider = Gtk.CssProvider()
        provider.load_from_data(CSS)
        Gtk.StyleContext.add_provider_for_screen(
            self.get_screen(), provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )

        # outer layout
        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        self.add(vbox)

        # ── header bar ────────────────────────────────────────────────────────
        hbar = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        hbar.set_margin_top(10)
        hbar.set_margin_bottom(6)
        hbar.set_margin_start(14)
        hbar.set_margin_end(14)
        vbox.pack_start(hbar, False, False, 0)

        title_lbl = Gtk.Label(label="▸ CYON // CMATRIX")
        title_lbl.get_style_context().add_class("title-label")
        title_lbl.set_halign(Gtk.Align.START)
        hbar.pack_start(title_lbl, True, True, 0)

        self._pause_btn = Gtk.Button(label="⏸  PAUSE")
        self._pause_btn.connect("clicked", self._toggle_pause)
        hbar.pack_end(self._pause_btn, False, False, 0)

        sep_top = Gtk.Separator()
        vbox.pack_start(sep_top, False, False, 0)

        # ── drawing area ──────────────────────────────────────────────────────
        self._matrix = CMatrixWidget()
        self._matrix.set_hexpand(True)
        self._matrix.set_vexpand(True)
        vbox.pack_start(self._matrix, True, True, 0)

        # ── footer ────────────────────────────────────────────────────────────
        sep_bot = Gtk.Separator()
        vbox.pack_start(sep_bot, False, False, 0)

        fbar = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
        fbar.set_margin_top(4)
        fbar.set_margin_bottom(6)
        fbar.set_margin_start(14)
        fbar.set_margin_end(14)
        vbox.pack_start(fbar, False, False, 0)

        self._status = Gtk.Label(label="▸ MATRIX ACTIVE. REALITY OPTIONAL.")
        self._status.get_style_context().add_class("status-label")
        self._status.set_xalign(0)
        fbar.pack_start(self._status, True, True, 0)

        hint = Gtk.Label(label="ESC / Q  quit")
        hint.get_style_context().add_class("status-label")
        hint.set_xalign(1)
        fbar.pack_end(hint, False, False, 0)

        # key handler
        self.connect("key-press-event", self._on_key)
        self.connect("delete-event", self._on_close)
        self._paused = False

        self.show_all()

    # ── close ─────────────────────────────────────────────────────────────────
    def _on_close(self, widget, event):
        self._matrix.stop()
        return False

    # ── pause / resume ────────────────────────────────────────────────────────
    def _toggle_pause(self, _btn):
        if self._paused:
            self._matrix._timer = GLib.timeout_add(SPEED_MS, self._matrix._on_tick)
            self._paused = False
            self._pause_btn.set_label("⏸  PAUSE")
            self._status.set_text("▸ MATRIX ACTIVE. REALITY OPTIONAL.")
        else:
            self._matrix.stop()
            self._paused = True
            self._pause_btn.set_label("▶  RESUME")
            self._status.set_text("▸ PAUSED. REALITY RESTORED.")

    # ── keyboard ──────────────────────────────────────────────────────────────
    def _on_key(self, _w, ev):
        key = ev.keyval
        if key in (Gdk.KEY_Escape, Gdk.KEY_q, Gdk.KEY_Q):
            self.get_application().quit()
            return True
        if key in (Gdk.KEY_space, Gdk.KEY_p, Gdk.KEY_P):
            self._toggle_pause(None)
            return True
        return False


# ── Application ────────────────────────────────────────────────────────────────
class CMatrixApp(Gtk.Application):
    def __init__(self):
        super().__init__(application_id="com.pyra.cmatrix")
        self.connect("activate", self._on_activate)

    def _on_activate(self, app):
        CMatrixWindow(app)


if __name__ == "__main__":
    app = CMatrixApp()
    app.run()
