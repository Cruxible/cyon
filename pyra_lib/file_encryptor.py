#!/usr/bin/env python3
# Author: Ioannes Cruxibulum
# File Encryption Tool
# pip install cryptography rich

import os
import sys
import random
import getpass
from pathlib import Path
from cryptography.fernet import Fernet
from rich.tree import Tree
from rich import print
from rich.console import Console
from pathlib import Path

PYRA_ENV = Path.home() / "pyra_env"
PYRA_LIB = Path.home() / "cyon" / "pyra_lib"

site_pkgs = list(PYRA_ENV.glob("lib/python3*/site-packages"))
if site_pkgs:
    sys.path.insert(0, str(site_pkgs[0]))
sys.path.append(str(PYRA_LIB))
from pyra_shared import Input, main_logo, HonerableMentions


# ─────────────────────────────────────────────
#  VARIABLES
# ─────────────────────────────────────────────
class MyVariables:
    color = random.choice(["red1", "purple"])

    home_dir = Path.home()
    desktop_dir = Path.home() / "Desktop"
    downloads_dir = Path.home() / "Downloads"
    documents_dir = Path.home() / "Documents"

    calls_list = ["encrypt", "decrypt", "generate key", "exit"]

    dir_list = ["Desktop", "Downloads", "Documents", "custom path", "exit"]


# ─────────────────────────────────────────────
#  KEY MANAGER
# ─────────────────────────────────────────────
class KeyManager:
    @staticmethod
    def generate_key() -> bytes:
        return Fernet.generate_key()

    @staticmethod
    def save_key(key: bytes, path: str):
        with open(path, "wb") as f:
            f.write(key)
        print(f" [green]✓ Key saved to:[/green] [white]{path}[/white]")

    @staticmethod
    def load_key(path: str) -> bytes:
        with open(path, "rb") as f:
            return f.read()


# ─────────────────────────────────────────────
#  FILE ENCRYPTOR
# ─────────────────────────────────────────────
class FileEncryptor:
    @staticmethod
    def encrypt(input_path: str, output_path: str, key: bytes):
        try:
            fernet = Fernet(key)
            with open(input_path, "rb") as f:
                data = f.read()
            encrypted = fernet.encrypt(data)
            with open(output_path, "wb") as f:
                f.write(encrypted)
            print(
                f" [green]✓ Encrypted:[/green] [white]{input_path} → {output_path}[/white]"
            )
        except Exception as e:
            print(f" [red]✗ Encryption failed: {e}[/red]")


# ─────────────────────────────────────────────
#  FILE DECRYPTOR
# ─────────────────────────────────────────────
class FileDecryptor:
    @staticmethod
    def decrypt(input_path: str, output_path: str, key: bytes):
        try:
            fernet = Fernet(key)
            with open(input_path, "rb") as f:
                data = f.read()
            decrypted = fernet.decrypt(data)
            with open(output_path, "wb") as f:
                f.write(decrypted)
            print(
                f" [green]✓ Decrypted:[/green] [white]{input_path} → {output_path}[/white]"
            )
        except Exception:
            print(" [red]✗ Decryption failed — wrong key or corrupted file.[/red]")


# ─────────────────────────────────────────────
#  CALLS (menus)
# ─────────────────────────────────────────────
class calls:
    @staticmethod
    def call_list():
        tree = Tree("[white] Encryption Tool", guide_style=f"{MyVariables.color}")
        for i in MyVariables.calls_list:
            tree.add("[white]" + str(i))
        print(" ", tree)

    @staticmethod
    def directory_list():
        tree = Tree("[white] Choose a directory", guide_style=f"{MyVariables.color}")
        for i in MyVariables.dir_list:
            tree.add("[white]" + str(i))
        print(" ", tree)

    @staticmethod
    def file_list(directory: str):
        files = [
            f
            for f in os.listdir(directory)
            if os.path.isfile(os.path.join(directory, f))
        ]
        if not files:
            print(f" [red]No files found in {directory}[/red]")
            return None
        tree = Tree(f"[white] Files in {directory}", guide_style=f"{MyVariables.color}")
        for i, name in enumerate(files, 1):
            tree.add(f"[white]{i}. {name}")
        print(" ", tree)
        return files


# ─────────────────────────────────────────────
#  MAIN FUNCTIONS
# ─────────────────────────────────────────────
class main_functions:
    @staticmethod
    def pick_directory() -> str:
        calls.directory_list()
        choice = Input.get_string_input().strip()

        if choice == MyVariables.dir_list[0]:
            return str(MyVariables.desktop_dir)
        elif choice == MyVariables.dir_list[1]:
            return str(MyVariables.downloads_dir)
        elif choice == MyVariables.dir_list[2]:
            return str(MyVariables.documents_dir)
        elif choice == MyVariables.dir_list[3]:
            print(" [white]Enter full directory path:[/white]")
            custom = Input.get_string_input().strip()
            if os.path.isdir(custom):
                return custom
            else:
                print(f" [red]✗ Directory not found: {custom}[/red]")
                return None
        elif choice == MyVariables.dir_list[4]:
            return None
        else:
            print(" [red]✗ Invalid choice.[/red]")
            return None

    @staticmethod
    def pick_file(directory: str) -> str:
        files = calls.file_list(directory)
        if not files:
            return None
        print(" [white]Pick a file number:[/white]")
        choice = Input.get_string_input().strip()
        if not choice.isdigit() or not (1 <= int(choice) <= len(files)):
            print(" [red]✗ Invalid choice.[/red]")
            return None
        return os.path.join(directory, files[int(choice) - 1])

    @staticmethod
    def pick_key() -> bytes:
        print(" [white]Enter path to your key file:[/white]")
        path = Input.get_string_input().strip()
        if not os.path.isfile(path):
            print(f" [red]✗ Key file not found: {path}[/red]")
            return None
        return KeyManager.load_key(path)

    @staticmethod
    def do_encrypt():
        print(" [white]Select directory containing the file to encrypt:[/white]")
        directory = main_functions.pick_directory()
        if not directory:
            return

        input_path = main_functions.pick_file(directory)
        if not input_path:
            return

        key = main_functions.pick_key()
        if not key:
            return

        base = os.path.splitext(input_path)[0]
        suggestion = base + ".enc"
        print(f" [white]Output filename (Enter for: {suggestion}):[/white]")
        output = Input.get_string_input().strip()
        output_path = output if output else suggestion

        FileEncryptor.encrypt(input_path, output_path, key)

    @staticmethod
    def do_decrypt():
        print(" [white]Select directory containing the file to decrypt:[/white]")
        directory = main_functions.pick_directory()
        if not directory:
            return

        input_path = main_functions.pick_file(directory)
        if not input_path:
            return

        key = main_functions.pick_key()
        if not key:
            return

        base, ext = os.path.splitext(input_path)
        suggestion = base + "_decrypted" + ext
        print(f" [white]Output filename (Enter for: {suggestion}):[/white]")
        output = Input.get_string_input().strip()
        output_path = output if output else suggestion

        FileDecryptor.decrypt(input_path, output_path, key)

    @staticmethod
    def do_generate_key():
        print(" [white]Save key as (e.g. my.key):[/white]")
        path = Input.get_string_input().strip()
        if not path:
            path = "my.key"
        key = KeyManager.generate_key()
        KeyManager.save_key(key, path)


# ─────────────────────────────────────────────
#  MAIN
# ─────────────────────────────────────────────
class Main:
    @staticmethod
    def main():
        calls.call_list()
        while True:
            command = Input.get_string_input().strip()

            if command == MyVariables.calls_list[0]:  # encrypt
                main_functions.do_encrypt()
                Main.main()

            elif command == MyVariables.calls_list[1]:  # decrypt
                main_functions.do_decrypt()
                Main.main()

            elif command == MyVariables.calls_list[2]:  # generate key
                main_functions.do_generate_key()
                Main.main()

            elif command == MyVariables.calls_list[3]:  # exit
                sys.exit()

            else:
                continue


if __name__ == "__main__":
    Main.main()
