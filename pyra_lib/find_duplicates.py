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

import os
import hashlib
from collections import defaultdict


def hash_file(file_path, chunk_size=4096):
    """Generate SHA-256 hash of a file's content."""
    sha256 = hashlib.sha256()
    try:
        with open(file_path, "rb") as f:
            while chunk := f.read(chunk_size):
                sha256.update(chunk)
    except (OSError, IOError) as e:
        print(f"Could not read {file_path}: {e}")
        return None
    return sha256.hexdigest()


def find_duplicates(directory):
    """Find duplicate files in the given directory."""
    hashes = defaultdict(list)

    for root, _, files in os.walk(directory):
        for filename in files:
            file_path = os.path.join(root, filename)
            file_hash = hash_file(file_path)

            if file_hash:
                hashes[file_hash].append(file_path)

    # Filter out unique files, keeping only duplicates
    duplicates = {h: paths for h, paths in hashes.items() if len(paths) > 1}
    return duplicates


def display_duplicates(duplicates):
    """Display duplicate files in a readable way."""
    if not duplicates:
        print("No duplicate files found.")
        return

    print("Duplicate files found:")
    for hash_value, files in duplicates.items():
        print(f"\nHash: {hash_value}")
        for file in files:
            print(f"  - {file}")


def main():
    directory = input("Enter the directory to scan for duplicates: ").strip()
    if not os.path.isdir(directory):
        print(f"{directory} is not a valid directory.")
        return

    duplicates = find_duplicates(directory)
    display_duplicates(duplicates)

    # Optional: Delete duplicates (only keep the first occurrence)
    if duplicates and input("\nDelete duplicates? (y/n): ").strip().lower() == "y":
        for files in duplicates.values():
            for file in files[1:]:  # Keep the first, delete the rest
                try:
                    os.remove(file)
                    print(f"Deleted: {file}")
                except OSError as e:
                    print(f"Error deleting {file}: {e}")


if __name__ == "__main__":
    main()
