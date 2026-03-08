#!/usr/bin/env python3
# Author: Ioannes Cruxibulum
# pyra_shared.py — shared classes for all Pyra tools

import os
import random
import getpass
import time
import sys
import threading
import tty
import termios
from rich.tree import Tree
from rich import print
from rich.console import Console


# ─── Glitch character pools ───────────────────────────────────────────────────
GLITCH_CHARS = "!@#$%^&*<>?/|\\~`░▒▓█"
GHOST_WORDS  = ["pyra", "pyra_env", "pyra_bin", "pyra_tool", "pyra_lib"]

# ANSI — save/restore cursor (same technique as cyon_cli.c)
SAVE    = "\0337"       # save cursor position
RESTORE = "\0338"       # restore cursor position
UP1     = "\033[1A"     # move up 1 line
CR      = "\r"
CYAN    = "\033[31m"   # static prompt color (red)
PURPLE  = "\033[35m"
RED     = "\033[96m"   # glitch flash color (cyan)
RESET   = "\033[37m"

HIDE = "\033[?25l"
SHOW = "\033[?25h"


def _glitch_str(s: str) -> str:
    """Scatter 1–3 glitch chars into a copy of s."""
    b = list(s)
    for _ in range(random.randint(1, 3)):
        b[random.randint(0, len(b) - 1)] = random.choice(GLITCH_CHARS)
    return "".join(b)


def _ghost_str(s: str) -> str:
    """Stamp a ghost word + a couple char glitches into s."""
    b = list(s)
    word = random.choice(GHOST_WORDS)
    pos  = random.randint(0, max(0, len(b) - len(word)))
    for i, ch in enumerate(word):
        b[pos + i] = ch
    for _ in range(2):
        b[random.randint(0, len(b) - 1)] = random.choice(GLITCH_CHARS)
    return "".join(b)


def _glitch_loop(cwd: str, user: str, stop_event: threading.Event):
    """
    Mirrors cyon_cli.c glitch_fn:
      save cursor → jump up 1 line → overwrite ┌░cwd░ in place → restore cursor
    The input line on line 2 is never touched.
    """
    top = f" ┌░{cwd}░"

    while not stop_event.is_set():
        stop_event.wait(timeout=random.uniform(0.08, 0.30))
        if stop_event.is_set():
            break

        # choose style: 25% ghost word (red), 75% char scatter (purple)
        if random.random() < 0.25:
            glitched, color = _ghost_str(top), RED
        else:
            glitched, color = _glitch_str(top), PURPLE

        # overwrite top line, restore cursor to input position
        sys.stdout.write(f"{SAVE}{UP1}{CR}{color}{glitched}{RESET}{RESTORE}")
        sys.stdout.flush()

        # hold glitch 60 ms then restore clean line
        stop_event.wait(timeout=0.06)
        if stop_event.is_set():
            break

        sys.stdout.write(f"{SAVE}{UP1}{CR}{CYAN}{top}{RESET}{RESTORE}")
        sys.stdout.flush()


def _build_prompt(cwd: str, user: str) -> str:
    return f"{CYAN} ┌░{RESET}{cwd}{CYAN}░\n └░{RESET}{user}{CYAN}░{RESET} "


def _read_line_raw() -> str:
    """Read a line in raw mode, echoing chars ourselves."""
    fd  = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    chars = []
    try:
        tty.setraw(fd)
        while True:
            ch = sys.stdin.read(1)
            if ch in ("\r", "\n"):
                sys.stdout.write("\r\n")
                sys.stdout.flush()
                break
            elif ch in ("\x7f", "\x08"):
                if chars:
                    chars.pop()
                    sys.stdout.write("\b \b")
                    sys.stdout.flush()
            elif ch == "\x03":
                raise KeyboardInterrupt
            else:
                chars.append(ch)
                sys.stdout.write(ch)
                sys.stdout.flush()
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)
    return "".join(chars)


class Input:
    @staticmethod
    def _prompt_with_glitch() -> str:
        user   = getpass.getuser()
        cwd    = os.getcwd()

        # Print the static two-line prompt once
        sys.stdout.write(HIDE)
        sys.stdout.write(_build_prompt(cwd, user))
        sys.stdout.flush()

        stop_event = threading.Event()
        t = threading.Thread(
            target=_glitch_loop,
            args=(cwd, user, stop_event),
            daemon=True,
        )
        t.start()

        try:
            line = _read_line_raw()
        finally:
            stop_event.set()
            t.join(timeout=0.35)
            # restore top line to clean state
            sys.stdout.write(f"{SAVE}{UP1}{CR}{CYAN} ┌░{RESET}{cwd}{CYAN}░{RESET}{RESTORE}")
            sys.stdout.write(SHOW)
            sys.stdout.flush()

        return line

    @staticmethod
    def get_string_input() -> str:
        return Input._prompt_with_glitch()

    @staticmethod
    def get_integer_input() -> int:
        return int(Input._prompt_with_glitch())

    @staticmethod
    def get_float_input() -> float:
        return float(Input._prompt_with_glitch())


# ─── ASCII Logo ───────────────────────────────────────────────────────────────


class main_logo:
    @staticmethod
    def logo():
        color           = random.choice(["red1", "purple"])
        pc = "Ioannes Cruxibulum"
        pn = "PYRA TOOLZ"
        pv = "v3.0"

        logo1 = f"""[{color}]
 ╔══════════════════════════════════════╗
 ║  ██▀▄ ▀▄ ██▀█  ▄▀█                  ║
 ║  ██▀▀ ▀▄ ██▀▄  █▀█  {pv}           ║
 ╠══════════════════════════════════════╣
 ║  ░▒▓ {pn}  ·  {pc} ▓▒░  ║
 ╚══════════════════════════════════════╝
[/{color}]"""

        logo2 = f"""[{color}]
 ▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄
 ░  ██▀▄ ▀▄ ██▀█  ▄▀█                  ░
 ░  ██▀▀ ▀▄ ██▀▄  █▀█  {pv}           ░
 ░░ {pn}  ·  {pc} ░░
 ▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀
[/{color}]"""

        logo3 = f"""[{color}]
 ╔╦╦╦╦╦╦╦╦╦╦╦╦╦╦╦╦╦╦╦╦╦╦╦╦╦╦╦╦╦╦╦╦╦╦╦╦╦╗
 ╠╣  ██▀▄ ▀▄ ██▀█  ▄▀█  ▲             ╠╣
 ╠╣  ██▀▀ ▀▄ ██▀▄  █▀█ ███  {pv}      ╠╣
 ╠╣  {pn}  ·  {pc}   ╠╣
 ╚╩╩╩╩╩╩╩╩╩╩╩╩╩╩╩╩╩╩╩╩╩╩╩╩╩╩╩╩╩╩╩╩╩╩╩╩╩╝
[/{color}]"""

        logo4 = f"""[{color}]
 ┌─[BOOT]──────────────────────────────┐
 │  ██▀▄ ▀▄ ██▀█  ▄▀█                 │
 │  ██▀▀ ▀▄ ██▀▄  █▀█  {pv}          │
 │  > {pn}  ·  {pc}  │
 │  > STATUS :: ░░░░░░░░ [ARMED]      │
 └─────────────────────────────────────┘
[/{color}]"""

        print(random.choice([logo1, logo2, logo3, logo4]))


class HonerableMentions:
    save_where = " [white]Save on Desktop Videos Music Downloads?[/white]"
    save_where_termux = " [white]Save on:\n Desktop\n Videos\n droid movies\n droid music\n droid download[/white]"
    save_audio_where = " [white]Save on Desktop Music?[/white]"
    old_filename = " [white]Filename?[/white]"
    new_filename = " [white]New filename?[/white]"
    exit_program = " [white]Exiting the program...[/white]"
