/* ngtest.c — перевірка бібліотеки ігор libnyxgame.h (J1, 20.11.2026).
 * 90 кадрів анімації (квадрат їде), потім 2 с контрольна сцена, яку тести
 * звіряють на скріншоті: червоний прямокутник, зелене коло, синя лінія,
 * спрайт-шахівниця з прозорістю, білий текст «NyxOS Привіт» у збільшенні x2. */
#include "libnyxgame.h"

static nyx_u32 checker[8 * 8];

static void scene(ng_t* g) {
    ng_clear(g, 0x101830);
    ng_rect(g, 10, 10, 40, 30, 0xE02020);
    ng_circle(g, 160, 90, 20, 0x20C040);
    ng_line(g, 0, 179, 319, 120, 0x2050E0);
    ng_frame(g, 255, 15, 50, 50, 0xF0F0F0);
    ng_sprite(g, 260, 20, checker, 8, 8, 0xFF00FF);
    ng_text(g, 10, 140, "NyxOS Привіт", 0xFFFFFF, 2);
}

int main(void) {
    for (int i = 0; i < 64; i++) checker[i] = ((i / 8 + i % 8) & 1) ? 0xFFD040 : 0xFF00FF;   /* 0xFF00FF - прозорий */
    ng_t g;
    if (ng_open(&g, 320, 180, 60) != 0) {
        printf("ngtest: графіки нема (текстовий режим) - пропуск\n");
        return 0;
    }
    for (int f = 0; f < 90; f++) {
        scene(&g);
        ng_rect(&g, f * 3, 60, 16, 16, 0xFFFFFF);
        ng_present(&g);
    }
    unsigned fps = ng_fps(&g);
    scene(&g);
    ng_present(&g);
    nyx_sleep(2000);
    printf("ngtest: 90 кадрів, %u кадр/с, масштаб x%d\n", fps, g.scale);
    int ok = g.scale > 0 && fps >= 55 && fps <= 62;
    printf("ngtest: %s\n", ok ? "ок" : "ПОМИЛКА");
    ng_close(&g);
    return ok ? 0 : 3;
}
