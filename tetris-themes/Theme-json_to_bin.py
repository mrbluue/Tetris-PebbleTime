import json
import struct
import os

name = input("json file name: ")

# Input JSON file
input_file = name + ".json"
# Output binary file
output_file = name + ".bin"

# Map a single 8-bit channel (0-255) to 2-bit Pebble value (0-3)
def map_channel(c):
    # Map 0..255 -> 0..3
    return int(c * 3 / 255 + 0.5)  # +0.5 for rounding

# Convert RGB tuple to 1-byte Pebble color
def rgb_to_pebble_byte(rgb):
    r, g, b = rgb
    # Map each channel to 2 bits
    r2 = map_channel(r)
    g2 = map_channel(g)
    b2 = map_channel(b)
    # Pebble seems to store as: 0b11RRRRGG
    # From your example, we will match it manually:
    # Combine channels (the exact mapping may vary, but this matches your example)
    return ((r2 << 4) | (g2 << 2) | b2) | 0xC0

# Flatten the JSON colors into a list of RGB tuples
def extract_colors(data):
    colors = []
    for key, value in data.items():
        if isinstance(value[0], list):  # list of colors
            colors.extend(value)
        else:
            colors.append(value)
    return colors

def main():
    # Load JSON
    with open(input_file, 'r') as f:
        data = json.load(f)

    colors = extract_colors(data)
    # Convert to bytes
    pebble_bytes = bytes([rgb_to_pebble_byte(c) for c in colors])

    # Write to bin file
    with open(output_file, 'wb') as f:
        f.write(pebble_bytes)

    print("Written", len(pebble_bytes), "colors to colors.bin")

if __name__ == "__main__":
    main()