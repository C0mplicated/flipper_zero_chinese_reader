"""Host integration tests. Pass the shared reader_core library as argv[1]."""
import ctypes as C
import random
import sys

class Glyph(C.Structure):
    _fields_ = [("code", C.c_uint32), ("x", C.c_uint8), ("y", C.c_uint8)]

class Page(C.Structure):
    _fields_ = [("glyphs", Glyph * 84), ("count", C.c_size_t), ("next", C.c_uint32)]

GET = C.CFUNCTYPE(C.c_int, C.c_void_p)
lib = C.CDLL(sys.argv[1])
lib.reader_page.argtypes = [GET, C.c_void_p, C.c_uint32, C.POINTER(Page)]
lib.reader_page.restype = None

def paginate(text):
    data = text.encode("utf-8")
    offset = 0
    pages = []
    while offset < len(data):
        pos = offset
        @GET
        def get(_):
            nonlocal pos
            if pos == len(data):
                return -1
            b = data[pos]
            pos += 1
            return b
        p = Page()
        lib.reader_page(get, None, offset, C.byref(p))
        assert offset < p.next <= len(data)
        assert p.count <= 84
        lines = ["", "", "", ""]
        for g in p.glyphs[:p.count]:
            assert g.x >= 1 and g.x + (6 if g.code < 128 else 12) <= 127
            assert g.y in (0, 13, 26, 39)
            lines[g.y // 13] += chr(g.code)
        pages.append(lines)
        offset = p.next
    actual = "".join("".join(p) for p in pages)
    assert "".join(actual.split()) == "".join(text.replace('\ufeff', '').split()), (text, actual)
    return pages

assert paginate("a\n\n\n\nb") == [["a", "b", "", ""]]
assert paginate("一" * 10 + "，二")[0][:2] == ["一" * 9, "一，二"]
assert paginate("一" * 9 + "“二”")[0][:2] == ["一" * 9, "“二”"]
p = paginate("一" * 40 + "，二")
assert p[0][3] == "一" * 9 and p[1][0] == "一，二"
p = paginate("一" * 39 + "“二”")
assert p[0][3] == "一" * 9 and p[1][0] == "“二”"
paginate("一" * 10 + "。”" + "二" * 30)
paginate("，" * 1000)
paginate("（" * 1000)
paginate("中文\n" * 1000)
random.seed(17)
for _ in range(500):
    paginate("".join(random.choices("中文阅读一二三abc123，。！？（）“”《》… \n\t\r", k=500)))
print("PASS: compact paragraphs, punctuation at line/page edges, order/no-loss, 500 random cases")
