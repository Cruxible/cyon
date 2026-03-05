#!/usr/bin/env python3
# Author: Ioannes Cruxibulum
# tarmaker.py — part of pyra_lib

import os
import sys
import tarfile
import shutil
import subprocess
from pathlib import Path

PYRA_ENV = Path.home() / "pyra_env"
PYRA_LIB = Path.home() / "cyon" / "pyra_lib"

site_pkgs = list(PYRA_ENV.glob("lib/python3*/site-packages"))
if site_pkgs:
    sys.path.insert(0, str(site_pkgs[0]))
sys.path.append(str(PYRA_LIB))
from pyra_shared import Input, main_logo, HonerableMentions


class TarMaker:
    @staticmethod
    def make_tarfile(output_filename, source_dir, destination):
        with tarfile.open(output_filename, "w:gz") as tar:
            tar.add(source_dir, arcname=os.path.basename(source_dir))
        dest_path = Path(destination) / output_filename
        try:
            shutil.move(output_filename, dest_path)
            print(f" Tarball moved to {dest_path}")
        except Exception as e:
            print(f" Failed to move the tarball: {e}")

    @staticmethod
    def enc_make_tarfile(output_filename, source_dir):
        with tarfile.open(output_filename, "w:gz") as tar:
            tar.add(source_dir, arcname=os.path.basename(source_dir))
        print(f" Created tarball: {output_filename}")

    @staticmethod
    def encrypt_tarball(tar_filename, password, destination):
        encrypted_filename = tar_filename + ".gpg"
        try:
            subprocess.run(
                [
                    "gpg",
                    "--symmetric",
                    "--cipher-algo",
                    "AES256",
                    "--batch",
                    "--yes",
                    "--passphrase",
                    password,
                    tar_filename,
                ],
                check=True,
            )
            print(f" Encrypted tarball: {encrypted_filename}")
            dest_path = Path(destination) / os.path.basename(encrypted_filename)
            shutil.move(encrypted_filename, dest_path)
            print(f" Moved encrypted tarball to {dest_path}")
            os.remove(tar_filename)
        except Exception as e:
            print(f" Failed to encrypt or move the tarball: {e}")

    @staticmethod
    def decrypt_tarball(encrypted_filename, password, destination):
        try:
            decrypted_filename = encrypted_filename.replace(".gpg", "")
            with open(decrypted_filename, "wb") as decrypted_file:
                subprocess.run(
                    [
                        "gpg",
                        "--decrypt",
                        "--batch",
                        "--yes",
                        "--passphrase",
                        password,
                        encrypted_filename,
                    ],
                    stdout=decrypted_file,
                    check=True,
                )
            print(f" Decrypted tarball: {decrypted_filename}")
            dest_path = Path(destination) / os.path.basename(decrypted_filename)
            shutil.move(decrypted_filename, dest_path)
            print(f" Moved decrypted tarball to: {dest_path}")
            with tarfile.open(dest_path, "r:gz") as tar:
                tar.extractall(str(destination))
            print(" Decryption and extraction completed.")
        except Exception as e:
            print(f" Failed to decrypt or extract the tarball: {e}")


def run():
    # destination changes depending on Linux vs Termux
    if Path("/sdcard/Download").exists():
        destination = "/sdcard/Download"
    else:
        destination = str(Path.home() / "Downloads")

    while True:
        print(" commands:\n create tarfile\n encrypt tarfile\n decrypt tarfile\n exit")
        tar_funcs = Input.get_string_input()

        if tar_funcs == "create tarfile":
            print("\n Directory to be packed? (Example: /home/ioannes/pyra_bin)")
            source_dir = Input.get_string_input()
            if source_dir in ("exit", ""):
                break
            print("\n New Tarball name?")
            new_dir = Input.get_string_input()
            if new_dir == "exit":
                break
            TarMaker.make_tarfile(f"{new_dir}.tar.gz", source_dir, destination)

        elif tar_funcs == "encrypt tarfile":
            print("\n Directory to be packed? (Example: /home/ioannes/pyra_bin)")
            source_dir = Input.get_string_input()
            if source_dir == "exit":
                break
            print("\n New Tarball name?")
            new_dir = Input.get_string_input()
            if new_dir == "exit":
                break
            tar_filename = f"{new_dir}.tar.gz"
            TarMaker.enc_make_tarfile(tar_filename, source_dir)
            print("\n Enter a password for encryption:")
            password = Input.get_string_input()
            TarMaker.encrypt_tarball(tar_filename, password, destination)

        elif tar_funcs == "decrypt tarfile":
            print(
                "\n Encrypted tarball path? (Example: /sdcard/Download/archive.tar.gz.gpg)"
            )
            encrypted_filename = Input.get_string_input()
            if encrypted_filename == "exit":
                break
            print("\n Enter the decryption password:")
            password = Input.get_string_input()
            TarMaker.decrypt_tarball(encrypted_filename, password, destination)

        elif tar_funcs == "exit":
            break

        else:
            print(" Invalid command")


if __name__ == "__main__":
    run()
