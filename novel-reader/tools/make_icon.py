"""Generate the app's native 10x10 monochrome open-book icon."""
from pathlib import Path
from PIL import Image

rows = [0b0111001110, 0b1000110001, 0b1010110101, 0b1000110001,
        0b1010110101, 0b1000110001, 0b1000110001, 0b0111001110,
        0b0000110000, 0b0000000000]
image = Image.new("1", (10, 10), 1)
for y, bits in enumerate(rows):
    for x in range(10):
        if bits & (1 << (9-x)):
            image.putpixel((x, y), 0)
image.save(Path(__file__).resolve().parents[1] / "src/book.png")
image.save(Path(__file__).resolve().parents[1] / "src/images/book.png")
print("Generated 10x10 1-bit book.png")
