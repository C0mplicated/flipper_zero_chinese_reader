#pragma once
#include <stdint.h>
#include <stdbool.h>

#define READER_CACHE_MAGIC 0x3349524EU
#define READER_LAYOUT 2U
typedef struct {
    uint32_t magic, layout, size, fingerprint, pages;
} ReaderCacheHeader;

static inline uint32_t reader_hash(uint32_t hash, const uint8_t* data, uint32_t size) {
    for(uint32_t i = 0; i < size; ++i) hash = (hash ^ data[i]) * 16777619U;
    return hash;
}

static inline bool reader_cache_valid(
    const ReaderCacheHeader* h, uint32_t size, uint32_t fingerprint, uint64_t disk_size) {
    return h->magic == READER_CACHE_MAGIC && h->layout == READER_LAYOUT &&
        h->size == size && h->fingerprint == fingerprint && h->pages > 0 &&
        h->pages <= size && disk_size == sizeof(*h) + (uint64_t)h->pages * 4;
}
