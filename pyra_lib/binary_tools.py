#!/usr/bin/env python3
# Author: Ioannes Cruxibulum
# binary_tools.py — part of pyra_lib

import sys
from pathlib import Path

PYRA_ENV = Path.home() / "pyra_env"
PYRA_LIB = Path.home() / "cyon" / "pyra_lib"

site_pkgs = list(PYRA_ENV.glob("lib/python3*/site-packages"))
if site_pkgs:
    sys.path.insert(0, str(site_pkgs[0]))
sys.path.append(str(PYRA_LIB))
from pyra_shared import Input, main_logo, HonerableMentions


class BinaryTools:
    @staticmethod
    def binary_to_string(binary_string):
        binary_values = binary_string.split(" ")
        characters = [chr(int(bv, 2)) for bv in binary_values]
        return "".join(characters)

    @staticmethod
    def string_to_binary(input_string):
        binary_list = [format(ord(char), "08b") for char in input_string]
        return " ".join(binary_list)

    @staticmethod
    def bin_2_string():
        print(" Enter binary number")
        binary_input = Input.get_string_input()
        if binary_input == "exit":
            return
        original_string = BinaryTools.binary_to_string(binary_input)
        print(f" Binary Representation: {binary_input}")
        print(f" Original String: {original_string}")

    @staticmethod
    def string_2_bin():
        print(" Enter string")
        pyra_string = Input.get_string_input()
        if pyra_string == "exit":
            return
        binary_representation = BinaryTools.string_to_binary(pyra_string)
        print(f" Original String: {pyra_string}")
        print(f" Binary Representation: {binary_representation}")


def run():
    while True:
        print(
            " Binary Functions:\n __________________\n bin 2 string\n string 2 bin\n exit"
        )
        command = Input.get_string_input()
        if command == "bin 2 string":
            BinaryTools.bin_2_string()
        elif command == "string 2 bin":
            BinaryTools.string_2_bin()
        elif command == "exit":
            break
        else:
            continue


if __name__ == "__main__":
    run()
