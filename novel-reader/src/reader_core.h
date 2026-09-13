#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef int (*ReaderGet)(void* context);
typedef struct {
    uint32_t code;
    uint8_t x, y;
} ReaderGlyph;
typedef struct {
    ReaderGlyph glyphs[84];
    size_t count;
    uint32_t next;
} ReaderPage;
/* Reads from a caller-positioned UTF-8 stream. The caller seeks to page.next
 * before the next page. Punctuation wrapping may roll back several glyphs. */
void reader_page(ReaderGet get, void* context, uint32_t start, ReaderPage* page);
