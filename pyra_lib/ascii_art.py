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
from PIL import Image

# ASCII characters to use (dark to light)
ASCII_CHARS = "@%#*+=-:. "


def resize_image(image, new_width=100):
    """Resize image while maintaining aspect ratio."""
    width, height = image.size
    aspect_ratio = height / width
    new_height = int(
        aspect_ratio * new_width * 0.55
    )  # Adjust height to match terminal proportions
    return image.resize((new_width, new_height))


def grayscale_image(image):
    """Convert image to grayscale."""
    return image.convert("L")


def map_pixels_to_ascii_chars(image, ascii_chars=ASCII_CHARS):
    """Map pixels to ASCII characters based on intensity."""
    pixels = image.getdata()
    ascii_str = "".join([ascii_chars[pixel // 25] for pixel in pixels])
    return ascii_str


def convert_image_to_ascii(image_path, new_width=100):
    """Convert an image to ASCII art."""
    try:
        image = Image.open(image_path)
    except Exception as e:
        print(f"Unable to open image: {e}")
        return

    # Resize and convert the image
    image = resize_image(image, new_width)
    image = grayscale_image(image)

    # Map pixels to ASCII characters
    ascii_str = map_pixels_to_ascii_chars(image)

    # Format the ASCII string to fit the output size
    ascii_lines = [
        ascii_str[i : i + new_width] for i in range(0, len(ascii_str), new_width)
    ]
    return "\n".join(ascii_lines)


if __name__ == "__main__":
    image_path = input("Enter the path to the image: ")
    new_width = int(input("Enter the desired width (e.g., 100): "))

    ascii_art = convert_image_to_ascii(image_path, new_width)
    if ascii_art:
        print(ascii_art)
