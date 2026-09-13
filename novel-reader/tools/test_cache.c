#include "reader_cache.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    ReaderCacheHeader h = {READER_CACHE_MAGIC, READER_LAYOUT, 19508131, 1234, 174279};
    uint64_t bytes = sizeof(h) + 174279U * 4U;
    assert(reader_cache_valid(&h, 19508131, 1234, bytes));
    assert(!reader_cache_valid(&h, 19508132, 1234, bytes));
    assert(!reader_cache_valid(&h, 19508131, 1235, bytes));
    assert(!reader_cache_valid(&h, 19508131, 1234, bytes - 4));
    assert(!reader_cache_valid(&h, 19508131, 1234, bytes + 4));
    h.magic = 0;
    assert(!reader_cache_valid(&h, 19508131, 1234, bytes));
    h.magic = READER_CACHE_MAGIC;
    ++h.layout;
    assert(!reader_cache_valid(&h, 19508131, 1234, bytes));
    h.layout = READER_LAYOUT; h.pages = 0;
    assert(!reader_cache_valid(&h, 19508131, 1234, sizeof(h)));
    h.pages = UINT32_MAX;
    assert(!reader_cache_valid(&h, 19508131, 1234, bytes));
    assert(reader_hash(2166136261U, (const uint8_t*)"hello", 5) == 0x4f9f2cabU);
    puts("PASS: valid cache, changed book, incomplete/truncated cache, layout mismatch, overflow, hash vector");
}
