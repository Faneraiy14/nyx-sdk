/* frametest.c — перевірка кадру на весь екран (J1.2, 20.11.2026).
 * Малює кадр 320x180 (смуги кольорів + квадрат, що їде), показує 120 кадрів
 * по 60 на секунду через nyx_present (ядро масштабує на весь екран), потім
 * тримає контрольний кадр 2 с (для скріншота в тестах) і друкує підсумок.
 *   frametest       - 320x180
 *   frametest 640   - 640x360 */
#include "nyxc.h"

static nyx_u32* frame;
static int W = 320, H = 180;

static void fill(int x, int y, int w, int h, nyx_u32 c) {
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= H) continue;
        for (int i = x; i < x + w; i++) {
            if (i >= 0 && i < W) frame[j * W + i] = c;
        }
    }
}

/* Контрольний кадр: 4 вертикальні смуги (червона, зелена, синя, біла) - тест
 * перевіряє їхні кольори на скріншоті. */
static void control_frame(void) {
    static const nyx_u32 bars[4] = {0xE02020, 0x20C040, 0x2050E0, 0xF0F0F0};
    for (int k = 0; k < 4; k++) fill(k * W / 4, 0, W / 4, H, bars[k]);
}

int main(int argc, char** argv) {
    if (argc > 1 && atoi(argv[1]) == 640) { W = 640; H = 360; }
    int sw, sh;
    nyx_screen_size(&sw, &sh);
    if (sw == 0) {
        printf("frametest: графіки нема (текстовий режим) - пропуск\n");
        return 0;
    }
    frame = (nyx_u32*) malloc((unsigned) (W * H * 4));
    if (!frame) {
        printf("frametest: нема пам'яті для кадру\n");
        return 2;
    }
    /* Безпека: чужа (ядрова) пам'ять чи кадр більший за екран - відмова. */
    int deny_kernel = nyx_present((const nyx_u32*) 0x00100000, W, H) == -1;
    int deny_big = nyx_present(frame, sw + 1, 1) == -1;
    unsigned t0 = nyx_get_ms(), spent = 0;
    int scale = 0;
    for (int f = 0; f < 120; f++) {
        for (int y = 0; y < H; y++) {          /* градієнт фону */
            nyx_u32 c = (nyx_u32) (20 + y * 60 / H);
            for (int x = 0; x < W; x++) frame[y * W + x] = (c << 16) | (c / 2 << 8) | (c + 40);
        }
        fill(f * (W - 30) / 119, H / 2 - 15, 30, 30, 0xFFD040);   /* квадрат їде зліва направо */
        unsigned p0 = nyx_get_ms();
        scale = nyx_present(frame, W, H);
        spent += nyx_get_ms() - p0;
        unsigned next = t0 + (unsigned) ((f + 1) * 1000 / 60);
        unsigned now = nyx_get_ms();
        if ((int) (next - now) > 0) nyx_sleep(next - now);
    }
    unsigned total = nyx_get_ms() - t0;
    control_frame();
    nyx_present(frame, W, H);
    nyx_sleep(2000);
    printf("frametest: екран %dx%d, кадр %dx%d, масштаб x%d\n", sw, sh, W, H, scale);
    printf("frametest: 120 кадрів за %u мс (%u кадр/с), показ кадру в середньому %u мс\n",
           total, total ? 120000u / total : 0u, spent / 120);
    printf("frametest: ядрова адреса - %s, кадр більший за екран - %s\n",
           deny_kernel ? "відмова (ок)" : "ПРИЙНЯТО (ПОМИЛКА)", deny_big ? "відмова (ок)" : "ПРИЙНЯТО (ПОМИЛКА)");
    int ok = scale > 0 && deny_kernel && deny_big;
    printf("frametest: %s\n", ok ? "ок" : "ПОМИЛКА");
    return ok ? 0 : 3;
}
