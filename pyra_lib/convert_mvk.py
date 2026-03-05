#!/usr/bin/env python3
# Author: Ioannes Cruxibulum
# mkv_converter.py — part of pyra_lib
import os
import sys
from pathlib import Path

PYRA_ENV = Path.home() / "pyra_env"
PYRA_LIB = Path.home() / "cyon" / "pyra_lib"

site_pkgs = list(PYRA_ENV.glob("lib/python3*/site-packages"))
if site_pkgs:
    sys.path.insert(0, str(site_pkgs[0]))
sys.path.append(str(PYRA_LIB))
from pyra_shared import Input, main_logo, HonerableMentions
from moviepy.editor import VideoFileClip


class MkvConverter:
    @staticmethod
    def mkv_to_mp4(source_file, destination):
        source_path = Path(source_file)
        if not source_path.exists():
            print(f" File not found: {source_file}")
            return
        if source_path.suffix.lower() != ".mkv":
            print(f" File is not an MKV: {source_file}")
            return
        output_filename = source_path.stem + ".mp4"
        dest_path = Path(destination) / output_filename
        try:
            print(f" Converting {source_path.name} to MP4...")
            clip = VideoFileClip(str(source_path))
            clip.write_videofile(str(dest_path), codec="libx264", audio_codec="aac")
            clip.close()
            print(f" Converted file saved to {dest_path}")
        except Exception as e:
            print(f" Conversion failed: {e}")


def run():
    main_logo()
    # destination changes depending on Linux vs Termux
    if Path("/sdcard/Download").exists():
        destination = "/sdcard/Download"
    else:
        destination = str(Path.home() / "Downloads")

    while True:
        print(" commands:\n convert mkv\n exit")
        choice = Input.get_string_input()
        if choice == "convert mkv":
            print("\n Path to MKV file? (Example: /home/ioannes/Videos/movie.mkv)")
            source_file = Input.get_string_input()
            if source_file in ("exit", ""):
                break
            MkvConverter.mkv_to_mp4(source_file, destination)
        elif choice == "exit":
            break
        else:
            print(" Invalid command")

    HonerableMentions()


if __name__ == "__main__":
    run()
