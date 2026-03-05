#!/usr/bin/env python3
# Author: Ioannes Cruxibulum
# pyra_shared.py — shared classes for all Pyra tools

import os
import random
import getpass
from rich.tree import Tree
from rich import print
from rich.console import Console


class Input:
    @staticmethod
    def get_string_input():
        color = random.choice(["red1", "purple"])
        user = getpass.getuser()
        curdir = os.getcwd()
        console = Console()
        return console.input(
            " [white]_______________________________________________[/white]"
            + f"[{color}]\n ┌░[/{color}]"
            + "[white]"
            + curdir
            + "░[/white]"
            + f"[{color}]\n └░[/{color}]"
            + "[white]"
            + user
            + "[/white]"
            + f"[{color}]░ [/{color}]"
        )

    @staticmethod
    def get_integer_input():
        color = random.choice(["red1", "purple"])
        user = getpass.getuser()
        curdir = os.getcwd()
        console = Console()
        return int(
            console.input(
                " [white]_______________________________________________[/white]"
                + f"[{color}]\n ┌░[/{color}]"
                + "[white]"
                + curdir
                + "░[/white]"
                + f"[{color}]\n └░[/{color}]"
                + "[white]"
                + user
                + "[/white]"
                + f"[{color}]░ [/{color}]"
            )
        )

    @staticmethod
    def get_float_input():
        color = random.choice(["red1", "purple"])
        user = getpass.getuser()
        curdir = os.getcwd()
        console = Console()
        return float(
            console.input(
                " [white]_______________________________________________[/white]"
                + f"[{color}]\n ┌░[/{color}]"
                + "[white]"
                + curdir
                + "░[/white]"
                + f"[{color}]\n └░[/{color}]"
                + "[white]"
                + user
                + "[/white]"
                + f"[{color}]░ [/{color}]"
            )
        )


class main_logo:
    @staticmethod
    def logo():
        color = random.choice(["red1", "purple"])
        program_creator = " Creator: Ioannes Cruxibulum"
        program_name = " Pyra Toolz"
        program_version = " 3.0"
        logos = [
            f"[{color}] \n  ██▓███ ▓██   ██▓ ██▀███   ▄▄▄      \n ▓██░  ██▒▒██  ██▒▓██ ▒ ██▒▒████▄    \n ▓██░ ██▓▒ ▒██ ██░▓██ ░▄█ ▒▒██  ▀█▄  \n ▒██▄█▓▒ ▒ ░ ▐██▓░▒██▀▀█▄  ░██▄▄▄▄██ \n ▒██▒ ░  ░ ░ ██▒▓░░██▓ ▒██▒ ▓█   ▓██▒\n ▒▓▒░ ░  ░  ██▒▒▒ ░ ▒▓ ░▒▓░ ▒▒   ▓▒█░\n ░▒ ░     ▓██ ░▒░   ░▒ ░ ▒░  ▒   ▒▒ ░\n ░░       ▒ ▒ ░░    ░░   ░   ░   ▒   \n          ░ ░        ░           ░  ░\n          ░ ░                        \n\n {program_name} {program_version}\n {program_creator}[/{color}]",
            f"[{color}]  ___  _   _ ____ ____\n  |__]  \\_/  |__/ |__|\n  |      |   |  \\ |  |\n\n {program_name} {program_version}\n {program_creator}[/{color}]",
        ]
        print(random.choice(logos))


class HonerableMentions:
    save_where = " [white]Save on Desktop Videos Music Downloads?[/white]"
    save_where_termux = " [white]Save on:\n Desktop\n Videos\n droid movies\n droid music\n droid download[/white]"
    save_audio_where = " [white]Save on Desktop Music?[/white]"
    old_filename = " [white]Filename?[/white]"
    new_filename = " [white]New filename?[/white]"
    exit_program = " [white]Exiting the program...[/white]"
