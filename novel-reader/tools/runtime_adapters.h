#define _GNU_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
#include "reader_core.h"
#include "reader_cache.h"
#define ROOT "appdata"
typedef int Storage;
typedef int FuriMutex;
typedef int FuriMessageQueue;
typedef int ViewPort;
typedef struct { char* text; } FuriString;
typedef struct { FILE* fp; bool book; } File;
typedef struct { float display_brightness; } Settings;
typedef struct { Settings settings; int locks; float effective; } NotificationApp;
typedef struct { int key, type; } InputEvent;
enum {FuriWaitForever, FuriStatusOk, FSE_OK, FSAM_WRITE, FSAM_READ, FSAM_READ_WRITE,
      FSOM_CREATE_ALWAYS, FSOM_OPEN_EXISTING, InputKeyBack, InputTypePress};
static const int sequence_display_backlight_enforce_auto=1;
static const int sequence_display_backlight_enforce_on=2;
static const int sequence_display_backlight_off=3;
static const int sequence_display_backlight_on=4;
static size_t book_bytes, writes;
static bool cancel_now;
static void notification_message_block(NotificationApp* n, const int* sequence) {
    if(*sequence == 1) { assert(n->locks > 0); --n->locks; }
    if(*sequence == 2) ++n->locks;
    if(*sequence == 3) n->effective = 0;
    if(*sequence == 4) n->effective = n->settings.display_brightness;
}
static void furi_mutex_acquire(FuriMutex* m, int timeout) {}
static void furi_mutex_release(FuriMutex* m) {}
static void view_port_update(ViewPort* v) {}
static int furi_message_queue_get(FuriMessageQueue* q, InputEvent* e, int timeout) {
    if(cancel_now) { cancel_now=false; e->key=InputKeyBack; e->type=InputTypePress; return FuriStatusOk; }
    return -1;
}
static File* storage_file_alloc(Storage* s) { return calloc(1, sizeof(File)); }
static void storage_file_free(File* f) { assert(!f->fp); free(f); }
static bool storage_file_open(File* f, const char* path, int mode, int how) {
    assert(!f->fp);
    f->fp=fopen(path, how == FSOM_CREATE_ALWAYS ? "w+b" : "rb");
    f->book = strcmp(path,"book.txt") == 0;
    return f->fp != NULL;
}
static bool storage_file_close(File* f) {
    if(!f->fp) return true;
    int error=fclose(f->fp); f->fp=NULL; return error==0;
}
static size_t storage_file_read(File* f, void* data, size_t count) {
    size_t n=fread(data,1,count,f->fp); if(f->book) book_bytes+=n; return n;
}
static size_t storage_file_write(File* f, const void* data, size_t count) {
    ++writes; return fwrite(data,1,count,f->fp);
}
static bool storage_file_seek(File* f, uint32_t at, bool absolute) {
    return fseek(f->fp,at,absolute ? SEEK_SET : SEEK_CUR)==0;
}
static uint32_t storage_file_tell(File* f) { return ftell(f->fp); }
static uint64_t storage_file_size(File* f) {
    long at=ftell(f->fp); fseek(f->fp,0,SEEK_END); long size=ftell(f->fp); fseek(f->fp,at,SEEK_SET); return size;
}
static bool storage_file_sync(File* f) { return fflush(f->fp)==0; }
static int storage_file_get_error(File* f) { return ferror(f->fp) ? -1 : FSE_OK; }
static FuriString* furi_string_alloc_printf(const char* format, ...) {
    FuriString* s=malloc(sizeof(*s)); va_list args; va_start(args,format);
    assert(vasprintf(&s->text,format,args)>=0); va_end(args); return s;
}
static const char* furi_string_get_cstr(FuriString* s) { return s->text; }
static void furi_string_free(FuriString* s) { free(s->text); free(s); }
