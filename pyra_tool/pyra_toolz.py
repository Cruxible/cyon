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
import sys
from pathlib import Path

sys.path.append(str(Path.home() / "cyon" / "pyra_lib"))
from pyra_shared import Input, main_logo, HonerableMentions
from rich import print
from rich.tree import Tree


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


class calls:
    @staticmethod
    def call_list():
        tree = Tree("[white] Pyra Tools", guide_style=f"{MySexyVariables.color}")
        for i in MySexyVariables.calls_list:
            tree.add("[white]" + str(i))
        print(" ", tree)

    @staticmethod
    def directory_list():
        tree = Tree("[white] directory lists", guide_style=f"{MySexyVariables.color}")
        for i in MySexyVariables.dir_list:
            tree.add("[white]" + str(i))
        print(" ", tree)


class list_dirs:
    @staticmethod
    def pyra_lib_list():
        tree = Tree(
            "[white]" + str(MySexyVariables.PYRA_LIB),
            guide_style=f"{MySexyVariables.color}",
        )
        for f in MySexyVariables.PYRA_LIB.iterdir():
            if f.is_file():
                tree.add("[white]" + f.name)
        print(" ", tree)

    @staticmethod
    def vid_list():
        os.chdir(MySexyVariables.video_dir)
        tree = Tree(
            "[white]" + str(MySexyVariables.video_dir),
            guide_style=f"{MySexyVariables.color}",
        )
        for i in MySexyVariables.vid_list:
            tree.add("[white]" + str(i))
        print(" ", tree)

    @staticmethod
    def music_list():
        os.chdir(MySexyVariables.audio_dir)
        tree = Tree(
            "[white]" + str(MySexyVariables.audio_dir),
            guide_style=f"{MySexyVariables.color}",
        )
        for i in MySexyVariables.audio_list:
            tree.add("[white]" + str(i))
        print(" ", tree)

    @staticmethod
    def desktop_list():
        os.chdir(MySexyVariables.desktop_dir)
        tree = Tree(
            "[white]" + str(MySexyVariables.desktop_dir),
            guide_style=f"{MySexyVariables.color}",
        )
        for i in MySexyVariables.desktop_list:
            tree.add("[white]" + str(i))
        print(" ", tree)

    @staticmethod
    def picture_list():
        os.chdir(MySexyVariables.pics_dir)
        tree = Tree(
            "[white]" + str(MySexyVariables.pics_dir),
            guide_style=f"{MySexyVariables.color}",
        )
        for i in MySexyVariables.pics_list:
            tree.add("[white]" + str(i))
        print(" ", tree)

    @staticmethod
    def downloads_list():
        os.chdir(MySexyVariables.downloads_dir)
        tree = Tree(
            "[white]" + str(MySexyVariables.downloads_dir),
            guide_style=f"{MySexyVariables.color}",
        )
        for i in MySexyVariables.downloads_list:
            tree.add("[white]" + str(i))
        print(" ", tree)


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
            executable = file_path.with_suffix("")
            subprocess.run([str(executable)])
        else:
            print(f" Unknown file extension: {file_path}")

    @staticmethod
    def pyra_run_func(search_dir):
        while True:
            if search_dir.is_dir():
                files = list(search_dir.rglob("*.py"))
                tree = Tree(
                    "[white]" + str(search_dir), guide_style=f"{MySexyVariables.color}"
                )
                for f in files:
                    tree.add("[white]" + f.name)
                print(" ", tree)
                print("\n Enter a filename:")
                filename = Input.get_string_input().strip()
                if filename.lower() == "exit":
                    break
                file_path = main_functions.search_for_file(search_dir, filename)
                if file_path:
                    print(f" File found: {file_path}")
                    main_functions.run_or_compile(file_path)
                else:
                    print(f" File '{filename}' not found in {search_dir}.")
            else:
                print(f" Error: {search_dir} is not a valid directory.")
                break

    @staticmethod
    def download_video():
        def download_call():
            list_choice = ["mp3", "best video"]
            tree = Tree("[white]How would you like to download?", guide_style="red")
            for i in list_choice:
                tree.add("[white]" + str(i))
            print(" ", tree)
            video_format = Input.get_string_input().strip()

            if video_format == "exit":
                Main.main()
                return

            print(" filename?")
            output_filename = Input.get_string_input().strip()
            print(" [white]Please enter a link[/white]")
            url = Input.get_string_input().strip()  # remove accidental newlines/spaces

            # Check if ffmpeg exists
            if not shutil.which("ffmpeg") and video_format == "best video":
                print("Warning: ffmpeg not found — video/audio merging may fail!")

            # Prepare subprocess arguments
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
                    "bv*+ba/b",  # best video + audio, fallback to combined
                    "--merge-output-format",
                    "mp4",
                    "-o",
                    output_filename,
                    url,
                ]
            else:
                print("Invalid choice!")
                return

            # Call yt-dlp safely
            subprocess.call(cmd)

        # Ask where to save
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
            print(" wrong, just wrong. Do it again.")
