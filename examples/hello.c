/* hello.c — user-програма NyxOS мовою C (не вручну закодовані байти
 * асемблера, як demo_program[] у shell.c) - доказ, що libnyx.h працює.
 * Блок 3 (п.13): демонструє ще й ФАЙЛОВІ syscall'и - записує файл,
 * читає його назад і друкує. Потрібен змонтований диск (shell
 * "diskmount"). Компілюється й пакується через programs/build.sh.
 */
#include "libnyx.h"

int main(void) {
    nyx_print("Привіт зі справжньої C-програми на NyxOS!\n");

    const char* name = "ctest.txt";
    const char* msg = "Рядок, записаний C-програмою через файловий syscall.";

    /* Щоб програму можна було запускати ПОВТОРНО: прибираємо старий файл
     * (fs_create не перезаписує наявний). Якщо файлу ще нема - це просто
     * -1, ігноруємо. */
    nyx_fdelete(name);

    if (nyx_fwrite(name, msg, (int) nyx_strlen(msg)) == 0) {
        nyx_print("Записав файл ctest.txt\n");
    } else {
        nyx_print("fwrite не вдався - диск змонтовано? (команда diskmount)\n");
        nyx_exit(1);
    }

    char buf[160];
    int n = nyx_fread(name, buf, (int) sizeof(buf) - 1);
    if (n >= 0) {
        buf[n] = '\0';
        nyx_print("Прочитав назад: ");
        nyx_print(buf);
        nyx_print("\n");
    } else {
        nyx_print("fread не вдався\n");
    }

    return 0;
}
