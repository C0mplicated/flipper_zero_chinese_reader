#include "reader_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { const unsigned char* data; size_t pos, size; } Stream;
static int get(void* ctx) {
    Stream* s = ctx;
    return s->pos < s->size ? s->data[s->pos++] : -1;
}
static void check(const char* data, unsigned glyphs) {
    Stream s = {(const unsigned char*)data, 0, strlen(data)};
    unsigned total = 0;
    ReaderPage page;
    while(s.pos < s.size) {
        size_t before = s.pos;
        reader_page(get, &s, (uint32_t)before, &page);
        assert(page.next > before && page.next <= s.size);
        assert(page.count <= 84);
        total += page.count;
        for(size_t i = 0; i < page.count; ++i) {
            assert(page.glyphs[i].x < 127);
            assert(page.glyphs[i].y <= 39);
        }
        s.pos = page.next;
    }
    assert(total == glyphs);
}
int main(void) {
    check("", 0);
    check("你好，世界！Hello", 11);
    check("\xEF\xBB\xBF" "ABC\r\nDEF", 6);
    check("a\n\n\n\nb", 2);
    check("\xF0\x9F\x98\x80", 1);
    check("\xFF\xFE", 2);
    check("\xE4\xB8", 1);
    char large[30001];
    for(unsigned i = 0; i < 10000; ++i) memcpy(large + 3 * i, "中", 3);
    large[30000] = 0;
    check(large, 10000);
    memset(large, 'a', 30000);
    check(large, 30000);
    puts("PASS: UTF-8, BOM, CRLF, blank lines, malformed input, pagination, long books");
}
