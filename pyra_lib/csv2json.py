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
import json

# Load CSVs
scripts_df = pd.read_csv("lotr_scripts.csv")
characters_df = pd.read_csv("lotr_characters.csv")

# Normalize character names for better matching
scripts_df["char"] = scripts_df["char"].astype(str).str.strip().str.lower()
characters_df["name"] = characters_df["name"].astype(str).str.strip().str.lower()

# Drop rows in scripts where dialog is missing
scripts_df = scripts_df.dropna(subset=["dialog"])

# Merge on normalized name
merged_df = scripts_df.merge(characters_df, left_on="char", right_on="name")

# Debug output
print("Sample script characters:", scripts_df["char"].unique()[:10])
print("Sample character names:", characters_df["name"].unique()[:10])
print(f"Merged rows: {len(merged_df)}")

# Save to JSONL
with open("lotr_quotes.jsonl", "w", encoding="utf-8") as f:
    count = 0
    for _, row in merged_df.iterrows():
        quote = str(row["dialog"]).strip()
        character = str(row["char"]).strip().title()
        if quote:
            f.write(json.dumps({"quote": quote, "character": character}) + "\n")
            count += 1

print(f"✅ Saved {count} quotes to lotr_quotes.jsonl")
