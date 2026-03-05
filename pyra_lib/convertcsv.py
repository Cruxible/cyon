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
import pandas as pd
import re


def clean_dialog(text):
    text = re.sub(r"\s+", " ", text)
    # Uncomment to remove stage directions:
    # text = re.sub(r'\([^)]*\)', '', text)
    return text.strip()


def main():
    df = pd.read_csv("lotr_scripts.csv")
    df = df.dropna(subset=["char", "dialog"])

    with open("lotr_quotes.txt", "w", encoding="utf-8") as f:
        for _, row in df.iterrows():
            quote = clean_dialog(row["dialog"])
            character = row["char"].strip()
            if quote and character:
                f.write(f"{quote}|{character}\n")

    print("✅ lotr_quotes.txt has been created.")


if __name__ == "__main__":
    main()
