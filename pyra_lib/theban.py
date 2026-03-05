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

# Define the mapping from English to Theban
english_to_theban = {
    "a": "᚛",
    "b": "᚜",
    "c": "ሣ",
    "d": "ካ",
    "e": "ኪ",
    "f": "ᚠ",
    "g": "ᚡ",
    "h": "ᚢ",
    "i": "ᚣ",
    "j": "ᚤ",
    "k": "ᚥ",
    "l": "ᚦ",
    "m": "ᚧ",
    "n": "ᚨ",
    "o": "ᚩ",
    "p": "ᚪ",
    "q": "ᚫ",
    "r": "ᚬ",
    "s": "ᚭ",
    "t": "ᚮ",
    "u": "ᚯ",
    "v": "ᚰ",
    "w": "ᚱ",
    "x": "x",
    "y": "ᚳ",
    "z": "ᚴ",
}


def convert_to_theban():
    # Take user input
    text = input("Enter your text: ")

    # Convert the text to lowercase
    text = text.lower()

    # Convert each character to Theban
    theban_text = ""
    for char in text:
        if char in english_to_theban:
            theban_text += english_to_theban[char]
        else:
            theban_text += (
                char  # Keep the character as it is if it's not in the English alphabet
            )

    return theban_text


# Test the function
print(convert_to_theban())
