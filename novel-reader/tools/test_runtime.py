"""Exercise actual app cache/light functions against host file/API adapters.
This verifies app logic, not the physical firmware notification service.
Requires a host C compiler; writes all generated files in a temporary directory.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/novel_reader.c").read_text()
def function(name):
    import re
    match = re.search(r"static [^\n]+\b" + name + r"\([^;]*?\)\s*\{", source)
    assert match, name
    start = match.start()
    at = source.index("{", match.start())
    depth = 1
    end = at + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]
struct = source[source.index("typedef struct {"):source.index("} Reader;") + len("} Reader;")]
names = ["light_restore", "light_apply", "preferences", "get_byte", "parse", "status",
         "build_index", "fingerprint", "index_offset", "resume_index", "prepare_index"]
adapters = (root / "tools/runtime_adapters.h").read_text()
tests = (root / "tools/runtime_cases.h").read_text()
with tempfile.TemporaryDirectory(prefix="reader-runtime-") as directory:
    tmp = Path(directory)
    (tmp / "runtime.c").write_text(adapters + "\n" + struct + "\n" + "\n".join(map(function, names)) + "\n" + tests)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
                    "-I", str(root / "src"), str(tmp / "runtime.c"), str(root / "src/reader_core.c"),
                    "-o", str(tmp / "test")], check=True)
    subprocess.run([str(tmp / "test")], cwd=tmp, check=True)
