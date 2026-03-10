#!/usr/bin/env python3
"""
GTK Color Picker
────────────────
Requirements:
    sudo apt install python3-gi python3-gi-cairo gir1.2-gtk-3.0   # Debian/Ubuntu
    sudo dnf install python3-gobject gtk3                          # Fedora

Run:
    python3 color_picker.py
"""

import gi
gi.require_version("Gtk", "3.0")
gi.require_version("Gdk", "3.0")
from gi.repository import Gtk, Gdk
import colorsys
import math
import cairo


# ── colour helpers ─────────────────────────────────────────────────────────────

def hsv_to_rgb(h, s, v):
    return colorsys.hsv_to_rgb(h, s, v)

def rgb_to_hsv(r, g, b):
    return colorsys.rgb_to_hsv(r, g, b)

def rgb_to_hsl_str(r, g, b):
    h, l, s = colorsys.rgb_to_hls(r, g, b)
    return f"hsl({round(h*360)}, {round(s*100)}%, {round(l*100)}%)"

def rgb_to_hex(r, g, b):
    return "#{:02x}{:02x}{:02x}".format(round(r*255), round(g*255), round(b*255))

def hex_to_rgb(hex_str):
    hex_str = hex_str.lstrip("#")
    if len(hex_str) != 6:
        return None
    try:
        return tuple(int(hex_str[i:i+2], 16) / 255 for i in (0, 2, 4))
    except ValueError:
        return None

def luminance(r, g, b):
    return 0.299 * r + 0.587 * g + 0.114 * b


# ── SV canvas ──────────────────────────────────────────────────────────────────

class SVCanvas(Gtk.DrawingArea):
    SIZE = 260

    def __init__(self, on_change):
        super().__init__()
        self.hue = 0.58
        self.sat = 0.75
        self.val = 0.90
        self._on_change = on_change
        self._dragging = False

        self.set_size_request(self.SIZE, self.SIZE)
        self.add_events(
            Gdk.EventMask.BUTTON_PRESS_MASK |
            Gdk.EventMask.BUTTON_RELEASE_MASK |
            Gdk.EventMask.POINTER_MOTION_MASK
        )
        self.connect("draw", self._draw)
        self.connect("button-press-event",   self._press)
        self.connect("button-release-event", lambda w, e: setattr(self, "_dragging", False))
        self.connect("motion-notify-event",  self._motion)

    def set_hue(self, hue):
        self.hue = hue
        self.queue_draw()

    def set_sv(self, s, v):
        self.sat = s
        self.val = v
        self.queue_draw()

    def _pick(self, x, y):
        S = self.SIZE
        self.sat = max(0.0, min(1.0, x / S))
        self.val = max(0.0, min(1.0, 1.0 - y / S))
        self.queue_draw()
        self._on_change(self.sat, self.val)

    def _press(self, _, e):
        self._dragging = True
        self._pick(e.x, e.y)

    def _motion(self, _, e):
        if self._dragging:
            self._pick(e.x, e.y)

    def _draw(self, _, cr):
        S = self.SIZE
        # Draw gradient using Cairo linear gradients layered
        # White→hue (horizontal), then transparent→black (vertical overlay)
        hue_r, hue_g, hue_b = hsv_to_rgb(self.hue, 1.0, 1.0)

        # Base: white to hue color
        grad_h = cairo.LinearGradient(0, 0, S, 0)
        grad_h.add_color_stop_rgb(0, 1, 1, 1)
        grad_h.add_color_stop_rgb(1, hue_r, hue_g, hue_b)
        cr.rectangle(0, 0, S, S)
        cr.set_source(grad_h)
        cr.fill()

        # Overlay: transparent to black (top to bottom)
        grad_v = cairo.LinearGradient(0, 0, 0, S)
        grad_v.add_color_stop_rgba(0, 0, 0, 0, 0)
        grad_v.add_color_stop_rgba(1, 0, 0, 0, 1)
        cr.rectangle(0, 0, S, S)
        cr.set_source(grad_v)
        cr.fill()

        # Crosshair
        cx = self.sat * S
        cy = (1 - self.val) * S
        r, g, b = hsv_to_rgb(self.hue, self.sat, self.val)
        fg = 1.0 if luminance(r, g, b) < 0.5 else 0.0

        cr.set_line_width(2.0)
        cr.set_source_rgb(fg, fg, fg)
        cr.arc(cx, cy, 7, 0, 2 * math.pi)
        cr.stroke()
        cr.set_line_width(1.0)
        cr.set_source_rgba(1-fg, 1-fg, 1-fg, 0.6)
        cr.arc(cx, cy, 9.5, 0, 2 * math.pi)
        cr.stroke()


# ── hue bar ────────────────────────────────────────────────────────────────────

class HueBar(Gtk.DrawingArea):
    HEIGHT = 24

    def __init__(self, on_change):
        super().__init__()
        self.hue = 0.58
        self._on_change = on_change
        self._dragging = False

        self.set_size_request(-1, self.HEIGHT)
        self.add_events(
            Gdk.EventMask.BUTTON_PRESS_MASK |
            Gdk.EventMask.BUTTON_RELEASE_MASK |
            Gdk.EventMask.POINTER_MOTION_MASK
        )
        self.connect("draw", self._draw)
        self.connect("button-press-event",   self._press)
        self.connect("button-release-event", lambda w, e: setattr(self, "_dragging", False))
        self.connect("motion-notify-event",  self._motion)

    def set_hue(self, hue):
        self.hue = hue
        self.queue_draw()

    def _pick(self, x):
        W = self.get_allocation().width
        self.hue = max(0.0, min(1.0, x / W))
        self.queue_draw()
        self._on_change(self.hue)

    def _press(self, _, e):
        self._dragging = True
        self._pick(e.x)

    def _motion(self, _, e):
        if self._dragging:
            self._pick(e.x)

    def _draw(self, _, cr):
        alloc = self.get_allocation()
        W, H = alloc.width, alloc.height

        # Rainbow gradient
        grad = cairo.LinearGradient(0, 0, W, 0)
        for i in range(7):
            r, g, b = hsv_to_rgb(i / 6, 1.0, 1.0)
            grad.add_color_stop_rgb(i / 6, r, g, b)
        cr.rectangle(0, 0, W, H)
        cr.set_source(grad)
        cr.fill()

        # Thumb
        tx = self.hue * W
        cr.set_source_rgb(0, 0, 0)
        cr.set_line_width(1.5)
        cr.rectangle(tx - 6, 0, 12, H)
        cr.stroke()
        cr.set_source_rgb(1, 1, 1)
        cr.set_line_width(1.5)
        cr.rectangle(tx - 5, 1, 10, H - 2)
        cr.stroke()


# ── swatch button row ──────────────────────────────────────────────────────────

class SwatchRow(Gtk.Box):
    MAX = 10

    def __init__(self, on_select):
        super().__init__(orientation=Gtk.Orientation.HORIZONTAL, spacing=5)
        self._on_select = on_select
        self._buttons = []

    def add_color(self, hex_color):
        if len(self._buttons) >= self.MAX:
            old = self._buttons.pop(0)
            self.remove(old)

        btn = Gtk.Button()
        btn.set_size_request(30, 30)
        btn.set_tooltip_text(hex_color)

        css = f"""
        button {{
            background: {hex_color};
            border-radius: 5px;
            border: 2px solid rgba(255,255,255,0.12);
            min-width: 30px; min-height: 30px;
            padding: 0;
        }}
        button:hover {{ border-color: rgba(255,255,255,0.55); }}
        """
        p = Gtk.CssProvider()
        p.load_from_data(css.encode())
        btn.get_style_context().add_provider(p, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        btn.connect("clicked", lambda b, c=hex_color: self._on_select(c))
        self.pack_start(btn, False, False, 0)
        btn.show()
        self._buttons.append(btn)


# ── main window ────────────────────────────────────────────────────────────────

class ColorPickerWindow(Gtk.Window):

    def __init__(self):
        super().__init__(title="Color Picker")
        self.set_resizable(False)
        self.set_border_width(20)
        self.connect("destroy", Gtk.main_quit)

        self._apply_global_css()

        self._hue = 0.58
        self._sat = 0.75
        self._val = 0.90

        root = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12)
        self.add(root)

        # Title
        title = Gtk.Label(label="◈  COLOR PICKER")
        title.set_xalign(0)
        self._css(title, "label { color:#777; font-family:monospace; font-size:11px; font-weight:bold; }")
        root.pack_start(title, False, False, 0)

        # SV canvas
        self._sv = SVCanvas(self._on_sv_change)
        self._sv.hue = self._hue
        self._sv.sat = self._sat
        self._sv.val = self._val
        root.pack_start(self._sv, False, False, 0)

        # Hue label + bar
        hl = Gtk.Label(label="HUE")
        hl.set_xalign(0)
        self._css(hl, "label { color:#555; font-family:monospace; font-size:9px; }")
        root.pack_start(hl, False, False, 0)

        self._hue_bar = HueBar(self._on_hue_change)
        self._hue_bar.hue = self._hue
        root.pack_start(self._hue_bar, False, False, 0)

        # Preview + text fields
        preview_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        root.pack_start(preview_row, False, False, 0)

        self._preview = Gtk.DrawingArea()
        self._preview.set_size_request(72, 72)
        self._preview.connect("draw", self._draw_preview)
        preview_row.pack_start(self._preview, False, False, 0)

        fields = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5)
        preview_row.pack_start(fields, True, True, 0)

        self._hex_entry = self._field("HEX", fields)
        self._rgb_entry = self._field("RGB", fields)
        self._hsl_entry = self._field("HSL", fields)
        self._hex_entry.connect("activate", self._on_hex_entered)

        # Copy buttons
        copy_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=6)
        root.pack_start(copy_row, False, False, 0)

        for label, getter in [("Copy HEX", lambda: self._hex_entry.get_text()),
                               ("Copy RGB", lambda: self._rgb_entry.get_text()),
                               ("Copy HSL", lambda: self._hsl_entry.get_text())]:
            btn = Gtk.Button(label=label)
            self._css(btn, """
                button { background:#2a2a2a; color:#bbb; border:1px solid #444;
                         border-radius:6px; font-family:monospace; font-size:10px; padding:3px 8px; }
                button:hover { background:#383838; color:#fff; }
            """)
            btn.connect("clicked", lambda _, g=getter: self._copy(g()))
            copy_row.pack_start(btn, True, True, 0)

        # Separator
        sep = Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL)
        self._css(sep, "separator { background:#333; }")
        root.pack_start(sep, False, False, 0)

        # Save row
        save_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        root.pack_start(save_row, False, False, 0)

        sl = Gtk.Label(label="SAVED COLORS")
        sl.set_xalign(0)
        self._css(sl, "label { color:#555; font-family:monospace; font-size:9px; }")
        save_row.pack_start(sl, False, False, 0)

        save_btn = Gtk.Button(label="＋ Save")
        self._css(save_btn, """
            button { background:#1e3a5f; color:#7ab8f5; border:1px solid #2a5a9f;
                     border-radius:6px; font-family:monospace; font-size:10px; padding:2px 10px; }
            button:hover { background:#254d7a; }
        """)
        save_btn.connect("clicked", self._save_color)
        save_row.pack_end(save_btn, False, False, 0)

        self._swatches = SwatchRow(self._load_swatch)
        root.pack_start(self._swatches, False, False, 0)

        self._update_ui()
        self.show_all()

    # ── helpers ────────────────────────────────────────────────────────────────

    def _apply_global_css(self):
        css = b"""
        window { background-color: #1a1a1a; }
        entry  { background:#252525; color:#e0e0e0; border:1px solid #383838;
                 border-radius:5px; font-family:monospace; font-size:11px;
                 padding:3px 8px; caret-color:#7ab8f5; }
        entry:focus { border-color:#4d78aa; }
        """
        p = Gtk.CssProvider()
        p.load_from_data(css)
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(), p, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)

    def _css(self, widget, css):
        p = Gtk.CssProvider()
        p.load_from_data(css.encode())
        widget.get_style_context().add_provider(p, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)

    def _field(self, label_text, parent):
        row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=6)
        lbl = Gtk.Label(label=label_text)
        lbl.set_width_chars(4)
        lbl.set_xalign(0)
        self._css(lbl, "label { color:#555; font-family:monospace; font-size:9px; }")
        entry = Gtk.Entry()
        entry.set_width_chars(22)
        row.pack_start(lbl, False, False, 0)
        row.pack_start(entry, True, True, 0)
        parent.pack_start(row, False, False, 0)
        return entry

    # ── preview ────────────────────────────────────────────────────────────────

    def _draw_preview(self, widget, cr):
        alloc = widget.get_allocation()
        W, H = alloc.width, alloc.height
        r, g, b = hsv_to_rgb(self._hue, self._sat, self._val)

        R = 10
        cr.new_sub_path()
        cr.arc(R,   R,   R, math.pi,       3*math.pi/2)
        cr.arc(W-R, R,   R, 3*math.pi/2,   0)
        cr.arc(W-R, H-R, R, 0,             math.pi/2)
        cr.arc(R,   H-R, R, math.pi/2,     math.pi)
        cr.close_path()
        cr.set_source_rgb(r, g, b)
        cr.fill_preserve()
        cr.set_source_rgba(1, 1, 1, 0.07)
        cr.set_line_width(1.5)
        cr.stroke()

        hex_str = rgb_to_hex(r, g, b).upper()
        fg = 1.0 if luminance(r, g, b) < 0.5 else 0.0
        cr.set_source_rgba(fg, fg, fg, 0.9)
        cr.select_font_face("monospace", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)
        cr.set_font_size(10)
        ext = cr.text_extents(hex_str)
        cr.move_to((W - ext.width) / 2 - ext.x_bearing,
                   (H + ext.height) / 2)
        cr.show_text(hex_str)

    # ── callbacks ──────────────────────────────────────────────────────────────

    def _on_hue_change(self, hue):
        self._hue = hue
        self._sv.set_hue(hue)
        self._update_ui()

    def _on_sv_change(self, sat, val):
        self._sat = sat
        self._val = val
        self._update_ui()

    def _on_hex_entered(self, entry):
        rgb = hex_to_rgb(entry.get_text().strip())
        if rgb:
            r, g, b = rgb
            h, s, v = rgb_to_hsv(r, g, b)
            self._hue = h
            self._sat = s
            self._val = v
            self._hue_bar.set_hue(h)
            self._sv.set_hue(h)
            self._sv.set_sv(s, v)
            self._update_ui()

    def _update_ui(self):
        r, g, b = hsv_to_rgb(self._hue, self._sat, self._val)
        self._hex_entry.set_text(rgb_to_hex(r, g, b))
        self._rgb_entry.set_text(f"rgb({round(r*255)}, {round(g*255)}, {round(b*255)})")
        self._hsl_entry.set_text(rgb_to_hsl_str(r, g, b))
        self._preview.queue_draw()

    def _copy(self, text):
        cb = Gtk.Clipboard.get(Gdk.SELECTION_CLIPBOARD)
        cb.set_text(text, -1)
        cb.store()

    def _save_color(self, _=None):
        self._swatches.add_color(self._hex_entry.get_text())

    def _load_swatch(self, hex_color):
        rgb = hex_to_rgb(hex_color)
        if rgb:
            r, g, b = rgb
            h, s, v = rgb_to_hsv(r, g, b)
            self._hue = h
            self._sat = s
            self._val = v
            self._hue_bar.set_hue(h)
            self._sv.set_hue(h)
            self._sv.set_sv(s, v)
            self._update_ui()


# ── entry point ────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    ColorPickerWindow()
    Gtk.main()
