/* inputtest.c — перевірка клавіш і миші для ігор (J1.3, 20.11.2026).
 * Друкує кожну подію клавіші й рух миші, поки не натиснуть Esc (або 20 с),
 * і підсумок, який звіряють тести (tests/run_tests.py):
 *   inputtest: W 1/1, A 1/1, утримання ок, відпускання ок, миша dx=.. dy=.. кнопки=..
 * Сам по собі ок/помилку не вирішує - скільки чого натискали, знає лише тест. */
#include "nyxc.h"

int main(void) {
    printf("inputtest: натискай клавіші, рухай мишею; Esc - вихід\n");
    int w_down = 0, w_up = 0, a_down = 0, a_up = 0, held_ok = 0, released_ok = 0, buttons_seen = 0;
    int dx_sum = 0, dy_sum = 0;
    unsigned t0 = nyx_get_ms();
    nyx_mouse_t m;
    int done = 0;
    while (!done && nyx_get_ms() - t0 < 20000) {
        int ev;
        while ((ev = nyx_key_event()) != 0) {
            int code = NYX_EV_CODE(ev), down = NYX_EV_DOWN(ev);
            printf("  клавіша 0x%x %s\n", code, down ? "натиснуто" : "відпущено");
            if (code == NYX_KEY_W) { if (down) w_down++; else w_up++; }
            if (code == NYX_KEY_A) {
                if (down) { a_down++; held_ok = nyx_key_down(NYX_KEY_A) == 1; }
                else { a_up++; released_ok = nyx_key_down(NYX_KEY_A) == 0; }
            }
            if (code == NYX_KEY_ESC && down) done = 1;
        }
        if (nyx_mouse(&m) == 0 && (m.dx || m.dy || m.buttons)) {
            if (m.dx || m.dy) printf("  миша dx=%d dy=%d\n", m.dx, m.dy);
            dx_sum += m.dx;
            dy_sum += m.dy;
            buttons_seen |= m.buttons;
        }
        nyx_sleep(5);
    }
    printf("inputtest: W %d/%d, A %d/%d, утримання %s, відпускання %s, миша dx=%d dy=%d кнопки=%d\n",
           w_down, w_up, a_down, a_up, held_ok ? "ок" : "НІ", released_ok ? "ок" : "НІ", dx_sum, dy_sum, buttons_seen);
    return 0;
}
