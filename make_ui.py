import re

def build_ui():
    print("Reading screndata.txt...")
    try:
        with open("screndata.txt", "r") as f:
            lines = f.readlines()
    except FileNotFoundError:
        print("ERROR: screndata.txt not found!")
        return

    screen_bytes = []
    color_bytes = []
    target = None

    for line in lines:
        if "Screen data" in line:
            target = screen_bytes
        elif "Colour data" in line:
            target = color_bytes
        elif "BYTE" in line and target is not None:
            row_bytes = re.findall(r'\$([0-9A-Fa-f]{2})', line)[:40]
            target.extend(row_bytes)

    # Generate ONLY the raw data header
    print("Generating ui_data.h...")
    with open("ui_data.h", "w") as f:
        f.write("#ifndef UI_DATA_H\n#define UI_DATA_H\n\n")
        f.write("static const unsigned char ui_screen_data[1000] = {\n    ")
        f.write(", ".join(["0x" + b for b in screen_bytes[:1000]]))
        f.write("\n};\n\n")
        f.write("static const unsigned char ui_color_data[1000] = {\n    ")
        f.write(", ".join(["0x" + b for b in color_bytes[:1000]]))
        f.write("\n};\n\n#endif\n")
        
    print("Success! ui_data.h created (Raw Data Only).")

if __name__ == '__main__':
    build_ui()