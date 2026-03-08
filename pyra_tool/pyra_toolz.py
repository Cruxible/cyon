#!/usr/bin/env python3
# Author: Ioannes Cruxibulum
# Sep.10th 2023
# sudo apt install yt-dlp

import platform
import os
import sys
import random
import subprocess
import shutil
from pathlib import Path

sys.path.append(str(Path.home() / "cyon" / "pyra_lib"))
from pyra_shared import Input, main_logo, HonerableMentions
from rich import print
from rich.table import Table
from rich.console import Console

_console = Console()


class MySexyVariables:
    color = random.choice(["red1", "purple"])
    SEARCH_DIRECTORY = Path.home() / "Desktop"
    PYRA_LIB = Path.home() / "cyon" / "pyra_lib"
    video_dir = Path.home() / "Videos"
    audio_dir = Path.home() / "Music"
    desktop_dir = Path.home() / "Desktop"
    pics_dir = Path.home() / "Pictures"
    downloads_dir = Path.home() / "Downloads"
    vid_list = os.listdir(video_dir)
    audio_list = os.listdir(audio_dir)
    desktop_list = os.listdir(desktop_dir)
    downloads_list = os.listdir(downloads_dir)
    pics_list = os.listdir(pics_dir)
    calls_list = ["download", "pyra run", "pyra lib", "list", "exit"]
    dir_list = ["videos", "desktop", "music", "pictures", "downloads", "exit"]
    pikachu_meme = """[#E8A020]
    ⢀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
    ⢻⣿⡗⢶⣤⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣠⣄
    ⠀⢻⣇⠀⠈⠙⠳⣦⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣤⠶⠛⠋⣹⣿⡿
    ⠀⠀⠹⣆⠀⠀⠀⠀⠙⢷⣄⣀⣀⣀⣤⣤⣤⣄⣀⣴⠞⠋⠉⠀⠀⠀⢀⣿⡟⠁
    ⠀⠀⠀⠙⢷⡀⠀⠀⠀⠀⠉⠉⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⡾⠋⠀⠀
    ⠀⠀⠀⠀⠈⠻⡶⠂⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣠⡾⠋⠀⠀⠀⠀
    ⠀⠀⠀⠀⠀⣼⠃⠀⢠⠒⣆⠀⠀⠀⠀⠀⠀⢠⢲⣄⠀⠀⠀⢻⣆⠀⠀⠀⠀⠀
    ⠀⠀⠀⠀⢰⡏⠀⠀⠈⠛⠋⠀⢀⣀⡀⠀⠀⠘⠛⠃⠀⠀⠀⠈⣿⡀⠀⠀⠀⠀
    ⠀⠀⠀⠀⣾⡟⠛⢳⠀⠀⠀⠀⠀⣉⣀⠀⠀⠀⠀⣰⢛⠙⣶⠀⢹⣇⠀⠀⠀⠀
    ⠀⠀⠀⠀⢿⡗⠛⠋⠀⠀⠀⠀⣾⠋⠀⢱⠀⠀⠀⠘⠲⠗⠋⠀⠈⣿⠀⠀⠀⠀
    ⠀⠀⠀⠀⠘⢷⡀⠀⠀⠀⠀⠀⠈⠓⠒⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⡇⠀⠀⠀
    ⠀⠀⠀⠀⠀⠈⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣧⠀⠀⠀
    ⠀⠀⠀⠀⠀⠈⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠁
      DID YOU TRY TURNING IT ON AND OFF AGAIN?
      ERROR: keyboard not found — press F1 to \n      continue
    [/#E8A020]"""


def _make_table(title: str, items: list) -> Table:
    """Build a two-column index+command table in the Pyra style."""
    c = MySexyVariables.color
    t = Table(
        show_header=False,
        border_style=c,
        box=__import__("rich.box", fromlist=["HEAVY"]).HEAVY,
        title=f"[{c}] {title} [/{c}]",
        title_justify="left",
        padding=(0, 1),
        min_width=28,
    )
    t.add_column("idx", style=c, justify="right", no_wrap=True)
    t.add_column("cmd", style="white", no_wrap=True)
    for i, item in enumerate(items, 1):
        t.add_row(str(i), item)
    return t


class calls:
    @staticmethod
    def call_list():
        _console.print(_make_table("Pyra Tools", MySexyVariables.calls_list))

    @staticmethod
    def directory_list():
        _console.print(_make_table("directory lists", MySexyVariables.dir_list))


class list_dirs:
    @staticmethod
    def _dir_table(path, items):
        import rich.box
        t = Table(
            show_header=False,
            border_style="red1",
            box=rich.box.HEAVY,
            title=f"[red1] {path} [/red1]",
            title_justify="left",
            padding=(0, 1),
        )
        t.add_column("a", style="white", no_wrap=True)
        t.add_column("b", style="white", no_wrap=True)
        t.add_column("c", style="white", no_wrap=True)
        chunk = [items[i:i+3] for i in range(0, len(items), 3)]
        for row in chunk:
            padded = (row + ["", "", ""])[:3]
            t.add_row(*padded)
        _console.print(t)

    @staticmethod
    def pyra_lib_list():
        files = [f.name for f in MySexyVariables.PYRA_LIB.iterdir() if f.is_file()]
        list_dirs._dir_table(str(MySexyVariables.PYRA_LIB), files)

    @staticmethod
    def vid_list():
        os.chdir(MySexyVariables.video_dir)
        list_dirs._dir_table(str(MySexyVariables.video_dir), MySexyVariables.vid_list)

    @staticmethod
    def music_list():
        os.chdir(MySexyVariables.audio_dir)
        list_dirs._dir_table(str(MySexyVariables.audio_dir), MySexyVariables.audio_list)

    @staticmethod
    def desktop_list():
        os.chdir(MySexyVariables.desktop_dir)
        list_dirs._dir_table(
            str(MySexyVariables.desktop_dir), MySexyVariables.desktop_list
        )

    @staticmethod
    def picture_list():
        os.chdir(MySexyVariables.pics_dir)
        list_dirs._dir_table(str(MySexyVariables.pics_dir), MySexyVariables.pics_list)

    @staticmethod
    def downloads_list():
        os.chdir(MySexyVariables.downloads_dir)
        list_dirs._dir_table(
            str(MySexyVariables.downloads_dir), MySexyVariables.downloads_list
        )


class main_functions:
    @staticmethod
    def search_for_file(directory, filename):
        for file in directory.rglob(filename):
            if file.is_file():
                return file
        return None

    @staticmethod
    def run_or_compile(file_path):
        if file_path.suffix == ".py":
            print(f" Running Python file: {file_path}")
            subprocess.run(["python3", str(file_path)])
        elif file_path.suffix == ".c":
            print(f" Compiling and running C file: {file_path}")
            executable = file_path.with_suffix("")
            subprocess.run(["gcc", str(file_path), "-o", str(executable)])
            subprocess.run([str(executable)])
        elif file_path.suffix == ".cpp":
            print(f" Compiling and running C++ file: {file_path}")
            executable = file_path.with_suffix("")
            subprocess.run(["g++", str(file_path), "-o", str(executable)])
            subprocess.run([str(executable)])
        elif file_path.suffix == "":
            subprocess.run([str(file_path)])
        else:
            print(MySexyVariables.pikachu_meme)
            print(f" Unknown file extension: {file_path}")

    @staticmethod
    def pyra_run_func(search_dir):
        import rich.box
        while True:
            if search_dir.is_dir():
                files = list(search_dir.rglob("*.py"))
                names = [f.name for f in files]
                t = Table(
                    show_header=False,
                    border_style="red1",
                    box=rich.box.HEAVY,
                    title=f"[red1] {search_dir} [/red1]",
                    title_justify="left",
                    padding=(0, 1),
                )
                t.add_column("a", style="white", no_wrap=True)
                t.add_column("b", style="white", no_wrap=True)
                t.add_column("c", style="white", no_wrap=True)
                chunk = [names[i:i+3] for i in range(0, len(names), 3)]
                for row in chunk:
                    padded = (row + ["", "", ""])[:3]
                    t.add_row(*padded)
                _console.print(t)
                print("\n Enter a filename:")
                filename = Input.get_string_input().strip()
                if filename.lower() == "exit":
                    break
                file_path = main_functions.search_for_file(search_dir, filename)
                if file_path:
                    print(f" File found: {file_path}")
                    main_functions.run_or_compile(file_path)
                else:
                    print(MySexyVariables.pikachu_meme)
                    print(f" File '{filename}' not found in {search_dir}.")
            else:
                print(MySexyVariables.pikachu_meme)
                print(f" Error: {search_dir} is not a valid directory.")
                break

    @staticmethod
    def download_video():
        def download_call():
            _console.print(_make_table("Download Format", ["mp3", "best video"]))
            video_format = Input.get_string_input().strip()

            if video_format == "exit":
                Main.main()
                return

            print(" filename?")
            output_filename = Input.get_string_input().strip()
            print(" [white]Please enter a link[/white]")
            url = Input.get_string_input().strip()

            if not shutil.which("ffmpeg") and video_format == "best video":
                print(MySexyVariables.pikachu_meme)
                print("Warning: ffmpeg not found — video/audio merging may fail!")

            if video_format == "mp3":
                cmd = [
                    "yt-dlp",
                    "-x",
                    "--audio-format",
                    "mp3",
                    "-o",
                    output_filename,
                    url,
                ]
            elif video_format == "best video":
                cmd = [
                    "yt-dlp",
                    "-f",
                    "bv*+ba/b",
                    "--merge-output-format",
                    "mp4",
                    "-o",
                    output_filename,
                    url,
                ]
            else:
                print(MySexyVariables.pikachu_meme)
                print("Invalid choice!")
                return

            subprocess.call(cmd)

        print(HonerableMentions.save_where)
        directory = Input.get_string_input().strip().lower()
        if directory == "desktop":
            os.chdir(MySexyVariables.desktop_dir)
        elif directory == "videos":
            os.chdir(MySexyVariables.video_dir)
        elif directory == "music":
            os.chdir(MySexyVariables.audio_dir)
        elif directory == "downloads":
            os.chdir(MySexyVariables.downloads_dir)
        elif directory == "exit":
            sys.exit()
        else:
            print(MySexyVariables.pikachu_meme)
            print("Unknown directory, using current path.")

        download_call()


class Main:
    @staticmethod
    def main():
        if platform.system() == "Linux":
            main_logo.logo()
            calls.call_list()
            while True:
                command = Input.get_string_input()

                if command == MySexyVariables.calls_list[0]:  # download
                    main_functions.download_video()
                    Main.main()

                elif command == MySexyVariables.calls_list[1]:  # pyra run
                    main_functions.pyra_run_func(MySexyVariables.SEARCH_DIRECTORY)
                    Main.main()

                elif command == MySexyVariables.calls_list[2]:  # pyra lib
                    main_functions.pyra_run_func(MySexyVariables.PYRA_LIB)
                    Main.main()

                elif command == MySexyVariables.calls_list[3]:  # list
                    while True:
                        calls.directory_list()
                        command = Input.get_string_input()
                        if command == MySexyVariables.dir_list[0]:
                            list_dirs.vid_list()
                        elif command == MySexyVariables.dir_list[1]:
                            list_dirs.desktop_list()
                        elif command == MySexyVariables.dir_list[2]:
                            list_dirs.music_list()
                        elif command == MySexyVariables.dir_list[3]:
                            list_dirs.picture_list()
                        elif command == MySexyVariables.dir_list[4]:
                            list_dirs.downloads_list()
                        elif command == MySexyVariables.dir_list[5]:
                            Main.main()

                elif command == MySexyVariables.calls_list[4]:  # exit
                    sys.exit()

                else:
                    continue
        else:
            print(MySexyVariables.pikachu_meme)


if __name__ == "__main__":
    Main.main()
