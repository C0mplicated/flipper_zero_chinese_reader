#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <notification/notification_app.h>
#include <notification/notification_messages.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "reader_core.h"
#include "reader_cache.h"
#include "novel_reader_icons.h"

#define ROOT "/ext/apps_data/novel_reader"
#define MAX_BOOK (64U * 1024U * 1024U)
typedef struct {
    Storage* storage;
    NotificationApp* notification;
    float saved_brightness;
    bool light_lock, brightness_override;
    uint8_t light_mode, menu_item;
    File *book, *font, *index;
    FuriMutex* mutex;
    FuriMessageQueue* queue;
    ViewPort* viewport;
    uint8_t buffer[4096];
    size_t at, used;
    uint32_t buffer_start;
    uint8_t pixels[84][24];
    ReaderPage page;
    uint32_t size, pages, current, offset, fingerprint;
    bool error, menu, invert;
    char status[64];
} Reader;

/* Pinned Momentum SDK: temporary brightness override, never persisted globally.
 * Notification on/auto locks are paired and released before the file picker. */
static void light_restore(Reader* r) {
    if(r->brightness_override) {
        r->notification->settings.display_brightness = r->saved_brightness;
        r->brightness_override = false;
    }
    if(r->light_lock) {
        notification_message_block(r->notification, &sequence_display_backlight_enforce_auto);
        r->light_lock = false;
    }
    notification_message_block(r->notification, &sequence_display_backlight_on);
}

static void light_apply(Reader* r) {
    if(r->brightness_override) {
        r->notification->settings.display_brightness = r->saved_brightness;
        r->brightness_override = false;
    }
    if(r->light_lock) {
        notification_message_block(r->notification, &sequence_display_backlight_enforce_auto);
        r->light_lock = false;
    }
    if(r->light_mode == 2) {
        r->saved_brightness = r->notification->settings.display_brightness;
        r->notification->settings.display_brightness = 0;
        r->brightness_override = true;
        notification_message_block(r->notification, &sequence_display_backlight_off);
    } else {
        if(r->light_mode == 1) {
            notification_message_block(r->notification, &sequence_display_backlight_enforce_on);
            r->light_lock = true;
        }
        notification_message_block(r->notification, &sequence_display_backlight_on);
    }
}

static bool preferences(Reader* r, bool write) {
    File* file = storage_file_alloc(r->storage);
    uint32_t data[3] = {0x3353524E, r->light_mode, r->invert};
    bool ok = storage_file_open(file, ROOT "/settings.bin", write ? FSAM_WRITE : FSAM_READ,
        write ? FSOM_CREATE_ALWAYS : FSOM_OPEN_EXISTING);
    if(ok && write) ok = storage_file_write(file, data, sizeof(data)) == sizeof(data) && storage_file_sync(file);
    if(ok && !write) {
        ok = storage_file_read(file, data, sizeof(data)) == sizeof(data) &&
            data[0] == 0x3353524E && data[1] <= 2 && data[2] <= 1;
        if(ok) { r->light_mode = data[1]; r->invert = data[2]; }
    }
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

static int get_byte(void* ctx) {
    Reader* r = ctx;
    if(r->at == r->used) {
        r->buffer_start = storage_file_tell(r->book);
        r->used = storage_file_read(r->book, r->buffer, sizeof(r->buffer));
        r->at = 0;
        if(!r->used) return -1;
    }
    return r->buffer[r->at++];
}

static bool parse(Reader* r, uint32_t offset, ReaderPage* page) {
    if(r->used && offset >= r->buffer_start && offset < r->buffer_start + r->used) {
        r->at = offset - r->buffer_start;
    } else {
        r->at = r->used = 0;
        if(!storage_file_seek(r->book, offset, true)) return false;
    }
    reader_page(get_byte, r, offset, page);
    return storage_file_get_error(r->book) == FSE_OK;
}

static void status(Reader* r, const char* s) {
    furi_mutex_acquire(r->mutex, FuriWaitForever);
    snprintf(r->status, sizeof(r->status), "%s", s);
    furi_mutex_release(r->mutex);
    view_port_update(r->viewport);
}

static void draw(Canvas* canvas, void* ctx) {
    Reader* r = ctx;
    furi_mutex_acquire(r->mutex, FuriWaitForever);
    canvas_clear(canvas);
    if(r->invert) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, 128, 64);
    }
    canvas_set_color(canvas, r->invert ? ColorWhite : ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    if(r->status[0]) {
        canvas_draw_str(canvas, 2, 18, r->status);
        canvas_draw_str(canvas, 2, 40, "Back: cancel / return");
    } else if(r->menu) {
        canvas_draw_str(canvas, 2, 9, "Reader 0.4 Final");
        const char* modes[] = {"Auto", "Always on", "Off"};
        char line[32];
        snprintf(line, sizeof(line), "Backlight: %s", modes[r->light_mode]);
        canvas_draw_str(canvas, 10, 20, line);
        canvas_draw_str(canvas, 10, 30, r->invert ? "Invert: On" : "Invert: Off");
        canvas_draw_str(canvas, 10, 40, "First page");
        canvas_draw_str(canvas, 10, 50, "Rebuild index");
        canvas_draw_str(canvas, 10, 60, "Return to book");
        canvas_draw_str(canvas, 1, 20 + r->menu_item * 10, ">");
    } else {
        for(size_t i = 0; i < r->page.count; ++i) {
            ReaderGlyph* g = &r->page.glyphs[i];
            if(g->code < 128) {
                canvas_set_font(canvas, FontKeyboard);
                char text[2] = {(char)g->code, 0};
                canvas_draw_str(canvas, g->x, g->y + 10, text);
            } else {
                for(unsigned y = 0; y < 12; ++y)
                    for(unsigned x = 0; x < 12; ++x)
                        if(r->pixels[i][y * 2 + x / 8] & (0x80 >> (x % 8)))
                            canvas_draw_dot(canvas, g->x + x, g->y + y);
            }
        }
        canvas_set_font(canvas, FontSecondary);
        char footer[40];
        snprintf(footer, sizeof(footer), "%lu/%lu %lu%%",
            (unsigned long)(r->current + 1), (unsigned long)r->pages,
            (unsigned long)((uint64_t)(r->current + 1) * 100 / r->pages));
        canvas_draw_str(canvas, 1, 63, footer);
    }
    furi_mutex_release(r->mutex);
}

static void input(InputEvent* event, void* ctx) {
    Reader* r = ctx;
    /* Never block the GUI thread on a full queue. */
    furi_message_queue_put(r->queue, event, 0);
}

static bool load_page(Reader* r, uint32_t number) {
    uint32_t offset;
    ReaderPage page;
    if(number >= r->pages || !storage_file_seek(r->index, sizeof(ReaderCacheHeader) + number * 4, true) ||
       storage_file_read(r->index, &offset, 4) != 4 || offset >= r->size ||
       !parse(r, offset, &page) || page.next <= offset) return false;
    furi_mutex_acquire(r->mutex, FuriWaitForever);
    r->page = page;
    r->current = number;
    r->offset = offset;
    memset(r->pixels, 0, sizeof(r->pixels));
    for(size_t i = 0; i < page.count; ++i) {
        uint32_t code = page.glyphs[i].code;
        if(code < 128) continue;
        bool found = code <= 65535 && storage_file_seek(r->font, 8 + code * 24, true) &&
            storage_file_read(r->font, r->pixels[i], 24) == 24;
        bool any = false;
        for(unsigned j = 0; j < 24; ++j) any |= r->pixels[i][j] != 0;
        if(!found || (!any && code != 0x3000)) {
            for(unsigned y = 0; y < 12; ++y) {
                r->pixels[i][y * 2] = (y == 0 || y == 11) ? 0xFF : 0x80;
                r->pixels[i][y * 2 + 1] = (y == 0 || y == 11) ? 0xF0 : 0x10;
            }
        }
    }
    r->status[0] = 0;
    furi_mutex_release(r->mutex);
    view_port_update(r->viewport);
    return true;
}

static uint32_t bookmark(Reader* r, const char* path, bool write) {
    FuriString* sidecar = furi_string_alloc_printf("%s.nrpos", path);
    File* f = storage_file_alloc(r->storage);
    uint32_t data[3] = {0x31524E42, r->size, r->offset};
    bool ok = storage_file_open(f, furi_string_get_cstr(sidecar),
        write ? FSAM_WRITE : FSAM_READ, write ? FSOM_CREATE_ALWAYS : FSOM_OPEN_EXISTING);
    if(ok) {
        if(write) ok = storage_file_write(f, data, sizeof(data)) == sizeof(data);
        else ok = storage_file_read(f, data, sizeof(data)) == sizeof(data) &&
            data[0] == 0x31524E42 && data[1] == r->size && data[2] < r->size;
    }
    storage_file_close(f);
    storage_file_free(f);
    furi_string_free(sidecar);
    if(write && !ok) status(r, "Bookmark save failed");
    return ok ? data[2] : 0;
}

static bool build_index(Reader* r, uint32_t resume, uint32_t* target) {
    ReaderCacheHeader header = {0};
    if(storage_file_write(r->index, &header, sizeof(header)) != sizeof(header)) return false;
    uint32_t offset = 0, batch[256];
    size_t count = 0;
    ReaderPage page;
    r->pages = 0;
    *target = 0;
    while(offset < r->size) {
        batch[count++] = offset;
        if(offset <= resume) *target = r->pages;
        ++r->pages;
        if(!parse(r, offset, &page) || page.next <= offset) return false;
        offset = page.next;
        if(count == 256 || offset >= r->size) {
            if(storage_file_write(r->index, batch, count * 4) != count * 4) return false;
            count = 0;
            char msg[64];
            snprintf(msg, sizeof(msg), "Indexing: %lu%%", (unsigned long)((uint64_t)offset * 100 / r->size));
            status(r, msg);
        }
        InputEvent event;
        while(furi_message_queue_get(r->queue, &event, 0) == FuriStatusOk)
            if(event.key == InputKeyBack && event.type == InputTypePress) return false;
    }
    if(!r->pages || !storage_file_sync(r->index)) return false;
    /* Publish the ready marker only after all offsets have reached storage. */
    header = (ReaderCacheHeader){READER_CACHE_MAGIC, READER_LAYOUT, r->size, r->fingerprint, r->pages};
    return storage_file_seek(r->index, 0, true) &&
        storage_file_write(r->index, &header, sizeof(header)) == sizeof(header) &&
        storage_file_sync(r->index);
}

static bool fingerprint(Reader* r) {
    uint32_t hash = 2166136261U;
    uint32_t count = r->size < sizeof(r->buffer) ? r->size : sizeof(r->buffer);
    uint32_t offsets[3] = {0, (r->size - count) / 2, r->size - count};
    for(unsigned i = 0; i < 3; ++i) {
        if(!storage_file_seek(r->book, offsets[i], true) ||
           storage_file_read(r->book, r->buffer, count) != count) return false;
        hash = reader_hash(hash, r->buffer, count);
    }
    r->at = r->used = 0;
    r->fingerprint = hash;
    return true;
}

static bool index_offset(Reader* r, uint32_t page, uint32_t* offset) {
    return page < r->pages && storage_file_seek(r->index, sizeof(ReaderCacheHeader) + page * 4, true) &&
        storage_file_read(r->index, offset, 4) == 4 && *offset < r->size;
}

static bool resume_index(Reader* r, uint32_t resume, uint32_t* target) {
    ReaderCacheHeader h;
    if(storage_file_read(r->index, &h, sizeof(h)) != sizeof(h) ||
       !reader_cache_valid(&h, r->size, r->fingerprint, storage_file_size(r->index))) return false;
    r->pages = h.pages;
    uint32_t first, last;
    if(!index_offset(r, 0, &first) || first != 0 ||
       !index_offset(r, r->pages - 1, &last)) return false;
    uint32_t lo = 0, hi = r->pages;
    while(lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2, offset;
        if(!index_offset(r, mid, &offset)) return false;
        if(offset <= resume) lo = mid + 1;
        else hi = mid;
    }
    *target = lo ? lo - 1 : 0;
    uint32_t here, next;
    if(!index_offset(r, *target, &here) || here > resume) return false;
    if(*target + 1 < r->pages &&
       (!index_offset(r, *target + 1, &next) || next <= here || next <= resume)) return false;
    return true;
}

static bool prepare_index(Reader* r, const char* name, uint32_t resume, uint32_t* target, bool rebuild) {
    status(r, rebuild ? "Rebuilding index..." : "Opening book...");
    if(!fingerprint(r)) return false;
    FuriString* path = furi_string_alloc_printf("%s.nridx", name);
    bool ok = false;
    if(!rebuild && storage_file_open(r->index, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING))
        ok = resume_index(r, resume, target);
    if(!ok) {
        storage_file_close(r->index);
        status(r, "Indexing... Back: cancel");
        if(storage_file_open(r->index, furi_string_get_cstr(path), FSAM_READ_WRITE, FSOM_CREATE_ALWAYS))
            ok = build_index(r, resume, target);
    }
    furi_string_free(path);
    return ok;
}

int32_t novel_reader_app(void* arg) {
    UNUSED(arg);
    Reader* r = calloc(1, sizeof(Reader));
    r->storage = furi_record_open(RECORD_STORAGE);
    r->notification = furi_record_open(RECORD_NOTIFICATION);
    Gui* gui = furi_record_open(RECORD_GUI);
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    r->book = storage_file_alloc(r->storage);
    r->font = storage_file_alloc(r->storage);
    r->index = storage_file_alloc(r->storage);
    r->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    r->queue = furi_message_queue_alloc(16, sizeof(InputEvent));
    r->viewport = view_port_alloc();
    view_port_draw_callback_set(r->viewport, draw, r);
    view_port_input_callback_set(r->viewport, input, r);
    snprintf(r->status, sizeof(r->status), "Reader 0.4 Final");
    storage_common_mkdir(r->storage, ROOT);
    storage_common_mkdir(r->storage, ROOT "/books");
    preferences(r, false);
    gui_add_view_port(gui, r->viewport, GuiLayerFullscreen);
    uint8_t header[8];
    bool font_ok = storage_file_open(r->font, ROOT "/font12.bin", FSAM_READ, FSOM_OPEN_EXISTING) &&
        storage_file_read(r->font, header, 8) == 8 && !memcmp(header, "NRF12V1\0", 8) &&
        storage_file_size(r->font) == 8 + 65536U * 24;
    FuriString* path = furi_string_alloc_set(ROOT "/books");
    if(!font_ok) {
        status(r, "Missing/bad font12.bin");
        InputEvent e;
        do { furi_message_queue_get(r->queue, &e, FuriWaitForever); }
        while(e.key != InputKeyBack || e.type != InputTypeShort);
    }
    while(font_ok) {
        view_port_enabled_set(r->viewport, false);
        DialogsFileBrowserOptions opts;
        dialog_file_browser_set_basic_options(&opts, ".txt", &I_book);
        opts.base_path = ROOT "/books";
        bool chosen = dialog_file_browser_show(dialogs, path, path, &opts);
        view_port_enabled_set(r->viewport, true);
        if(!chosen) break;
        InputEvent e;
        while(furi_message_queue_get(r->queue, &e, 0) == FuriStatusOk) {}
        const char* name = furi_string_get_cstr(path);
        bool ok = storage_file_open(r->book, name, FSAM_READ, FSOM_OPEN_EXISTING);
        r->at = r->used = 0;
        uint64_t size = ok ? storage_file_size(r->book) : 0;
        r->size = (uint32_t)size;
        uint32_t target = 0;
        ok = ok && size > 0 && size <= MAX_BOOK;
        if(ok) ok = prepare_index(r, name, bookmark(r, name, false), &target, false);
        if(ok && !load_page(r, target)) {
            storage_file_close(r->index);
            ok = prepare_index(r, name, bookmark(r, name, false), &target, true) && load_page(r, target);
        }
        if(!ok) {
            status(r, "Open failed/cancelled");
            do { furi_message_queue_get(r->queue, &e, FuriWaitForever); }
            while(e.key != InputKeyBack || e.type != InputTypeShort);
        } else {
            light_apply(r);
            furi_mutex_acquire(r->mutex, FuriWaitForever);
            r->menu = false;
            furi_mutex_release(r->mutex);
            while(true) {
                if(furi_message_queue_get(r->queue, &e, FuriWaitForever) != FuriStatusOk) continue;
                if(e.key == InputKeyBack && e.type == InputTypeLong) break;
                if(e.type != InputTypeShort && e.type != InputTypeRepeat) continue;
                if(e.key == InputKeyBack) {
                    if(!r->menu) break;
                    furi_mutex_acquire(r->mutex, FuriWaitForever);
                    r->menu = false;
                    furi_mutex_release(r->mutex);
                } else if(!r->menu && e.key == InputKeyOk) {
                    furi_mutex_acquire(r->mutex, FuriWaitForever);
                    r->menu = true;
                    r->menu_item = 0;
                    furi_mutex_release(r->mutex);
                } else if(r->menu) {
                    bool save = false;
                    if(e.key == InputKeyUp || e.key == InputKeyDown) {
                        furi_mutex_acquire(r->mutex, FuriWaitForever);
                        r->menu_item = (r->menu_item + (e.key == InputKeyUp ? 4 : 1)) % 5;
                        furi_mutex_release(r->mutex);
                    } else if(e.type == InputTypeShort &&
                              (e.key == InputKeyOk || e.key == InputKeyLeft || e.key == InputKeyRight)) {
                        if(r->menu_item == 0) {
                            furi_mutex_acquire(r->mutex, FuriWaitForever);
                            r->light_mode = (r->light_mode + (e.key == InputKeyLeft ? 2 : 1)) % 3;
                            furi_mutex_release(r->mutex);
                            light_apply(r);
                            save = true;
                        } else if(r->menu_item == 1) {
                            furi_mutex_acquire(r->mutex, FuriWaitForever);
                            r->invert = !r->invert;
                            furi_mutex_release(r->mutex);
                            save = true;
                        } else if(e.key == InputKeyOk) {
                            if(r->menu_item == 2) {
                                if(!load_page(r, 0)) { status(r, "Read failed"); break; }
                                bookmark(r, name, true);
                            } else if(r->menu_item == 3) {
                                uint32_t resume = r->offset;
                                storage_file_close(r->index);
                                if(!prepare_index(r, name, resume, &target, true) || !load_page(r, target)) {
                                    status(r, "Rebuild cancelled/failed");
                                    break;
                                }
                            }
                            furi_mutex_acquire(r->mutex, FuriWaitForever);
                            r->menu = false;
                            furi_mutex_release(r->mutex);
                        }
                    }
                    if(save && !preferences(r, true)) status(r, "Settings save failed");
                } else {
                    int32_t next = (int32_t)r->current;
                    if(e.key == InputKeyUp) --next;
                    else if(e.key == InputKeyDown) ++next;
                    else if(e.key == InputKeyLeft) next -= 5;
                    else if(e.key == InputKeyRight) next += 5;
                    if(next < 0) next = 0;
                    if((uint32_t)next >= r->pages) next = r->pages - 1;
                    if((uint32_t)next != r->current) {
                        if(!load_page(r, next)) { status(r, "Read failed"); break; }
                        bookmark(r, name, true);
                    }
                }
                view_port_update(r->viewport);
            }
            bookmark(r, name, true);
            light_restore(r);
        }
        storage_file_close(r->book);
        storage_file_close(r->index);
        status(r, "Select a book");
    }
    view_port_enabled_set(r->viewport, false);
    gui_remove_view_port(gui, r->viewport);
    view_port_free(r->viewport);
    light_restore(r);
    storage_file_close(r->font);
    storage_file_free(r->book);
    storage_file_free(r->font);
    storage_file_free(r->index);
    furi_string_free(path);
    furi_message_queue_free(r->queue);
    furi_mutex_free(r->mutex);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    free(r);
    return 0;
}
