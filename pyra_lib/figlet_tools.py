#!/usr/bin/env python3
# Author: Ioannes Cruxibulum
# figlet_tools.py — part of pyra_lib
import sys
from pathlib import Path

PYRA_ENV = Path.home() / "pyra_env"
PYRA_LIB = Path.home() / "cyon" / "pyra_lib"

site_pkgs = list(PYRA_ENV.glob("lib/python3*/site-packages"))
if site_pkgs:
    sys.path.insert(0, str(site_pkgs[0]))
sys.path.append(str(PYRA_LIB))
from pyra_shared import Input, main_logo, HonerableMentions
import pyfiglet


class FigletTools:
    @staticmethod
    def list_fonts():
        fonts = pyfiglet.FigletFont.getFonts()
        for font in fonts:
            print(f"  {font}")
        print()

    @staticmethod
    def make_font(text, style):
        try:
            result = pyfiglet.figlet_format(text, font=style)
            print(result)
        except pyfiglet.FontNotFound:
            print(f" Font not found: {style}")


def run():
    main_logo()
    while True:
        print(" commands:\n list fonts\n make font\n exit")
        choice = Input.get_string_input()
        if choice == "list fonts":
            FigletTools.list_fonts()
        elif choice == "make font":
            print("\n Enter a string:")
            text = Input.get_string_input()
            if text in ("exit", ""):
                break
            print("\n Enter a font name: (try: cybermedium, slant, banner3)")
            style = Input.get_string_input()
            if style in ("exit", ""):
                break
            FigletTools.make_font(text, style)
        elif choice == "exit":
            break
        else:
            print(" Invalid command")

    HonerableMentions()


if __name__ == "__main__":
    run()
