/* libnyxgame.h — маленька бібліотека для ігор на NyxOS (J1, 20.11.2026).
 * Лише заголовок, як nyxc.h. Поверх NyxGame API ядра (libnyx.h):
 * nyx_present (кадр на весь екран, ціле збільшення), nyx_get_ms (час),
 * nyx_key_event/nyx_key_down/nyx_mouse (ввід).
 *
 * Як користуватись:
 *     ng_t g;
 *     if (ng_open(&g, 320, 180, 60) != 0) return 1;     // кадр 320x180, 60 кадрів/с
 *     while (!ng_key_down(NYX_KEY_ESC)) {
 *         ng_clear(&g, 0x101830);
 *         ng_rect(&g, 10, 10, 40, 30, 0xE02020);
 *         ng_text(&g, 10, 150, "Привіт, NyxOS!", 0xFFFFFF, 1);
 *         ng_present(&g);                                 // показати й дочекатися наступного кадру
 *     }
 *     ng_close(&g);
 *
 * Кольори — 0x00RRGGBB. Усе малювання обрізається краями кадру (за межі не пише).
 * Текст — згладжений шрифт NyxOS (Inter, /system/fonts; Е20) або запасний 8x16 ядра; ng_text_pixel - завжди 8x16
 * (піксель-арт), ng_text_mono - сітка 8x16 для терміналів. */
#ifndef NYXOS_LIBNYXGAME_H
#define NYXOS_LIBNYXGAME_H

#include "nyxc.h"
#include "nyx_font8x16.h"      /* власний піксельний шрифт SDK (з JetBrains Mono) */

#define NG_FN static __attribute__((unused))

typedef struct {
    int w, h;            /* розмір кадру в пікселях */
    nyx_u32* px;         /* пікселі кадру: px[y * w + x] */
    unsigned fps;        /* бажана частота кадрів (0 - без обмеження) */
    unsigned t0;         /* мс на старті */
    unsigned frames;     /* скільки кадрів показано */
    unsigned last_ms;    /* час минулого ng_present */
    unsigned dt;         /* мс між двома останніми кадрами (для руху: швидкість * dt) */
    int scale;           /* у скільки разів ядро збільшило кадр (0 - ще не показували) */
} ng_t;

/* Відкрити кадр w x h з fps кадрами на секунду (0 - без обмеження). 0 - ок, -1 - нема графіки чи пам'яті. */
NG_FN int ng_open(ng_t* g, int w, int h, int fps) {
    int sw, sh;
    nyx_screen_size(&sw, &sh);
    if (sw == 0 || w <= 0 || h <= 0 || w > sw || h > sh) return -1;
    g->px = (nyx_u32*) malloc((unsigned) (w * h * 4));
    if (!g->px) return -1;
    g->w = w;
    g->h = h;
    g->fps = fps > 0 ? (unsigned) fps : 0;
    g->t0 = g->last_ms = nyx_get_ms();
    g->frames = 0;
    g->dt = 0;
    g->scale = 0;
    return 0;
}

/* Кадр без екрана - для вікна програми (Р4.2): малюєш тими самими ng_*, показуєш nyx_win_present(id, g.px). */
NG_FN int ng_buffer(ng_t* g, int w, int h) {
    g->px = (nyx_u32*) malloc((unsigned) (w * h * 4));
    if (!g->px) return -1;
    g->w = w;
    g->h = h;
    g->fps = 0;
    g->t0 = g->last_ms = nyx_get_ms();
    g->frames = 0;
    g->dt = 0;
    g->scale = 0;
    return 0;
}

NG_FN void ng_close(ng_t* g) {
    free(g->px);
    g->px = 0;
}

/* Показати кадр на весь екран і дочекатися часу наступного (вирівнювання за годинником, без
 * накопичення похибки - J1.4). Повертає масштаб збільшення (або -1). */
NG_FN int ng_present(ng_t* g) {
    g->scale = nyx_present(g->px, g->w, g->h);
    g->frames++;
    if (g->fps) {
        /* без 64-бітного ділення (у програмах нема libgcc): frames*1000 вистачає на ~20 год при 60 кадр/с,
         * а відлік перезапускається при відставанні */
        unsigned next = g->t0 + g->frames * 1000u / g->fps;
        unsigned now = nyx_get_ms();
        if ((int) (next - now) > 0) {
            nyx_sleep((int) (next - now));
        } else if ((int) (now - next) > 250) {
            g->t0 = now;          /* сильно відстали (гра «задумалась») - не наздоганяємо, а починаємо відлік заново */
            g->frames = 0;
        }
    }
    unsigned now = nyx_get_ms();
    g->dt = now - g->last_ms;
    g->last_ms = now;
    return g->scale;
}

/* Скільки кадрів на секунду насправді (з моменту ng_open або останнього перезапуску відліку). */
NG_FN unsigned ng_fps(ng_t* g) {
    unsigned el = nyx_get_ms() - g->t0;
    return el ? g->frames * 1000u / el : 0;
}

/* --- малювання --- */
NG_FN inline void ng_pixel(ng_t* g, int x, int y, nyx_u32 c) {
    if ((unsigned) x < (unsigned) g->w && (unsigned) y < (unsigned) g->h) g->px[y * g->w + x] = c;
}

NG_FN void ng_clear(ng_t* g, nyx_u32 c) {
    int n = g->w * g->h;
    for (int i = 0; i < n; i++) g->px[i] = c;
}

/* Заповнений прямокутник. */
NG_FN void ng_rect(ng_t* g, int x, int y, int w, int h, nyx_u32 c) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > g->w) w = g->w - x;
    if (y + h > g->h) h = g->h - y;
    for (int j = 0; j < h; j++) {
        nyx_u32* row = g->px + (y + j) * g->w + x;
        for (int i = 0; i < w; i++) row[i] = c;
    }
}

/* Рамка прямокутника товщиною 1. */
NG_FN void ng_frame(ng_t* g, int x, int y, int w, int h, nyx_u32 c) {
    ng_rect(g, x, y, w, 1, c);
    ng_rect(g, x, y + h - 1, w, 1, c);
    ng_rect(g, x, y, 1, h, c);
    ng_rect(g, x + w - 1, y, 1, h, c);
}

/* Лінія (Брезенхем). */
NG_FN void ng_line(ng_t* g, int x0, int y0, int x1, int y1, nyx_u32 c) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        ng_pixel(g, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* Заповнене коло. */
NG_FN void ng_circle(ng_t* g, int cx, int cy, int r, nyx_u32 c) {
    for (int y = -r; y <= r; y++) {
        int span = 0;
        while ((span + 1) * (span + 1) + y * y <= r * r) span++;
        ng_rect(g, cx - span, cy + y, 2 * span + 1, 1, c);
    }
}

/* Спрайт w x h (пікселі 0x00RRGGBB); колір key - прозорий (0xFFFFFFFF - без прозорості). */
NG_FN void ng_sprite(ng_t* g, int x, int y, const nyx_u32* spr, int w, int h, nyx_u32 key) {
    for (int j = 0; j < h; j++) {
        int yy = y + j;
        if ((unsigned) yy >= (unsigned) g->h) continue;
        for (int i = 0; i < w; i++) {
            int xx = x + i;
            nyx_u32 c = spr[j * w + i];
            if ((unsigned) xx < (unsigned) g->w && c != key) g->px[yy * g->w + xx] = c;
        }
    }
}

/* --- текст системним шрифтом 8x16 --- */
NG_FN unsigned char ng_glyph_code(const char* s, int* i) {
    unsigned char b0 = (unsigned char) s[*i];
    if (b0 < 0x80) { (*i)++; return b0; }
    if ((b0 & 0xE0) == 0xC0 && s[*i + 1]) {
        unsigned cp = ((unsigned) (b0 & 0x1F) << 6) | ((unsigned char) s[*i + 1] & 0x3F);
        *i += 2;
        for (unsigned k = 0; k < CYR_MAP_COUNT; k++) {
            if (cyr_map[k].codepoint == cp) return cyr_map[k].byte_code;
        }
        return '?';
    }
    while (s[*i] && ((unsigned char) s[++*i] & 0xC0) == 0x80) { }   /* інші символи - пропуск, '?' */
    return '?';
}

NG_FN int ng_text_pixel(ng_t* g, int x, int y, const char* s, nyx_u32 c, int scale);

/* --- згладжений шрифт NyxOS (Е20): атласи .nyxf у /system/fonts (tools/make-font.py) ---
 * ng_text малює ними, коли файл є в образі; інакше - старим 8x16 (теж у ядрі). Розміри за scale:
 * 1 -> Inter 14, 2 -> Inter SemiBold 26, 3 -> 38, 4 -> 50, 5+ -> 64; моно (Термінал) - ng_text_mono, клітинка 8x16.
 * #define NG_PIXEL_FONT перед include - лише старий шрифт. */
typedef struct {
    int line_h, base, count;
    const unsigned char* tab;    /* count x 16 байтів, за зростанням коду */
    const unsigned char* data;
} ng_font_t;

#define NG_FONT_SLOTS 6          /* 0..4 - scale 1..5+, 5 - моно */
static ng_font_t* ng_fonts[NG_FONT_SLOTS];
static int ng_font_tried[NG_FONT_SLOTS];
static const char* const ng_font_files[NG_FONT_SLOTS] = {
    "/system/fonts/ui-14.nyxf", "/system/fonts/ui-26.nyxf", "/system/fonts/ui-38.nyxf",
    "/system/fonts/ui-50.nyxf", "/system/fonts/ui-64.nyxf", "/system/fonts/mono-13.nyxf"};

NG_FN unsigned ng_u16(const unsigned char* p) { return (unsigned) p[0] | ((unsigned) p[1] << 8); }
NG_FN unsigned ng_u32(const unsigned char* p) { return ng_u16(p) | (ng_u16(p + 2) << 16); }

/* Завантажити .nyxf (файл цілком у пам'ять програми). 0 - нема файлу чи він зіпсований. */
NG_FN ng_font_t* ng_font_load(const char* path) {
    int fd = nyx_open(path, NYX_O_READ);
    if (fd < 0) return 0;
    int size = nyx_lseek(fd, 0, 2);
    nyx_lseek(fd, 0, 0);
    unsigned char* b = size > 16 ? (unsigned char*) malloc((unsigned) size + sizeof(ng_font_t)) : 0;
    int got = b ? nyx_read(fd, b, size) : -1;
    nyx_close(fd);
    if (!b || got != size || b[0] != 'N' || b[1] != 'Y' || b[2] != 'X' || b[3] != 'F' || b[4] != 1 || b[5] != 4) {
        if (b) free(b);
        return 0;
    }
    ng_font_t* f = (ng_font_t*) (b + size);   /* опис - у тому ж блоці, за даними */
    f->line_h = (int) ng_u16(b + 6);
    f->base = (int) ng_u16(b + 8);
    f->count = (int) ng_u16(b + 10);
    unsigned data_off = ng_u32(b + 12);
    if (16 + (unsigned) f->count * 16 > (unsigned) size || data_off > (unsigned) size) { free(b); return 0; }
    f->tab = b + 16;
    f->data = b + data_off;
    return f;
}

NG_FN ng_font_t* ng_font_slot(int slot) {
#ifdef NG_PIXEL_FONT
    (void) slot;
    return 0;
#else
    if (!ng_font_tried[slot]) { ng_font_tried[slot] = 1; ng_fonts[slot] = ng_font_load(ng_font_files[slot]); }
    return ng_fonts[slot];
#endif
}

/* UTF-8 -> код символу (1-3 байти), зсуває *i. */
NG_FN unsigned ng_utf8(const char* s, int* i) {
    unsigned char b0 = (unsigned char) s[*i];
    if (b0 < 0x80) { (*i)++; return b0; }
    if ((b0 & 0xE0) == 0xC0 && s[*i + 1]) {
        unsigned cp = ((unsigned) (b0 & 0x1F) << 6) | ((unsigned char) s[*i + 1] & 0x3F);
        *i += 2;
        return cp;
    }
    if ((b0 & 0xF0) == 0xE0 && s[*i + 1] && s[*i + 2]) {
        unsigned cp = ((unsigned) (b0 & 0x0F) << 12) | (((unsigned char) s[*i + 1] & 0x3F) << 6) | ((unsigned char) s[*i + 2] & 0x3F);
        *i += 3;
        return cp;
    }
    (*i)++;
    while (s[*i] && ((unsigned char) s[*i] & 0xC0) == 0x80) (*i)++;
    return '?';
}

NG_FN const unsigned char* ng_glyph(const ng_font_t* f, unsigned cp) {
    int lo = 0, hi = f->count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        unsigned c = ng_u32(f->tab + mid * 16);
        if (c == cp) return f->tab + mid * 16;
        if (c < cp) lo = mid + 1; else hi = mid - 1;
    }
    return cp != '?' ? ng_glyph(f, '?') : 0;
}

/* Один рядок згладженим шрифтом; mono_adv > 0 - крок кожної літери (Термінал). Повертає ширину. */
NG_FN int ng_text_font(ng_t* g, const ng_font_t* f, int x, int y, const char* s, nyx_u32 c, int mono_adv, int draw) {
    int i = 0, cx = x;
    int cr = (int) ((c >> 16) & 0xFF), cg = (int) ((c >> 8) & 0xFF), cb = (int) (c & 0xFF);
    while (s[i]) {
        unsigned cp = ng_utf8(s, &i);
        const unsigned char* e = ng_glyph(f, cp);
        if (!e) continue;
        int gw = e[8], gh = e[9], gx = (signed char) e[10], gy = (signed char) e[11], adv = mono_adv ? mono_adv : e[12];
        if (draw && gw && gh) {
            const unsigned char* rows = f->data + ng_u32(e + 4);
            int stride = (gw + 1) / 2;
            for (int ry = 0; ry < gh; ry++) {
                int py = y + gy + ry;
                if (py < 0 || py >= g->h) continue;
                nyx_u32* line = g->px + py * g->w;
                for (int rx = 0; rx < gw; rx++) {
                    int a = rows[ry * stride + rx / 2];
                    a = (rx & 1) ? (a & 15) : (a >> 4);
                    int px = cx + gx + rx;
                    if (!a || px < 0 || px >= g->w) continue;
                    if (a == 15) { line[px] = c & 0xFFFFFF; continue; }
                    nyx_u32 d = line[px];
                    int dr = (int) ((d >> 16) & 0xFF), dg = (int) ((d >> 8) & 0xFF), db = (int) (d & 0xFF);
                    dr += (cr - dr) * a / 15; dg += (cg - dg) * a / 15; db += (cb - db) * a / 15;
                    line[px] = ((nyx_u32) dr << 16) | ((nyx_u32) dg << 8) | (nyx_u32) db;
                }
            }
        }
        cx += adv;
    }
    return cx - x;
}

NG_FN int ng_font_index(int scale) { return scale < 1 ? 0 : scale > 5 ? 4 : scale - 1; }

/* Ширина тексту в пікселях (для центрування) - тим шрифтом, яким його намалює ng_text. */
NG_FN int ng_text_w(const char* s, int scale) {
    ng_font_t* f = ng_font_slot(ng_font_index(scale));
    if (f) return ng_text_font(0, f, 0, 0, s, 0, 0, 0);
    int n = 0;
    for (; *s; s++) if (((unsigned char) *s & 0xC0) != 0x80) n++;
    return n * 8 * (scale < 1 ? 1 : scale);
}

/* Висота рядка тексту цього розміру. */
NG_FN int ng_text_h(int scale) {
    ng_font_t* f = ng_font_slot(ng_font_index(scale));
    return f ? f->line_h : 16 * (scale < 1 ? 1 : scale);
}

/* Моноширинний текст у сітці 8x16 (Термінал, таблиці): JetBrains Mono, інакше старий 8x16. */
NG_FN int ng_text_mono(ng_t* g, int x, int y, const char* s, nyx_u32 c) {
    ng_font_t* f = ng_font_slot(5);
    if (f) return ng_text_font(g, f, x, y - 1, s, c, 8, 1);
    return ng_text_pixel(g, x, y, s, c, 1);
}

/* Текст (UTF-8). scale - розмір (1 - звичайний). Повертає ширину в пікселях. */
NG_FN int ng_text(ng_t* g, int x, int y, const char* s, nyx_u32 c, int scale) {
    ng_font_t* f = ng_font_slot(ng_font_index(scale));
    if (f) return ng_text_font(g, f, x, y, s, c, 0, 1);
    return ng_text_pixel(g, x, y, s, c, scale);
}

/* Старий шрифт 8x16 у збільшенні scale (піксель-арт ігор, запасний шлях). */
NG_FN int ng_text_pixel(ng_t* g, int x, int y, const char* s, nyx_u32 c, int scale) {
    if (scale < 1) scale = 1;
    int i = 0, cx = x;
    while (s[i]) {
        const unsigned char* gl = vga_font_data[ng_glyph_code(s, &i)];
        for (int row = 0; row < 16; row++) {
            for (int col = 0; col < 8; col++) {
                if (gl[row] & (0x80 >> col)) {
                    if (scale == 1) ng_pixel(g, cx + col, y + row, c);
                    else ng_rect(g, cx + col * scale, y + row * scale, scale, scale, c);
                }
            }
        }
        cx += 8 * scale;
    }
    return cx - x;
}

/* --- картинки .nyxi (Е20; tools/make-image.py): палітра 256 кольорів, індекс 0 може бути прозорим --- */
typedef struct {
    int w, h;
    nyx_u32* px;   /* w*h; 0x01000000 у старшому байті - прозорий піксель */
} ng_image_t;

/* Завантажити картинку (файл цілком). 0 - нема чи зіпсована. */
NG_FN ng_image_t* ng_image_load(const char* path) {
    int fd = nyx_open(path, NYX_O_READ);
    if (fd < 0) return 0;
    int size = nyx_lseek(fd, 0, 2);
    nyx_lseek(fd, 0, 0);
    unsigned char* b = size > 12 ? (unsigned char*) malloc((unsigned) size) : 0;
    int got = b ? nyx_read(fd, b, size) : -1;
    nyx_close(fd);
    if (!b || got != size || b[0] != 'N' || b[1] != 'Y' || b[2] != 'X' || b[3] != 'I' || b[4] != 1) { if (b) free(b); return 0; }
    int transparent = b[5] & 1, w = (int) ng_u16(b + 6), h = (int) ng_u16(b + 8), ncol = (int) ng_u16(b + 10);
    const unsigned char* pal = b + 12;
    const unsigned char* rle = pal + ncol * 3;
    ng_image_t* im = (w > 0 && h > 0 && ncol <= 256 && 12 + ncol * 3 <= size) ?
                     (ng_image_t*) malloc(sizeof(ng_image_t) + (unsigned) (w * h) * 4) : 0;
    if (!im) { free(b); return 0; }
    im->w = w; im->h = h; im->px = (nyx_u32*) (im + 1);
    int k = 0;
    for (const unsigned char* p = rle; p + 1 < b + size && k < w * h; p += 2) {
        int n = p[0], v = p[1];
        nyx_u32 c = v < ncol ? ((nyx_u32) pal[v * 3] << 16) | ((nyx_u32) pal[v * 3 + 1] << 8) | pal[v * 3 + 2] : 0;
        if (transparent && v == 0) c = 0x01000000;
        while (n-- > 0 && k < w * h) im->px[k++] = c;
    }
    while (k < w * h) im->px[k++] = transparent ? 0x01000000 : 0;
    free(b);
    return im;
}

/* Нова картинка dw x dh: зменшення усередненням (якісно, для шпалер), збільшення - найближчий піксель.
 * cover=1 - обрізати по центру під пропорцію dw:dh (як шпалери «заповнити екран»). */
NG_FN ng_image_t* ng_image_scale(const ng_image_t* im, int dw, int dh, int cover) {
    if (!im || dw <= 0 || dh <= 0) return 0;
    ng_image_t* out = (ng_image_t*) malloc(sizeof(ng_image_t) + (unsigned) (dw * dh) * 4);
    if (!out) return 0;
    out->w = dw; out->h = dh; out->px = (nyx_u32*) (out + 1);
    int sx0 = 0, sy0 = 0, sw = im->w, sh = im->h;
    if (cover) {
        if (im->w * dh > im->h * dw) { sw = im->h * dw / dh; sx0 = (im->w - sw) / 2; }
        else { sh = im->w * dh / dw; sy0 = (im->h - sh) * 2 / 5; }
    }
    for (int y = 0; y < dh; y++) {
        int ya = sy0 + y * sh / dh, yb = sy0 + (y + 1) * sh / dh;
        if (yb <= ya) yb = ya + 1;
        for (int x = 0; x < dw; x++) {
            int xa = sx0 + x * sw / dw, xb = sx0 + (x + 1) * sw / dw;
            if (xb <= xa) xb = xa + 1;
            unsigned r = 0, gg = 0, b = 0, n = 0;
            for (int yy = ya; yy < yb && yy < im->h; yy++)
                for (int xx = xa; xx < xb && xx < im->w; xx++) {
                    nyx_u32 c = im->px[yy * im->w + xx];
                    r += (c >> 16) & 0xFF; gg += (c >> 8) & 0xFF; b += c & 0xFF; n++;
                }
            out->px[y * dw + x] = n ? ((r / n) << 16) | ((gg / n) << 8) | (b / n) : 0;
        }
    }
    return out;
}

/* Намалювати картинку в прямокутник dw x dh (найближчий піксель; dw=im->w - без масштабу). */
NG_FN void ng_image_draw(ng_t* g, const ng_image_t* im, int x, int y, int dw, int dh) {
    if (!im || dw <= 0 || dh <= 0) return;
    for (int ry = 0; ry < dh; ry++) {
        int py = y + ry;
        if (py < 0 || py >= g->h) continue;
        const nyx_u32* src = im->px + (ry * im->h / dh) * im->w;
        nyx_u32* dst = g->px + py * g->w;
        for (int rx = 0; rx < dw; rx++) {
            int px = x + rx;
            if (px < 0 || px >= g->w) continue;
            nyx_u32 c = src[rx * im->w / dw];
            if (!(c & 0x01000000)) dst[px] = c;
        }
    }
}

/* --- ввід (тонкі обгортки, щоб гра брала все з одного місця) --- */
NG_FN inline int ng_key_down(int code) { return nyx_key_down(code); }
NG_FN inline int ng_key_event(void) { return nyx_key_event(); }
NG_FN inline int ng_mouse(nyx_mouse_t* m) { return nyx_mouse(m); }

#endif
