from PIL import Image

# Open the WebP file
input_file = "/home/jonathan/Pictures/hacker_pics/neon-gengar-evolution-emegyhfy6blsk7b6.webp"  # Replace with your WebP file
output_file = "example.png"  # Replace with desired output file (e.g., example.jpg)

# Convert and save
with Image.open(input_file) as img:
    img.save(output_file)
    print(f"Converted '{input_file}' to '{output_file}'.")
