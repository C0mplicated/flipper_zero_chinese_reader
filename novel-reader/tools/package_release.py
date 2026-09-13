"""Build one reproducible file-list archive and record release hashes."""
import hashlib
from pathlib import Path
import zipfile
root = Path(__file__).resolve().parents[1]
files = sorted(p for p in root.rglob("*") if p.is_file() and p.name != "SHA256SUMS.txt")
manifest = "".join(f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(root).as_posix()}\n" for p in files)
(root / "SHA256SUMS.txt").write_text(manifest, encoding="utf-8")
archive = root.parent / "novel-reader-momentum-d3f89dfe-v0.4-final.zip"
with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as z:
    for p in files + [root / "SHA256SUMS.txt"]:
        z.write(p, (Path(root.name) / p.relative_to(root)).as_posix())
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
print(archive)
print(f"Archive bytes: {archive.stat().st_size}")
