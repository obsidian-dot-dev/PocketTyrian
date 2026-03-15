#include "libc.h"
#include <stdbool.h>
#include "../dataslot.h"
#include "../terminal.h"

/* Standard file descriptors */
static FILE stdin_file = {0, 0, 0, 0, NULL};
static FILE stdout_file = {0, 0, 0, 0, NULL};
static FILE stderr_file = {0, 0, 0, 0, NULL};

FILE *stdin = &stdin_file;
FILE *stdout = &stdout_file;
FILE *stderr = &stderr_file;

/* File table for open files */
#define MAX_OPEN_FILES 8
static FILE file_table[MAX_OPEN_FILES];
static int file_table_used[MAX_OPEN_FILES] = {0};

/* ROM Filesystem - Preloaded to SDRAM by tyrian_main */
#define ROMFS_SDRAM_BASE 0x11000000

/* Consolidated Nonvolatile Storage (Slot 5)
 * Total Size: 64KB (0x10000)
 * Address: 0x30000000 (CRAM1 - Matches data.json Slot 5)
 * PSRAM is UNCACHED in this core, so no flush_dcache is needed for these.
 */
#define SLOT_SAVES_ADDR    0x30000000

#define CFG_LEGACY_ADDR (SLOT_SAVES_ADDR + 0x0000)
#define CFG_LEGACY_SIZE 64

#define CFG_MODERN_ADDR (SLOT_SAVES_ADDR + 0x1000)
#define CFG_MODERN_SIZE 4096

#define SAV_DATA_ADDR   (SLOT_SAVES_ADDR + 0x2000)
#define SAV_DATA_SIZE   8192

typedef struct {
    char name[32];
    uint32_t offset;
    uint32_t size;
} rom_entry_t;

typedef struct {
    char magic[8];
    uint32_t num_files;
} rom_header_t;

static rom_entry_t *rom_entries = NULL;
static uint32_t rom_num_files = 0;

void rom_fs_init(void) {
    if (rom_entries) return;
    rom_header_t *header = (rom_header_t *)ROMFS_SDRAM_BASE;
    if (header && memcmp(header->magic, "TYRIANRM", 8) == 0) {
        rom_num_files = header->num_files;
        rom_entries = (rom_entry_t *)(ROMFS_SDRAM_BASE + sizeof(rom_header_t));
    }
}

static FILE *alloc_file(void) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!file_table_used[i]) {
            file_table_used[i] = 1;
            memset(&file_table[i], 0, sizeof(FILE));
            return &file_table[i];
        }
    }
    return NULL;
}

static void free_file(FILE *f) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (&file_table[i] == f) {
            file_table_used[i] = 0;
            return;
        }
    }
}

static const char *path_basename(const char *pathname) {
    const char *last = pathname;
    while (*pathname) {
        if (*pathname == '/' || *pathname == '\\') last = pathname + 1;
        pathname++;
    }
    return last;
}

/* VexRiscv D-Cache hard flush */
#define CACHE_FLUSH_ADDR    0x10380000
#define CACHE_FLUSH_SIZE    (128 * 1024)

static void flush_dcache(void) {
    volatile uint32_t *p = (volatile uint32_t *)CACHE_FLUSH_ADDR;
    volatile uint32_t sink;
    for (int i = 0; i < (int)(CACHE_FLUSH_SIZE / 4); i++) {
        sink = p[i];
    }
    (void)sink;
}

static bool is_region_empty(void *addr) {
    uint32_t *p = (uint32_t *)addr;
    /* Check first 64 bytes */
    for (int i = 0; i < 16; i++) {
        if (p[i] != 0) return false;
    }
    return true;
}

FILE *fopen(const char *pathname, const char *mode) {
    if (!rom_entries) rom_fs_init();
    const char *base = path_basename(pathname);
    bool is_write = (mode[0] == 'w');
    
    /* 1. tyrian.cfg */
    if (strcasecmp(base, "tyrian.cfg") == 0) {
        if (!is_write && is_region_empty((void*)CFG_LEGACY_ADDR)) return NULL;
        FILE *f = alloc_file();
        if (!f) return NULL;
        f->data = (void *)CFG_LEGACY_ADDR;
        f->offset = 0;
        f->size = is_write ? 0 : CFG_LEGACY_SIZE;
        f->flags = 0x40000000 | (is_write ? 0x80000000 : 0);
        return f;
    }

    /* 2. opentyrian.cfg */
    if (strcasecmp(base, "opentyrian.cfg") == 0) {
        if (!is_write && is_region_empty((void*)CFG_MODERN_ADDR)) return NULL;
        FILE *f = alloc_file();
        if (!f) return NULL;
        f->data = (void *)CFG_MODERN_ADDR;
        f->offset = 0;
        f->size = is_write ? 0 : CFG_MODERN_SIZE;
        f->flags = 0x40000000 | (is_write ? 0x80000000 : 0);
        return f;
    }

    /* 3. tyrian.sav */
    if (strcasecmp(base, "tyrian.sav") == 0) {
        if (!is_write && is_region_empty((void*)SAV_DATA_ADDR)) return NULL;
        FILE *f = alloc_file();
        if (!f) return NULL;
        f->data = (void *)SAV_DATA_ADDR;
        f->offset = 0;
        f->size = is_write ? 0 : SAV_DATA_SIZE;
        f->flags = 0x40000000 | (is_write ? 0x80000000 : 0);
        return f;
    }

    /* 4. ROM Assets */
    if (rom_entries) {
        for (uint32_t i = 0; i < rom_num_files; i++) {
            if (strcasecmp(rom_entries[i].name, base) == 0) {
                FILE *f = alloc_file();
                if (!f) return NULL;
                f->offset = 0;
                f->size = rom_entries[i].size;
                f->data = (void *)(ROMFS_SDRAM_BASE + rom_entries[i].offset);
                f->flags = 0;
                return f;
            }
        }
    }

    return NULL;
}

int fclose(FILE *stream) {
    if (!stream) return 0;
    /* PSRAM is uncached, so no flush_dcache needed for RAMFS.
     * Only SDRAM framebuffers etc need it. */
    free_file(stream);
    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!stream) return 0;
    size_t count = size * nmemb;
    if (stream->offset + count > stream->size) count = stream->size - stream->offset;
    if (count <= 0) return 0;
    memcpy(ptr, (uint8_t *)stream->data + stream->offset, count);
    stream->offset += count;
    return count / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!stream || !(stream->flags & 0x80000000)) return 0;
    size_t count = size * nmemb;
    
    uint32_t max_size = 0;
    uint32_t addr = (uint32_t)stream->data;
    if (addr == CFG_LEGACY_ADDR) max_size = CFG_LEGACY_SIZE;
    else if (addr == CFG_MODERN_ADDR) max_size = CFG_MODERN_SIZE;
    else if (addr == SAV_DATA_ADDR) max_size = SAV_DATA_SIZE;

    if (stream->offset + count > max_size) count = max_size - stream->offset;
    if (count <= 0) return 0;

    memcpy((uint8_t *)stream->data + stream->offset, ptr, count);
    stream->offset += count;
    if (stream->offset > stream->size) stream->size = stream->offset;
    return count / size;
}

int fseek(FILE *stream, long offset, int whence) {
    if (!stream) return -1;
    long new_pos;
    switch (whence) {
        case SEEK_SET: new_pos = offset; break;
        case SEEK_CUR: new_pos = stream->offset + offset; break;
        case SEEK_END: new_pos = (long)stream->size + offset; break;
        default: return -1;
    }
    if (new_pos < 0 || (uint32_t)new_pos > stream->size) return -1;
    stream->offset = (uint32_t)new_pos;
    return 0;
}

long ftell(FILE *stream) { return stream ? (long)stream->offset : -1; }
int feof(FILE *stream) { return stream ? (stream->offset >= stream->size) : 1; }
int ferror(FILE *stream) { (void)stream; return 0; }
int fflush(FILE *stream) {
    (void)stream;
    return 0;
}

/* Formatted I/O */
int vfprintf(FILE *stream, const char *format, va_list args) {
    if (stream == stdout || stream == stderr) {
        vterm_printf(format, args);
        return 0;
    }
    if (!stream || !(stream->flags & 0x80000000)) return 0;
    char buf[512];
    int len = vsnprintf(buf, sizeof(buf), format, args);
    return (int)fwrite(buf, 1, len, stream);
}

int fprintf(FILE *stream, const char *format, ...) {
    va_list args; va_start(args, format);
    int result = vfprintf(stream, format, args);
    va_end(args); return result;
}

#undef printf
int printf(const char *format, ...) {
    va_list args; va_start(args, format);
    vterm_printf(format, args);
    va_end(args); return 0;
}

int sprintf(char *str, const char *format, ...) {
    va_list args; va_start(args, format);
    int result = vsnprintf(str, 0x7FFFFFFF, format, args);
    va_end(args); return result;
}

int vsprintf(char *str, const char *format, va_list args) { return vsnprintf(str, 0x7FFFFFFF, format, args); }
int snprintf(char *str, size_t size, const char *format, ...) {
    va_list args; va_start(args, format);
    int result = vsnprintf(str, size, format, args);
    va_end(args); return result;
}

int vsnprintf(char *str, size_t size, const char *format, va_list args) {
    char *out = str; char *end = str + size - 1; const char *p = format;
    if (size == 0) return 0;
    while (*p && (str == NULL || out < end)) {
        if (*p == '%') {
            p++; int left_align = 0; char pad_char = ' ';
            if (*p == '-') { left_align = 1; p++; }
            if (*p == '0') { pad_char = '0'; p++; }
            int width = 0; while (*p >= '0' && *p <= '9') { width = width * 10 + (*p - '0'); p++; }
            int precision = -1; if (*p == '.') { p++; precision = 0; while (*p >= '0' && *p <= '9') { precision = precision * 10 + (*p - '0'); p++; } }
            int is_long = 0; if (*p == 'l') { is_long = 1; p++; }
            switch (*p) {
                case 'd':
                case 'i': {
                    long val = is_long ? va_arg(args, long) : (long)va_arg(args, int);
                    char buf[20]; int i = 0, neg = 0;
                    if (val < 0) { neg = 1; val = -val; }
                    do { buf[i++] = '0' + (val % 10); val /= 10; } while (val > 0);
                    while (i < precision) buf[i++] = '0';
                    int len = i + neg;
                    if (str) {
                        if (!left_align) while (len < width && out < end) { *out++ = pad_char; len++; }
                        if (neg && out < end) *out++ = '-';
                        while (i > 0 && out < end) *out++ = buf[--i];
                        if (left_align) while (len < width && out < end) { *out++ = ' '; len++; }
                    } else { out += (len < width) ? width : len; }
                    break;
                }
                case 'u': {
                    unsigned long val = is_long ? va_arg(args, unsigned long) : (unsigned long)va_arg(args, unsigned int);
                    char buf[20]; int i = 0;
                    do { buf[i++] = '0' + (val % 10); val /= 10; } while (val > 0);
                    while (i < precision) buf[i++] = '0';
                    int len = i;
                    if (str) { if (!left_align) while (len < width && out < end) { *out++ = pad_char; len++; } while (i > 0 && out < end) *out++ = buf[--i]; } else { out += (len < width) ? width : len; }
                    break;
                }
                case 'x':
                case 'X': {
                    unsigned long val = is_long ? va_arg(args, unsigned long) : (unsigned long)va_arg(args, unsigned int);
                    const char *hex = (*p == 'x') ? "0123456789abcdef" : "0123456789ABCDEF";
                    char buf[16]; int i = 0;
                    do { buf[i++] = hex[val & 0xF]; val >>= 4; } while (val > 0);
                    while (i < precision) buf[i++] = '0';
                    int len = i;
                    if (str) { if (!left_align) while (len < width && out < end) { *out++ = pad_char; len++; } while (i > 0 && out < end) *out++ = buf[--i]; } else { out += (len < width) ? width : len; }
                    break;
                }
                case 's': {
                    const char *s = va_arg(args, const char *);
                    if (!s) s = "(null)";
                    int len = strlen(s);
                    if (str) {
                        if (!left_align) while (len < width && out < end) { *out++ = ' '; width--; }
                        while (*s && out < end) *out++ = *s++;
                        if (left_align) while (len < width && out < end) { *out++ = ' '; len++; }
                    } else { out += (len < width) ? width : len; }
                    break;
                }
                case 'c': { char c = (char)va_arg(args, int); if (str) { if (out < end) *out++ = c; } else { out++; } break; }
                default: if (str && out < end) *out++ = *p; else out++; break;
            }
        } else { if (str && out < end) *out++ = *p; else out++; }
        p++;
    }
    if (str) *out = '\0';
    return out - str;
}

int sscanf(const char *str, const char *format, ...) { (void)str; (void)format; return 0; }
int fscanf(FILE *stream, const char *format, ...) { (void)stream; (void)format; return 0; }
int fgetc(FILE *stream) { uint8_t c; if (fread(&c, 1, 1, stream) != 1) return EOF; return c; }
int getc(FILE *stream) { return fgetc(stream); }
int fputc(int c, FILE *stream) { uint8_t val = (uint8_t)c; if (fwrite(&val, 1, 1, stream) != 1) return EOF; return c; }
int fputs(const char *s, FILE *stream) { int len = strlen(s); if (fwrite(s, 1, len, stream) != (size_t)len) return EOF; return 0; }
void setbuf(FILE *stream, char *buf) { (void)stream; (void)buf; }
int unlink(const char *pathname) { (void)pathname; return -1; }
