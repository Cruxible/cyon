#!/usr/bin/env python3
# Author: Ioannes Cruxibulum
# image_converter.py — part of pyra_lib

import os
import sys
import random
from PIL import Image
from pathlib import Path
from rich import print
from rich.tree import Tree

PYRA_ENV = Path.home() / "pyra_env"
PYRA_LIB = Path.home() / "cyon" / "pyra_lib"

site_pkgs = list(PYRA_ENV.glob("lib/python3*/site-packages"))
if site_pkgs:
    sys.path.insert(0, str(site_pkgs[0]))
sys.path.append(str(PYRA_LIB))
from pyra_shared import Input, main_logo, HonerableMentions


class MySexyVariables:
    color = random.choice(["red1", "purple"])
    in_formats = ["jpg", "png", "webp", "exit"]
    out_formats = ["jpg", "png", "webp", "exit"]


class calls:
    @staticmethod
    def input_format_list():
        tree = Tree(
            "[white] What format is your image?", guide_style=f"{MySexyVariables.color}"
        )
        for i in MySexyVariables.in_formats:
            tree.add("[white]" + str(i))
        print(" ", tree)

    @staticmethod
    def output_format_list(exclude):
        tree = Tree("[white] Convert to?", guide_style=f"{MySexyVariables.color}")
        for i in MySexyVariables.out_formats:
            if i != exclude:
                tree.add("[white]" + str(i))
        print(" ", tree)


class ImageConverter:
    @staticmethod
    def convert_image(input_file, output_format):
        input_file = os.path.expanduser(input_file)
        if not os.path.isfile(input_file):
            print(f" Error: '{input_file}' not found.")
            return
        try:
            with Image.open(input_file) as img:
                output_file = (
                    f"{os.path.splitext(input_file)[0]}.{output_format.lower()}"
                )
                if img.mode in ("RGBA", "P"):
                    img = img.convert("RGB")
                fmt = (
                    "JPEG" if output_format.upper() == "JPG" else output_format.upper()
                )
                img.save(output_file, fmt)
                print(
                    f" Converted to {output_format.upper()} — saved as '{output_file}'."
                )
                with Image.open(output_file) as converted_img:
                    print(f" Verified format: {converted_img.format}")
        except Exception as e:
            print(f" Conversion failed: {e}")


def run():
    main_logo.logo()
    while True:
        calls.input_format_list()
        print(" Type the format of the image you want to convert (or 'exit' to quit):")
        input_format = Input.get_string_input().strip().lower()

        if input_format == "exit":
            break
        elif input_format not in {"jpg", "png", "webp"}:
            print(" Invalid format. Choose jpg, png, or webp.")
            continue

        print(f" Enter the full path to your {input_format.upper()} file (~ is fine):")
        input_file = Input.get_string_input().strip()
        if input_file == "exit":
            break

        calls.output_format_list(exclude=input_format)
        print(f" Type the format you want to convert to (not {input_format.upper()}):")
        output_format = Input.get_string_input().strip().lower()

        if output_format == "exit":
            break
        elif output_format == input_format:
            print(
                f" Can't convert {input_format.upper()} to {input_format.upper()}. Pick a different format."
            )
            continue
        elif output_format not in {"jpg", "png", "webp"}:
            print(" Invalid format. Choose jpg, png, or webp.")
            continue

        ImageConverter.convert_image(input_file, output_format)


if __name__ == "__main__":
    run()
