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

# Load your image
img = Image.open("main_pyra_logo.jpg")

# Define the number of pixels to shift (positive for right, negative for left)
shift_x = 50  # Adjust this value as needed

# Create a translation matrix
translation_matrix = (1, 0, shift_x, 0, 1, 0)

# Apply the translation to the image
shifted_img = img.transform(img.size, Image.AFFINE, translation_matrix)

# Display the shifted image
shifted_img.show()

# Save the result
shifted_img.save("shifted_image.png")
