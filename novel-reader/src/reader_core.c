#include "reader_core.h"

static uint32_t decode(ReaderGet get, void* ctx, uint32_t* pos) {
    int b = get(ctx);
    if(b < 0) return UINT32_MAX;
    ++*pos;
    if(b < 128) return (uint32_t)b;
    unsigned n;
    uint32_t c, min;
    if(b >= 0xC2 && b <= 0xDF) { n = 1; c = b & 31; min = 128; }
    else if(b >= 0xE0 && b <= 0xEF) { n = 2; c = b & 15; min = 2048; }
    else if(b >= 0xF0 && b <= 0xF4) { n = 3; c = b & 7; min = 65536; }
    else return 0xFFFD;
    for(unsigned i = 0; i < n; ++i) {
        b = get(ctx);
        if(b < 0) return 0xFFFD;
        ++*pos;
        if((b & 0xC0) != 0x80) return 0xFFFD;
        c = (c << 6) | (b & 63);
    }
    if(c < min || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) return 0xFFFD;
    return c;
}

static bool closing(uint32_t c) {
    switch(c) {
    case ',': case '.': case '!': case '?': case ':': case ';':
    case ')': case ']': case '}':
    case 0xFF0C: case 0x3002: case 0x3001: case 0xFF01: case 0xFF1F:
    case 0xFF1A: case 0xFF1B: case 0x201D: case 0x2019: case 0xFF09:
    case 0x300B: case 0x3009: case 0x3011: case 0x300D: case 0x300F:
    case 0x2026: return true;
    default: return false;
    }
}

static bool opening(uint32_t c) {
    switch(c) {
    case '(': case '[': case '{': case 0x201C: case 0x2018: case 0xFF08:
    case 0x300A: case 0x3008: case 0x3010: case 0x300C: case 0x300E: return true;
    default: return false;
    }
}

void reader_page(ReaderGet get, void* ctx, uint32_t start, ReaderPage* page) {
    uint32_t pos = start;
    uint32_t offsets[84];
    unsigned x = 0, row = 0;
    page->count = 0;
    while(true) {
        uint32_t before = pos;
        uint32_t c = decode(get, ctx, &pos);
        if(c == UINT32_MAX) break;
        if(c == 0xFEFF || c == '\r') continue;
        /* Compact paragraphs: keep the paragraph boundary, not empty rows.
         * Consume trailing whitespace even when the page is full. */
        if(c == '\n') { if(x) ++row; x = 0; continue; }
        if(c == '\t') c = ' ';
        if(c < 32 || c == 127) continue;
        if(!x && (c == ' ' || c == 0x3000)) continue;
        unsigned width = c < 128 ? 6 : 12;
        if(row >= 4) { pos = before; break; }
        if(x + width > 126) {
            size_t split = page->count;
            size_t line_start = split;
            while(line_start && page->glyphs[line_start - 1].y == row * 13) --line_start;
            if(closing(c) && split > line_start) {
                --split;
                while(split > line_start && closing(page->glyphs[split].code)) --split;
            }
            while(split > line_start && opening(page->glyphs[split - 1].code)) --split;
            unsigned moved_width = 0;
            for(size_t i = split; i < page->count; ++i)
                moved_width += page->glyphs[i].code < 128 ? 6 : 12;
            /* Degenerate punctuation-only runs must still make progress. */
            if(split == line_start || moved_width + width > 126) split = page->count;
            ++row;
            x = 0;
            if(row >= 4) {
                pos = split < page->count ? offsets[split] : before;
                page->count = split;
                break;
            }
            for(size_t i = split; i < page->count; ++i) {
                page->glyphs[i].x = x + 1;
                page->glyphs[i].y = row * 13;
                x += page->glyphs[i].code < 128 ? 6 : 12;
            }
        }
        if(page->count >= 84) { pos = before; break; }
        offsets[page->count] = before;
        page->glyphs[page->count++] = (ReaderGlyph){c, (uint8_t)(x + 1), (uint8_t)(row * 13)};
        x += width;
    }
    page->next = pos;
}
