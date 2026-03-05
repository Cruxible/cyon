#!/usr/bin/env python3
# Author: Ioannes Cruxibulum
# pyra_player.py — part of pyra_lib

import os
import sys
import threading
import subprocess
from pathlib import Path

import gi
gi.require_version("Gtk", "3.0")
gi.require_version("Gdk", "3.0")
from gi.repository import Gtk, Gdk, GLib

import random
import math

# ── optional GStreamer ────────────────────────────────────────────────────────
try:
    gi.require_version("Gst", "1.0")
    from gi.repository import Gst
    Gst.init(None)
    USE_GST = True
except Exception:
    USE_GST = False

MUSIC_DIR = Path.home() / "Music"
SUPPORTED  = {".mp3", ".flac", ".ogg", ".wav", ".m4a", ".opus", ".aac"}

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
.track-label {
    color: #00ff99;
    font-family: monospace;
    font-size: 12px;
}
.dim-label {
    color: #336655;
    font-family: monospace;
    font-size: 10px;
}
.status-label {
    color: #336655;
    font-family: monospace;
    font-size: 10px;
}
.status-ok  { color: #00ff99; }
.status-err { color: #ff3355; }
entry {
    background-color: #0d0d15;
    color: #00ff99;
    font-family: monospace;
    font-size: 12px;
    border: 1px solid #1a2a20;
    border-radius: 0px;
    padding: 4px;
}
entry:focus { border-color: #00ff99; }
button {
    background-color: #0d0d15;
    color: #00cc77;
    font-family: monospace;
    font-size: 12px;
    border: 1px solid #1a2a20;
    border-radius: 0px;
    padding: 6px 10px;
}
button:hover {
    background-color: #003322;
    color: #00ff99;
    border-color: #00ff99;
}
treeview {
    background-color: #05050a;
    color: #00cc77;
    font-family: monospace;
    font-size: 11px;
}
treeview:selected {
    background-color: #003322;
    color: #00ff99;
}
scrolledwindow { border: 1px solid #1a2a20; }
separator      { background-color: #1a2a20; }
.marquee-label {
    color: #00ff99;
    font-family: monospace;
    font-size: 12px;
    letter-spacing: 1px;
    background-color: #05050a;
    padding: 2px 4px;
}

scale trough {
    background-color: #0d0d15;
    border: 1px solid #1a2a20;
    min-height: 4px;
}
scale highlight {
    background-color: #00cc77;
    min-height: 4px;
}
scale slider {
    background-color: #00ff99;
    min-width: 10px;
    min-height: 10px;
    border-radius: 0px;
    border: none;
}
"""


# ── Marquee glitch strings ────────────────────────────────────────────────────
GLITCH_MSGS = [
    "▸ █▓░ SIGNAL LOST ░▓█ ◈ RE-ROUTING...",
    "▸ [0x4E554C4C] NULL PTR DETECTED — VIBES UNDEFINED",
    "▸ ▒▒▒ BUFFER OVERFLOW IN SECTOR 7G ▒▒▒",
    "▸ ░░░ D̷A̸T̵A̶ ̴C̷O̸R̵R̶U̷P̸T̵ ░░░ CHECKSUM FAIL",
    "▸ ████ SEEK ERROR ████ RETRYING FROM VOID...",
    "▸ [ERR::OOB] INDEX -1 OUT OF BOUNDS — PLAYING ANYWAY",
    "▸ ▓▓▒▒░░ ENTROPY SPIKE DETECTED ░░▒▒▓▓",
    "▸ ☠ SEGFAULT IN AUDIO.DLL — PROCEEDING RECKLESSLY",
    "▸ 01001110 01001111 00100000 01010011 01001001 01000111",
    "▸ ◈◈◈ FRAME DROP IMMINENT ◈◈◈ CATCH IT IF YOU CAN",
    "▸ [WARN] REALITY.EXE HAS STOPPED RESPONDING",
    "▸ ░ ▒ ▓ █ DECODING WAVEFORM █ ▓ ▒ ░",
    "▸ >>> MEMORY LEAK IN SECTOR [????] — DO NOT LOOK",
    "▸ ▞▚▞▚ CARRIER WAVE UNSTABLE ▞▚▞▚",
    "▸ [0xDEADBEEF] MAGIC NUMBER FOUND — LUCKY YOU",
    "▸ ▸▸▸ STACK TRACE: SOMEWHERE > NOWHERE > HERE",
    "▸ ██░░ SIGNAL: ████ NOISE: ░░██ RATIO: ¿?",
    "▸ ◀◀ REWINDING TIME... FAILED. PLAYING FORWARD.",
    "▸ [SYS] CLOCK SKEW +∞ms — TIME IS A SUGGESTION",
    "▸ ▓░▓░ INTERLEAVE ERROR ░▓░▓ BITS REARRANGED",
    "▸ ☢ RADIOACTIVE SAMPLE RATE DETECTED: 666hz",
    "▸ [OOB] TRACK INDEX ESCAPED ARRAY — STILL VIBING",
    "▸ ░░░░ LATENCY: ████ms ░░░░ SOURCE: UNKNOWN",
    "▸ ▸ KERNEL PANIC AVERTED... THIS TIME.",
    "▸ [!!] UNEXPECTED TOKEN '♪' IN EXPRESSION",
    "▸ ████████░░░░░░░░ LOADING SILENCE... DONE",
    "▸ ◈ AUDIO THREAD DESYNC — SOUL.WAV OUT OF SYNC",
    "▸ 𝙽𝚄𝙻𝙻 𝚁𝙴𝙵𝙴𝚁𝙴𝙽𝙲𝙴 𝙴𝚇𝙲𝙴𝙿𝚃𝙸𝙾𝙽 𝙸𝙽 𝙼𝚄𝚂𝙸𝙲.𝙲𝙾𝚁𝙴",
    "▸ [DEPRECATED] USE feeling() INSTEAD OF thinking()",
    "▸ ▒▒ WAVEFORM: ╔═╗╚═╝╔╗╚╝╔═╗ UNREADABLE ▒▒",
]

MARQUEE_WIDTH = 52

# ── helpers ───────────────────────────────────────────────────────────────────

def fmt_time(secs):
    if secs < 0:
        return "--:--"
    m, s = divmod(int(secs), 60)
    return f"{m:02d}:{s:02d}"


def scan_music(root: Path):
    tracks = []
    for p in sorted(root.rglob("*")):
        if p.suffix.lower() in SUPPORTED:
            tracks.append(p)
    return tracks


# ── GStreamer player backend ──────────────────────────────────────────────────

class GstPlayer:
    def __init__(self, on_eos, on_tick):
        self.on_eos   = on_eos
        self.on_tick  = on_tick
        self._pipe    = Gst.parse_launch("playbin")
        bus = self._pipe.get_bus()
        bus.add_signal_watch()
        bus.connect("message::eos",   self._eos)
        bus.connect("message::error", self._err)
        GLib.timeout_add(500, self._tick)

    def play(self, path):
        self._pipe.set_state(Gst.State.NULL)
        self._pipe.set_property("uri", Path(path).as_uri())
        self._pipe.set_state(Gst.State.PLAYING)

    def pause(self):
        self._pipe.set_state(Gst.State.PAUSED)

    def resume(self):
        self._pipe.set_state(Gst.State.PLAYING)

    def stop(self):
        self._pipe.set_state(Gst.State.NULL)

    def seek(self, secs):
        self._pipe.seek_simple(
            Gst.Format.TIME,
            Gst.SeekFlags.FLUSH | Gst.SeekFlags.KEY_UNIT,
            int(secs * Gst.SECOND),
        )

    def get_position(self):
        ok, pos = self._pipe.query_position(Gst.Format.TIME)
        return pos / Gst.SECOND if ok else 0.0

    def get_duration(self):
        ok, dur = self._pipe.query_duration(Gst.Format.TIME)
        return dur / Gst.SECOND if ok else 0.0

    def set_volume(self, v):
        self._pipe.set_property("volume", max(0.0, min(1.0, v)))

    def _tick(self):
        GLib.idle_add(self.on_tick)
        return True

    def _eos(self, bus, msg):
        GLib.idle_add(self.on_eos)

    def _err(self, bus, msg):
        err, _ = msg.parse_error()
        GLib.idle_add(self.on_eos)


# ── fallback mpv / ffplay backend ────────────────────────────────────────────

class ProcPlayer:
    """Thin wrapper around mpv (or ffplay) for when GStreamer is unavailable."""

    def __init__(self, on_eos, on_tick):
        self.on_eos  = on_eos
        self.on_tick = on_tick
        self._proc   = None
        self._pos    = 0.0
        self._dur    = 0.0
        self._playing = False
        GLib.timeout_add(500, self._tick)

    def _player_cmd(self, path):
        for cmd in ("mpv", "ffplay"):
            if subprocess.run(["which", cmd], capture_output=True).returncode == 0:
                if cmd == "mpv":
                    return ["mpv", "--no-video", "--really-quiet", str(path)]
                else:
                    return ["ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet", str(path)]
        return None

    def play(self, path):
        self.stop()
        cmd = self._player_cmd(path)
        if cmd is None:
            GLib.idle_add(self.on_eos)
            return
        self._pos = 0.0
        self._playing = True
        self._proc = subprocess.Popen(cmd)
        threading.Thread(target=self._wait, daemon=True).start()

    def _wait(self):
        if self._proc:
            self._proc.wait()
            self._playing = False
            GLib.idle_add(self.on_eos)

    def pause(self):  pass
    def resume(self): pass

    def stop(self):
        if self._proc and self._proc.poll() is None:
            self._proc.terminate()
        self._proc = None
        self._playing = False

    def seek(self, secs): pass

    def get_position(self):
        if self._playing:
            self._pos += 0.5
        return self._pos

    def get_duration(self):
        return self._dur

    def set_volume(self, v): pass

    def _tick(self):
        GLib.idle_add(self.on_tick)
        return True


# ── Cairo particle visualizer ────────────────────────────────────────────────

class Particle:
    """A single drifting dot with its own velocity, size, and fade life."""
    CYANS = [
        (0.0,  1.0,  1.0),   # #00ffff
        (0.0,  0.9,  1.0),   # #00e5ff
        (0.0,  1.0,  0.8),   # #00ffcc
        (0.2,  1.0,  1.0),   # #33ffff
        (0.25, 0.88, 0.82),  # #40e0d0
        (0.0,  0.8,  0.8),   # #00cccc
        (0.4,  1.0,  1.0),   # #66ffff
        (0.0,  0.7,  0.82),  # #00b3d1
    ]

    def __init__(self, w, h):
        self.w = w
        self.h = h
        self._respawn()

    def _respawn(self):
        self.x    = random.uniform(0, self.w)
        self.y    = random.uniform(0, self.h)
        self.vx   = random.uniform(-0.6, 0.6)
        self.vy   = random.uniform(-0.6, 0.6)
        self.r    = random.uniform(1.5, 4.5)
        self.life = random.uniform(0.4, 1.0)   # current alpha
        self.fade = random.uniform(0.003, 0.009)
        self.color = random.choice(self.CYANS)

    def step(self):
        self.x    += self.vx
        self.y    += self.vy
        self.life -= self.fade
        # wrap edges
        if self.x < 0:   self.x = self.w
        if self.x > self.w: self.x = 0
        if self.y < 0:   self.y = self.h
        if self.y > self.h: self.y = 0
        if self.life <= 0:
            self._respawn()

    def draw(self, cr):
        r, g, b = self.color
        cr.set_source_rgba(r, g, b, self.life)
        cr.arc(self.x, self.y, self.r, 0, 2 * math.pi)
        cr.fill()
        # soft glow halo
        cr.set_source_rgba(r, g, b, self.life * 0.15)
        cr.arc(self.x, self.y, self.r * 3.0, 0, 2 * math.pi)
        cr.fill()


class ParticleViz:
    """
    A GTK window containing a Cairo DrawingArea with drifting cyan particles.
    Call show() on play, hide() on pause / stop.
    """
    W, H    = 500, 500
    N_PARTS = 120       # number of particles

    def __init__(self):
        self._win      = None
        self._area     = None
        self._particles = []
        self._timer_id  = None
        self._active    = False

    # ── public ────────────────────────────────────────────────────────────────

    def show(self):
        if self._active:
            return
        self._active = True
        self._build_window()

    def hide(self):
        if not self._active:
            return
        self._active = False
        if self._timer_id is not None:
            GLib.source_remove(self._timer_id)
            self._timer_id = None
        if self._win:
            self._win.destroy()
            self._win  = None
            self._area = None
        self._particles = []

    # ── internal ──────────────────────────────────────────────────────────────

    def _build_window(self):
        self._win = Gtk.Window(title="PYRA // VIZ")
        self._win.set_default_size(self.W, self.H)
        self._win.set_resizable(False)
        self._win.connect("delete-event", self._on_close)

        # force black background on the window itself
        self._win.override_background_color(
            Gtk.StateFlags.NORMAL, Gdk.RGBA(0, 0, 0, 1))

        self._area = Gtk.DrawingArea()
        self._area.set_size_request(self.W, self.H)
        self._area.connect("draw", self._on_draw)
        self._win.add(self._area)

        # spawn particles
        self._particles = [Particle(self.W, self.H) for _ in range(self.N_PARTS)]

        self._win.show_all()

        # ~60 fps tick
        self._timer_id = GLib.timeout_add(16, self._tick)

    def _on_close(self, win, event):
        self.hide()
        return True   # suppress destroy so hide() controls it cleanly

    def _tick(self):
        if not self._active:
            return False
        for p in self._particles:
            p.step()
        if self._area:
            self._area.queue_draw()
        return True

    def _on_draw(self, area, cr):
        # black background
        cr.set_source_rgb(0, 0, 0)
        cr.paint()

        # very faint grid for that terminal / oscilloscope feel
        cr.set_source_rgba(0.0, 0.3, 0.3, 0.08)
        cr.set_line_width(0.5)
        step = 40
        for x in range(0, self.W, step):
            cr.move_to(x, 0); cr.line_to(x, self.H); cr.stroke()
        for y in range(0, self.H, step):
            cr.move_to(0, y); cr.line_to(self.W, y); cr.stroke()

        # draw every particle
        for p in self._particles:
            p.draw(cr)


# ── main window ──────────────────────────────────────────────────────────────

class PlayerWindow(Gtk.ApplicationWindow):
    def __init__(self, app, treeview_window=None):
        super().__init__(application=app, title="CYON // PLAYER")
        self.set_default_size(560, 560)
        self.set_border_width(0)
        self.treeview_window = treeview_window
        self.connect("delete-event", self._on_delete)

        provider = Gtk.CssProvider()
        provider.load_from_data(CSS)
        Gtk.StyleContext.add_provider_for_screen(
            self.get_screen(), provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )

        self.tracks    = []
        self.current   = -1
        self.playing   = False
        self._seeking  = False
        self._viz      = ParticleViz()   # Cairo particle visualizer

        # marquee state
        self._mq_buf    = ""
        self._mq_offset = 0
        self._mq_idx    = 0
        self._load_next_glitch()
        GLib.timeout_add(80, self._tick_marquee)

        if USE_GST:
            self.player = GstPlayer(self._on_eos, self._on_tick)
        else:
            self.player = ProcPlayer(self._on_eos, self._on_tick)

        self._build_ui()
        self._load_library()
        self.show_all()

    def _build_ui(self):
        outer = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        outer.set_margin_top(16)
        outer.set_margin_bottom(12)
        outer.set_margin_start(16)
        outer.set_margin_end(16)
        self.add(outer)

        title = Gtk.Label(label="▸ PLAYER // MUSIC LIBRARY")
        title.get_style_context().add_class("title-label")
        title.set_halign(Gtk.Align.START)
        outer.pack_start(title, False, False, 0)

        outer.pack_start(Gtk.Separator(), False, False, 8)

        self.now_label = Gtk.Label(label="▸ NO TRACK LOADED")
        self.now_label.get_style_context().add_class("track-label")
        self.now_label.set_halign(Gtk.Align.START)
        self.now_label.set_ellipsize(3)
        outer.pack_start(self.now_label, False, False, 2)

        self.dir_label = Gtk.Label(label=str(MUSIC_DIR))
        self.dir_label.get_style_context().add_class("dim-label")
        self.dir_label.set_halign(Gtk.Align.START)
        self.dir_label.set_ellipsize(3)
        outer.pack_start(self.dir_label, False, False, 0)

        # ── glitch marquee ────────────────────────────────────────────────
        self.marquee_label = Gtk.Label(label="")
        self.marquee_label.get_style_context().add_class("marquee-label")
        self.marquee_label.set_halign(Gtk.Align.FILL)
        self.marquee_label.set_selectable(False)
        outer.pack_start(self.marquee_label, False, False, 2)
        # ─────────────────────────────────────────────────────────────────

        outer.pack_start(Gtk.Separator(), False, False, 6)

        # seek bar
        seek_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        outer.pack_start(seek_box, False, False, 2)

        self.time_label = Gtk.Label(label="00:00")
        self.time_label.get_style_context().add_class("dim-label")
        seek_box.pack_start(self.time_label, False, False, 0)

        self.seek_bar = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 0, 100, 1)
        self.seek_bar.set_draw_value(False)
        self.seek_bar.set_hexpand(True)
        self.seek_bar.connect("button-press-event",   self._seek_start)
        self.seek_bar.connect("button-release-event", self._seek_end)
        seek_box.pack_start(self.seek_bar, True, True, 0)

        self.dur_label = Gtk.Label(label="00:00")
        self.dur_label.get_style_context().add_class("dim-label")
        seek_box.pack_start(self.dur_label, False, False, 0)

        # transport
        ctrl_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        outer.pack_start(ctrl_box, False, False, 4)

        btn_prev = Gtk.Button(label="◀◀ PREV")
        btn_prev.connect("clicked", lambda _: self._prev())
        ctrl_box.pack_start(btn_prev, True, True, 0)

        self.btn_play = Gtk.Button(label="▶ PLAY")
        self.btn_play.connect("clicked", self._toggle_play)
        ctrl_box.pack_start(self.btn_play, True, True, 0)

        btn_next = Gtk.Button(label="NEXT ▶▶")
        btn_next.connect("clicked", lambda _: self._next())
        ctrl_box.pack_start(btn_next, True, True, 0)

        btn_stop = Gtk.Button(label="■ STOP")
        btn_stop.connect("clicked", lambda _: self._stop())
        ctrl_box.pack_start(btn_stop, True, True, 0)

        # volume
        vol_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        outer.pack_start(vol_box, False, False, 2)

        vol_lbl = Gtk.Label(label="VOL")
        vol_lbl.get_style_context().add_class("dim-label")
        vol_box.pack_start(vol_lbl, False, False, 0)

        self.vol_bar = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 0, 100, 1)
        self.vol_bar.set_draw_value(False)
        self.vol_bar.set_value(80)
        self.vol_bar.set_hexpand(True)
        self.vol_bar.connect("value-changed", self._on_volume)
        vol_box.pack_start(self.vol_bar, True, True, 0)

        self.vol_pct = Gtk.Label(label="80%")
        self.vol_pct.get_style_context().add_class("dim-label")
        vol_box.pack_start(self.vol_pct, False, False, 0)

        outer.pack_start(Gtk.Separator(), False, False, 6)

        # search
        search_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        outer.pack_start(search_box, False, False, 2)

        self.search_entry = Gtk.Entry()
        self.search_entry.set_placeholder_text("Filter tracks...")
        self.search_entry.connect("changed", self._on_search)
        search_box.pack_start(self.search_entry, True, True, 0)

        btn_reload = Gtk.Button(label="↺ RELOAD")
        btn_reload.connect("clicked", lambda _: self._load_library())
        search_box.pack_start(btn_reload, False, False, 0)

        # track list
        scroll = Gtk.ScrolledWindow()
        scroll.set_min_content_height(200)
        scroll.set_vexpand(True)

        self.store  = Gtk.ListStore(str, str)
        self.filter = self.store.filter_new()
        self.filter.set_visible_func(self._track_visible)

        self.tree = Gtk.TreeView(model=self.filter)
        self.tree.set_headers_visible(False)
        self.tree.connect("row-activated", self._on_row_activated)
        self.tree.append_column(
            Gtk.TreeViewColumn("Track", Gtk.CellRendererText(), text=0))

        scroll.add(self.tree)
        outer.pack_start(scroll, True, True, 0)

        self.status = Gtk.Label(label="▸ Ready.")
        self.status.get_style_context().add_class("status-label")
        self.status.set_halign(Gtk.Align.START)
        outer.pack_start(self.status, False, False, 4)

    # ── library ───────────────────────────────────────────────────────────────

    def _load_library(self):
        self.store.clear()
        self.tracks = scan_music(MUSIC_DIR)
        for p in self.tracks:
            try:
                rel = p.relative_to(MUSIC_DIR)
            except ValueError:
                rel = p
            self.store.append([str(rel), str(p)])
        self._set_status(f"▸ {len(self.tracks)} tracks found in {MUSIC_DIR}")

    # ── playback ──────────────────────────────────────────────────────────────

    def _play_index(self, idx):
        if idx < 0 or idx >= len(self.tracks):
            return
        self.current = idx
        path = self.tracks[idx]
        self.now_label.set_text(f"▸ {path.name}")
        self.player.play(str(path))
        self.playing = True
        self.btn_play.set_label("⏸ PAUSE")
        self._set_status(f"▸ Playing: {path.name}")
        self._highlight_row(idx)
        self._viz.show()

    def _toggle_play(self, *_):
        if not self.playing:
            if self.current == -1 and self.tracks:
                self._play_index(0)
            else:
                self.player.resume()
                self.playing = True
                self.btn_play.set_label("⏸ PAUSE")
                self._viz.show()
        else:
            self.player.pause()
            self.playing = False
            self.btn_play.set_label("▶ PLAY")
            self._viz.hide()

    def _stop(self):
        self.player.stop()
        self.playing = False
        self.btn_play.set_label("▶ PLAY")
        self.seek_bar.set_value(0)
        self.time_label.set_text("00:00")
        self._set_status("▸ Stopped.")
        self._viz.hide()

    def _next(self):
        if self.current + 1 < len(self.tracks):
            self._play_index(self.current + 1)

    def _prev(self):
        if self.current > 0:
            self._play_index(self.current - 1)

    def _on_eos(self):
        self._next()

    # ── tick / seek ───────────────────────────────────────────────────────────

    def _on_tick(self):
        if self._seeking or not self.playing:
            return
        pos = self.player.get_position()
        dur = self.player.get_duration()
        self.time_label.set_text(fmt_time(pos))
        self.dur_label.set_text(fmt_time(dur))
        if dur > 0:
            self.seek_bar.set_range(0, dur)
            self.seek_bar.set_value(pos)

    def _seek_start(self, *_):
        self._seeking = True

    def _seek_end(self, widget, event):
        self.player.seek(widget.get_value())
        self._seeking = False

    # ── volume ────────────────────────────────────────────────────────────────

    def _on_volume(self, scale):
        v = scale.get_value() / 100.0
        self.player.set_volume(v)
        self.vol_pct.set_text(f"{int(v*100)}%")

    # ── search ────────────────────────────────────────────────────────────────

    def _on_search(self, entry):
        self.filter.refilter()

    def _track_visible(self, model, it, data):
        q = self.search_entry.get_text().lower()
        return (not q) or (q in model[it][0].lower())

    # ── treeview ──────────────────────────────────────────────────────────────

    def _on_row_activated(self, tree, path, col):
        model = tree.get_model()
        it    = model.get_iter(path)
        full  = model[it][1]
        idx   = next((i for i, t in enumerate(self.tracks) if str(t) == full), -1)
        if idx != -1:
            self._play_index(idx)

    def _highlight_row(self, idx):
        path_str = str(self.tracks[idx])
        for i, row in enumerate(self.filter):
            if row[1] == path_str:
                tp = Gtk.TreePath.new_from_indices([i])
                self.tree.get_selection().select_path(tp)
                self.tree.scroll_to_cell(tp, None, False, 0, 0)
                break

    # ── glitch marquee ────────────────────────────────────────────────────────

    def _load_next_glitch(self):
        import random
        self._mq_idx = random.randint(0, len(GLITCH_MSGS) - 1)
        msg = GLITCH_MSGS[self._mq_idx]
        self._mq_buf    = " " * MARQUEE_WIDTH + msg
        self._mq_offset = 0

    def _tick_marquee(self):
        if not self.marquee_label or not self.marquee_label.get_realized():
            return True
        buf = self._mq_buf
        total = len(buf)
        if self._mq_offset >= total:
            self._load_next_glitch()
            return True
        end = self._mq_offset + MARQUEE_WIDTH
        view = buf[self._mq_offset:end]
        view = view.ljust(MARQUEE_WIDTH)
        self.marquee_label.set_text(view)
        self._mq_offset += 1
        return True

    # ── misc ──────────────────────────────────────────────────────────────────

    def _set_status(self, msg):
        self.status.set_text(msg)

    def _on_delete(self, widget, event):
        self._viz.hide()
        self.player.stop()
        self.destroy()
        if self.treeview_window is not None:
            self.treeview_window.show_all()
        return False


# ── application ───────────────────────────────────────────────────────────────

class PyraPlayerApp(Gtk.Application):
    def __init__(self, treeview_window=None):
        super().__init__(application_id="com.pyra.player")
        self.treeview_window = treeview_window
        self.connect("activate", self.on_activate)

    def on_activate(self, app):
        PlayerWindow(app, treeview_window=self.treeview_window)


if __name__ == "__main__":
    app = PyraPlayerApp()
    app.run()
