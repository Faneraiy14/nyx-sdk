#ifndef NYXC_H
#define NYXC_H

/* nyxc.h — мінімальна "libc" для user-програм поверх libnyx.h: рядки,
 * malloc/free (поверх sbrk), printf. Лише заголовок - програми поки
 * збираються одним файлом. Функції static і позначені unused, щоб
 * невикористані не давали попереджень. */

#include <stdarg.h>
#include "libnyx.h"

#define NYXC_FN static __attribute__((unused))

#ifndef NULL
#define NULL ((void*) 0)
#endif

/* ---- рядки й пам'ять -------------------------------------------- */

NYXC_FN int strlen(const char* s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

NYXC_FN int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char) *a - (unsigned char) *b;
}

NYXC_FN int strncmp(const char* a, const char* b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i] || !a[i]) return (unsigned char) a[i] - (unsigned char) b[i];
    }
    return 0;
}

NYXC_FN char* strcpy(char* dst, const char* src) {
    char* d = dst;
    while ((*d++ = *src++)) { }
    return dst;
}

NYXC_FN char* strchr(const char* s, int c) {
    for (; *s; s++) if (*s == (char) c) return (char*) s;
    return c == 0 ? (char*) s : 0;
}

NYXC_FN char* strstr(const char* hay, const char* needle) {
    int n = strlen(needle);
    for (; *hay; hay++) {
        if (strncmp(hay, needle, n) == 0) return (char*) hay;
    }
    return n == 0 ? (char*) hay : NULL;
}

NYXC_FN void* memcpy(void* dst, const void* src, int n) {
    unsigned char* d = dst;
    const unsigned char* s = src;
    for (int i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

NYXC_FN void* memset(void* dst, int v, int n) {
    unsigned char* d = dst;
    for (int i = 0; i < n; i++) d[i] = (unsigned char) v;
    return dst;
}

NYXC_FN int atoi(const char* s) {
    int sign = 1, v = 0;
    while (*s == ' ') s++;
    if (*s == '-') { sign = -1; s++; }
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    return sign * v;
}

/* ---- malloc/free ------------------------------------------------
 * Список блоків у порядку адрес; free позначає блок вільним і зливає
 * всі сусідні вільні блоки в списку (інакше звільнення по порядку не
 * зливалось би ніколи - сусід праворуч ще зайнятий), malloc бере перший вільний, що
 * підходить, або розширює купу через sbrk. */

typedef struct nyxc_block {
    unsigned size; /* корисний розмір, байтів */
    int free;
    struct nyxc_block* next;
} nyxc_block_t;

static nyxc_block_t* nyxc_heap = NULL;

NYXC_FN void* malloc(int size) {
    if (size <= 0) return NULL;
    unsigned need = ((unsigned) size + 7u) & ~7u;
    nyxc_block_t* last = NULL;
    for (nyxc_block_t* b = nyxc_heap; b; b = b->next) {
        if (b->free && b->size >= need) {
            if (b->size >= need + sizeof(nyxc_block_t) + 16) {
                nyxc_block_t* rest = (nyxc_block_t*) ((char*) (b + 1) + need);
                rest->size = b->size - need - sizeof(nyxc_block_t);
                rest->free = 1;
                rest->next = b->next;
                b->next = rest;
                b->size = need;
            }
            b->free = 0;
            return b + 1;
        }
        last = b;
    }
    nyxc_block_t* b = nyx_sbrk((int) (sizeof(nyxc_block_t) + need));
    if (b == (void*) -1) return NULL;
    b->size = need;
    b->free = 0;
    b->next = NULL;
    if (last) last->next = b; else nyxc_heap = b;
    return b + 1;
}

NYXC_FN void free(void* p) {
    if (!p) return;
    nyxc_block_t* b = (nyxc_block_t*) p - 1;
    b->free = 1;
    for (b = nyxc_heap; b; b = b->next) {
        while (b->free && b->next && b->next->free) {
            b->size += sizeof(nyxc_block_t) + b->next->size;
            b->next = b->next->next;
        }
    }
}

NYXC_FN void* realloc(void* p, int size) {
    if (!p) return malloc(size);
    nyxc_block_t* b = (nyxc_block_t*) p - 1;
    if (b->size >= (unsigned) size) return p;
    void* n = malloc(size);
    if (n) { memcpy(n, p, (int) b->size); free(p); }
    return n;
}

/* ---- вивід ------------------------------------------------------ */

NYXC_FN int puts(const char* s) {
    nyx_write(1, s, strlen(s));
    nyx_write(1, "\n", 1);
    return 0;
}

NYXC_FN int putchar(int c) {
    char ch = (char) c;
    nyx_write(1, &ch, 1);
    return c;
}

/* vsnprintf: %d %i %u %x %X %c %s %p %%, ширина й '-'/'0' (напр. %-8s, %08x). */
NYXC_FN int vsnprintf(char* out, int cap, const char* fmt, va_list ap) {
    int n = 0;
#define NYXC_PUT(ch) do { if (n + 1 < cap) out[n] = (ch); n++; } while (0)
    for (; *fmt; fmt++) {
        if (*fmt != '%') { NYXC_PUT(*fmt); continue; }
        fmt++;
        int left = 0, zero = 0, width = 0;
        if (*fmt == '-') { left = 1; fmt++; }
        if (*fmt == '0') { zero = 1; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        char tmp[16];
        const char* s = tmp;
        int len = 0;
        switch (*fmt) {
        case 'd': case 'i': case 'u': case 'x': case 'X': case 'p': {
            unsigned v;
            int neg = 0;
            unsigned base = (*fmt == 'x' || *fmt == 'X' || *fmt == 'p') ? 16 : 10;
            if (*fmt == 'd' || *fmt == 'i') {
                int sv = va_arg(ap, int);
                neg = sv < 0;
                v = neg ? (unsigned) -sv : (unsigned) sv;
            } else {
                v = va_arg(ap, unsigned);
            }
            const char* digits = *fmt == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
            char rev[12];
            int r = 0;
            do { rev[r++] = digits[v % base]; v /= base; } while (v);
            if (neg) tmp[len++] = '-';
            while (r) tmp[len++] = rev[--r];
            break;
        }
        case 'c': tmp[0] = (char) va_arg(ap, int); len = 1; break;
        case 's': s = va_arg(ap, const char*); if (!s) s = "(null)"; len = strlen(s); break;
        case '%': tmp[0] = '%'; len = 1; break;
        case '\0': fmt--; continue;
        default: tmp[0] = '%'; tmp[1] = *fmt; len = 2; break;
        }
        int pad = width > len ? width - len : 0;
        if (!left) while (pad-- > 0) NYXC_PUT(zero ? '0' : ' ');
        for (int i = 0; i < len; i++) NYXC_PUT(s[i]);
        if (left) while (pad-- > 0) NYXC_PUT(' ');
    }
    if (cap > 0) out[n < cap ? n : cap - 1] = '\0';
#undef NYXC_PUT
    return n;
}

NYXC_FN int snprintf(char* out, int cap, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(out, cap, fmt, ap);
    va_end(ap);
    return n;
}

NYXC_FN int printf(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    nyx_write(1, buf, n < (int) sizeof(buf) ? n : (int) sizeof(buf) - 1);
    return n;
}

/* Рядок зі stdin (до Enter, без '\n'). Повертає довжину або -1. */
NYXC_FN int readline(char* buf, int cap) {
    int n = nyx_read(0, buf, cap - 1);
    if (n < 0) return -1;
    if (n > 0 && buf[n - 1] == '\n') n--;
    buf[n] = '\0';
    return n;
}

/* Домашня тека користувача, що увійшов: "/home/<ім'я>" (з whoami, не з поточної теки - її міняє cd).
 * 0 - нікого нема (живий режим без диска, root) - тоді налаштування живуть до перезавантаження. */
NYXC_FN int nyx_home(char* out, int cap) {
    char who[64];
    out[0] = '\0';
    if (nyx_syscmd("whoami", who, sizeof(who)) <= 0) return 0;
    for (char* p = who; *p; p++) if (*p == '\n') *p = '\0';
    if (!who[0] || !strcmp(who, "root") || strchr(who, '/')) return 0;
    snprintf(out, cap, "/home/%s", who);
    return 1;
}

#endif
