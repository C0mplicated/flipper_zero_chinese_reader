"""Validate font format and generate a magnified development preview."""
from pathlib import Path
import tempfile
from PIL import Image

root = Path(__file__).resolve().parents[1]
data = (root / "sdcard/apps_data/novel_reader/font12.bin").read_bytes()
assert len(data) == 8 + 65536 * 24
assert data[:8] == b"NRF12V1\0"
sample = (root / "sdcard/apps_data/novel_reader/books/sample.txt").read_text(encoding="utf-8")
for char in set(sample):
    if ord(char) >= 128 and not char.isspace():
        assert any(data[8 + ord(char)*24:8 + (ord(char)+1)*24]), char
screen = Image.new("RGB", (128, 64), "#ebae43")
for row, line in enumerate(["雨停的时候，街道尽头", "还亮着一盏灯。林舟把", "信放进口袋，沿着石阶", "慢慢走下去。测试中文"]):
    for col, char in enumerate(line):
        glyph = data[8 + ord(char)*24:8 + (ord(char)+1)*24]
        assert any(glyph), char
        for y in range(12):
            for x in range(12):
                if glyph[2*y+x//8] & (128 >> (x%8)):
                    screen.putpixel((1 + col*12+x, row*13+y), (20, 20, 20))
screen.resize((768, 384), Image.Resampling.NEAREST).save(Path(tempfile.gettempdir()) / "novel-reader-preview.png")
print("PASS: font header, size, sample Chinese coverage; preview generated")
