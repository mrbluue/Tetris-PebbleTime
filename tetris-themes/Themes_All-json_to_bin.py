import json
import struct
import os

# --- Helpers -------------------------------------------------------------

# Map a single 8-bit channel (0–255) to 2-bit Pebble value (0–3)
def map_channel(c):
    return int(c * 3 / 255 + 0.5)

# Convert an [R,G,B] list to a single Pebble byte
def rgb_to_pebble_byte(rgb):
    r, g, b = rgb
    r2 = map_channel(r)
    g2 = map_channel(g)
    b2 = map_channel(b)

    # 0b11 RRR GG BB → same pattern you used
    return ((r2 << 4) | (g2 << 2) | b2) | 0xC0

# Extract colors from JSON structure
def extract_colors(data):
    colors = []
    for key, value in data.items():
        if isinstance(value, list) and len(value) > 0 and isinstance(value[0], list):
            # List of lists
            colors.extend(value)
        else:
            colors.append(value)
    return colors

# --- Main ----------------------------------------------------------------

def main():
    theme_files = [f"theme_{i:02}.json" for i in range(4)]
    output = bytearray()

    for filename in theme_files:
        if not os.path.exists(filename):
            print(f"Warning: {filename} missing, skipping.")
            continue

        print(f"Processing {filename}...")

        with open(filename, "r") as f:
            data = json.load(f)

        colors = extract_colors(data)
        for rgb in colors:
            output.append(rgb_to_pebble_byte(rgb))

    # Write all packed themes
    with open("themes.bin", "wb") as f:
        f.write(output)

    print(f"Done: wrote {len(output)} bytes to themes.bin")

if __name__ == "__main__":
    main()
