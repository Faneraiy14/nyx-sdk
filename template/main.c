/* Шаблон програми NyxOS: кадр 320x180 на весь екран, квадрат рухається стрілками, Esc — вихід.
 * Збірка: tools/nyx-build.sh build template/main.c  ->  build/main.bin (поклади в /games, і він з'явиться
 * в Ігровому центрі). Документація API — у include/libnyxgame.h і include/libnyx.h. */
#include "libnyxgame.h"

int main(void) {
    ng_t g;
    if (ng_open(&g, 320, 180, 60) != 0) return 1;   /* 320x180, 60 кадрів/с; ядро збільшить на весь екран */
    int x = 150, y = 80;
    while (!ng_key_down(NYX_KEY_ESC)) {
        if (ng_key_down(NYX_KEY_LEFT)) x -= 2;
        if (ng_key_down(NYX_KEY_RIGHT)) x += 2;
        if (ng_key_down(NYX_KEY_UP)) y -= 2;
        if (ng_key_down(NYX_KEY_DOWN)) y += 2;
        ng_clear(&g, 0x101830);
        ng_rect(&g, x, y, 20, 20, 0xFFD040);
        ng_text_pixel(&g, 8, 8, "Привіт, NyxOS! Стрілки - рух, Esc - вихід", 0xFFFFFF, 1);
        ng_present(&g);
    }
    ng_close(&g);
    return 0;
}
