#ifndef LIBNYX_H
#define LIBNYX_H

/* libnyx.h — міні-бібліотека для user-програм NyxOS (Фаза C, п.12).
 * Кожна функція - тонка обгортка над системним викликом `int 0x80` ядра
 * NyxCore - можна писати ЗВИЧАЙНИЙ C-код. Конвенція виклику - своя, найпростіша
 * можлива: eax = номер syscall, ebx/ecx/edx = до 3 аргументів,
 * повернене значення (якщо є) приходить назад через eax.
 *
 * Компілюється й лінкується ОКРЕМО від ядра (programs/build.sh) -
 * freestanding, без стандартної бібліотеки C, разом зі start.s
 * (входить у виконання ПЕРШИМ, викликає main(), яку й пише сама
 * програма).
 */

typedef unsigned int nyx_u32;
typedef unsigned char nyx_u8;

/* Версія ABI ядра, під яку написано цей SDK (= NYXCORE_ABI ядра; таблиця викликів - abi.txt). */
#define NYX_ABI 1
#define NYX_STR_(x) #x
#define NYX_STR(x) NYX_STR_(x)

/* Р5 (перевірка ABI): кожна обгортка, яку програма СПРАВДІ використовує, записує номер свого syscall'у в
 * секцію .nyxabi (не завантажується - у сирий бінарник не потрапляє). programs/build.sh звіряє цей список з
 * build/abi.txt ядра й не збирає програму, якій бракує виклику. Невикористані static inline обгортки gcc не
 * генерує - тож у список ідуть лише потрібні. n - лише числовий літерал. */
#define NYX_SC(n) ({ __asm__ volatile (".pushsection .nyxabi,\"\",@progbits\n.long " #n "\n.popsection"); (nyx_u32) (n); })

/* Завершує процес із кодом code (shell покаже ненульовий код). */
static inline void nyx_exit(int code) {
    __asm__ volatile ("int $0x80" : : "a"(NYX_SC(0)), "b"((nyx_u32) code));
}

static inline void nyx_putc(char c) {
    __asm__ volatile ("int $0x80" : : "a"(NYX_SC(1)), "b"((nyx_u32) c));
}

/* rgb - звичайний 0x00RRGGBB (як у html-кольорах), НЕ формат ЦІЄЇ
 * конкретної відеокарти - ядро саме перепаковує через gfx_rgb(). */
static inline void nyx_draw_pixel(int x, int y, nyx_u32 rgb) {
    __asm__ volatile ("int $0x80" : : "a"(NYX_SC(3)), "b"((nyx_u32) x), "c"((nyx_u32) y), "d"(rgb));
}

static inline nyx_u32 nyx_get_ticks(void) {
    nyx_u32 result;
    __asm__ volatile ("int $0x80" : "=a"(result) : "a"(NYX_SC(4)));
    return result;
}

/* Рядок МАЄ лежати в пам'яті САМОГО процесу (звичайний масив char[]
 * чи стрічковий літерал у .rodata - обидва копіюються в PAGE_USER-
 * сторінку процесу разом з рештою коду при завантаженні). */
static inline void nyx_print(const char* str) {
    __asm__ volatile ("int $0x80" : : "a"(NYX_SC(2)), "b"((nyx_u32) str));
}

static inline void nyx_fill_rect(int x, int y, int w, int h, nyx_u32 rgb) {
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            nyx_draw_pixel(x + col, y + row, rgb);
        }
    }
}

/* ------------------------------------------------------------------
 * Файлові syscall'и (Блок 3, п.13) - робота з файлами NyxFS прямо з
 * user-програми. Диск має бути змонтований (shell "diskmount") - інакше
 * fs.c вимкнений і виклики повертають помилку. Імена/буфери - у пам'яті
 * самого процесу. ------------------------------------------------------ */

/* Читає файл name у buf (не більше buf_len байтів). Повертає РОЗМІР
 * файлу при успіху, або -1 (нема файлу / буфер замалий / диск не
 * змонтовано). */
static inline int nyx_fread(const char* name, void* buf, int buf_len) {
    int result;
    __asm__ volatile ("int $0x80"
        : "=a"(result)
        : "a"(NYX_SC(5)), "b"((nyx_u32) name), "c"((nyx_u32) buf), "d"((nyx_u32) buf_len)
        : "memory");
    return result;
}

/* Записує len байтів data у НОВИЙ файл name. Повертає 0 при успіху, -1
 * при помилці (файл уже існує / диск повний / не змонтовано). */
static inline int nyx_fwrite(const char* name, const void* data, int len) {
    int result;
    __asm__ volatile ("int $0x80"
        : "=a"(result)
        : "a"(NYX_SC(6)), "b"((nyx_u32) name), "c"((nyx_u32) data), "d"((nyx_u32) len)
        : "memory");
    return result;
}

/* 1, якщо файл name існує, інакше 0. */
static inline int nyx_fexists(const char* name) {
    int result;
    __asm__ volatile ("int $0x80"
        : "=a"(result)
        : "a"(NYX_SC(7)), "b"((nyx_u32) name)
        : "memory");
    return result;
}

/* Видаляє файл name. 0 при успіху, -1 якщо не знайдено. */
static inline int nyx_fdelete(const char* name) {
    int result;
    __asm__ volatile ("int $0x80"
        : "=a"(result)
        : "a"(NYX_SC(8)), "b"((nyx_u32) name)
        : "memory");
    return result;
}

/* Довжина нуль-термінованого рядка - без syscall, звичайний C-хелпер
 * (зручно для nyx_fwrite(name, msg, nyx_strlen(msg))). */
static inline nyx_u32 nyx_strlen(const char* s) {
    nyx_u32 n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

/* ------------------------------------------------------------------
 * POSIX-подібні виклики (п.3 ROADMAP) - дескриптори файлів, купа, pid,
 * сон, ввід. fd 0 - клавіатура (рядок після Enter, з луною), 1/2 - екран,
 * файли - з 3. Відкритий файл живе в буфері ядра і потрапляє на диск
 * при nyx_close (або виході з програми). --------------------------- */

#define NYX_O_READ   1
#define NYX_O_WRITE  2 /* створити або обнулити */
#define NYX_O_APPEND 4 /* дописувати в кінець */

#define NYX_SEEK_SET 0
#define NYX_SEEK_CUR 1
#define NYX_SEEK_END 2

static inline int nyx_syscall3(nyx_u32 n, nyx_u32 a, nyx_u32 b, nyx_u32 c) {
    int result;
    __asm__ volatile ("int $0x80" : "=a"(result) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return result;
}

static inline int nyx_open(const char* path, int flags) {
    return nyx_syscall3(NYX_SC(9), (nyx_u32) path, (nyx_u32) flags, 0);
}

/* Повертає кількість прочитаних байтів (0 - кінець файлу) або -1. */
static inline int nyx_read(int fd, void* buf, int len) {
    return nyx_syscall3(NYX_SC(10), (nyx_u32) fd, (nyx_u32) buf, (nyx_u32) len);
}

static inline int nyx_write(int fd, const void* buf, int len) {
    return nyx_syscall3(NYX_SC(11), (nyx_u32) fd, (nyx_u32) buf, (nyx_u32) len);
}

static inline int nyx_close(int fd) {
    return nyx_syscall3(NYX_SC(12), (nyx_u32) fd, 0, 0);
}

/* Розширює купу на incr байтів; повертає СТАРИЙ кінець купи або (void*)-1. */
static inline void* nyx_sbrk(int incr) {
    return (void*) nyx_syscall3(NYX_SC(13), (nyx_u32) incr, 0, 0);
}

static inline int nyx_getpid(void) {
    return nyx_syscall3(NYX_SC(14), 0, 0, 0);
}

static inline void nyx_sleep(int ms) {
    nyx_syscall3(NYX_SC(15), (nyx_u32) ms, 0, 0);
}

/* Одна клавіша без луни й без чекання Enter. nonblock=1 - не чекати
 * (повертає -1, якщо нічого не натиснуто). Стрілки - 2 (вгору) / 3 (вниз). */
static inline int nyx_getchar(int nonblock) {
    return nyx_syscall3(NYX_SC(16), (nyx_u32) nonblock, 0, 0);
}

static inline int nyx_lseek(int fd, int offset, int whence) {
    return nyx_syscall3(NYX_SC(17), (nyx_u32) fd, (nyx_u32) offset, (nyx_u32) whence);
}

/* Розмір екрана (чи вікна термінала) у клітинках: рядки << 16 | стовпці.
 * Керувати ним - ANSI-послідовностями у виводі: ESC[2J, ESC[r;cH, ESC[K,
 * ESC[7m/ESC[0m (ядро розбирає їх у виводі програми). */
static inline int nyx_termsize(void) {
    return nyx_syscall3(NYX_SC(22), 0, 0, 0);
}

/* uid користувача, від імені якого працює програма (0 - root). */
static inline int nyx_getuid(void) {
    return nyx_syscall3(NYX_SC(21), 0, 0, 0);
}

/* fork: копія поточної програми. Повертає pid дитини батькові, 0 -
 * дитині, -1 - помилка. */
static inline int nyx_fork(void) {
    return nyx_syscall3(NYX_SC(18), 0, 0, 0);
}

/* exec: замінює поточну програму файлом path (argv з NULL у кінці, або
 * 0). При успіху не повертається; -1 - файлу нема, стара програма живе. */
static inline int nyx_exec(const char* path, char* const* argv) {
    return nyx_syscall3(NYX_SC(19), (nyx_u32) path, (nyx_u32) argv, 0);
}

/* wait: чекає завершення будь-якої дитини. Повертає її pid (код виходу
 * в *status, якщо status не 0) або -1, якщо дітей нема. */
static inline int nyx_wait(int* status) {
    return nyx_syscall3(NYX_SC(20), (nyx_u32) status, 0, 0);
}

/* Як nyx_wait, але НЕ чекає (Р4.2): 0 - діти є, але ніхто ще не завершився; -1 - дітей нема. */
static inline int nyx_wait_nohang(int* status) {
    return nyx_syscall3(NYX_SC(20), (nyx_u32) status, 1, 0);
}

static inline int nyx_getppid(void) {
    return nyx_syscall3(NYX_SC(28), 0, 0, 0);
}

/* Ядро не знає такого виклику (старіше ядро чи помилка в номері) - усі виклики 9+ повертають це значення. */
#define NYX_ENOSYS (-38)

/* Віддати решту свого кванта іншим процесам. */
static inline void nyx_yield(void) {
    nyx_syscall3(NYX_SC(29), 0, 0, 0);
}

/* Мілісекунди від запуску системи (точність 1 мс; J1 - точний час для ігор).
 * nyx_get_ticks() лишається як був - тіки по 10 мс. Різницю рахувати
 * беззнаково (now - start). */
static inline nyx_u32 nyx_get_ms(void) {
    return (nyx_u32) nyx_syscall3(NYX_SC(30), 0, 0, 0);
}

/* Розмір екрана в пікселях (J1.2). 0 в обох - графіки нема (текстовий режим). */
static inline void nyx_screen_size(int* w, int* h) {
    nyx_u32 v = (nyx_u32) nyx_syscall3(NYX_SC(31), 0, 0, 0);
    *w = (int) (v >> 16);
    *h = (int) (v & 0xFFFF);
}

/* Показати кадр на весь екран (J1.2): pixels - w*h пікселів 0x00RRGGBB
 * у пам'яті програми. Ядро збільшує його в ціле число разів (320x180 на
 * 1920x1080 = x6), ставить по центру з чорними полями. Повертає масштаб
 * або -1 (нема графіки / кадр більший за екран / буфер не свій). */
static inline int nyx_present(const nyx_u32* pixels, int w, int h) {
    return nyx_syscall3(NYX_SC(32), (nyx_u32) pixels, (nyx_u32) w, (nyx_u32) h);
}

/* --- Клавіші й миша для ігор (J1.3) ---------------------------------------
 * Код клавіші - scancode set 1; розширені (стрілки тощо) - код | 0x80.
 * Подія з nyx_key_event(): (натиснуто << 8) | код; 0 - подій нема.
 * Автоповтор (клавіша затиснута) подією не є: одне «натиснуто», одне «відпущено». */
#define NYX_KEY_ESC    0x01
#define NYX_KEY_1      0x02
#define NYX_KEY_ENTER  0x1C
#define NYX_KEY_LCTRL  0x1D
#define NYX_KEY_LSHIFT 0x2A
#define NYX_KEY_SPACE  0x39
#define NYX_KEY_Q 0x10
#define NYX_KEY_W 0x11
#define NYX_KEY_E 0x12
#define NYX_KEY_R 0x13
#define NYX_KEY_A 0x1E
#define NYX_KEY_S 0x1F
#define NYX_KEY_D 0x20
#define NYX_KEY_F 0x21
#define NYX_KEY_UP    0xC8
#define NYX_KEY_DOWN  0xD0
#define NYX_KEY_LEFT  0xCB
#define NYX_KEY_RIGHT 0xCD
#define NYX_EV_DOWN(ev) (((ev) >> 8) & 1)
#define NYX_EV_CODE(ev) ((ev) & 0xFF)

static inline int nyx_key_event(void) {
    return nyx_syscall3(NYX_SC(33), 0, 0, 0);
}

/* Чи натиснута клавіша зараз (1/0). */
static inline int nyx_key_down(int code) {
    return nyx_syscall3(NYX_SC(34), (nyx_u32) code, 0, 0);
}

/* Миша: позиція, кнопки (біт 0 ліва, 1 права, 2 середня) і рух dx/dy з
 * минулого виклику (+x праворуч, +y вниз). 0 - ок, -1 - помилка. */
/* Вміст теки (Р4.1): рядки «ім'я\n» (теки - «ім'я/\n») у buf, у кінці '\0'. Повертає довжину або -1. */
static inline int nyx_listdir(const char* path, char* buf, int cap) {
    return nyx_syscall3(NYX_SC(36), (nyx_u32) path, (nyx_u32) buf, (nyx_u32) cap);
}

typedef struct { int x, y, buttons, dx, dy; } nyx_mouse_t;

/* --- Вікна (Р4.2) ----------------------------------------------------------
 * Програма: nyx_win_create -> малює у СВОЄМУ буфері w*h (0x00RRGGBB) -> nyx_win_present копіює в
 * вікно; nyx_win_event - події (0 - нема):
 *   NYX_WEV_KEY:   (натиснуто << 8) | код клавіші
 *   NYX_WEV_MOUSE: клік лівою кнопкою, x/y всередині вікна (NYX_WEV_X/Y, до 4095)
 *   NYX_WEV_CLOSE: натиснули хрестик - програма закриває вікно / завершується
 *   NYX_WEV_RESIZE: стіл просить розмір NYX_WEV_W x NYX_WEV_H («на весь екран» / «повернути»): програма
 *                   робить nyx_win_resize і перемальовує; не вміє - просто ігнорує
 * Вікна процесу зникають самі, коли він завершується. */
#define NYX_WEV_TYPE(e)    (((unsigned) (e) >> 24) & 0xFF)
#define NYX_WEV_KEY   1
#define NYX_WEV_MOUSE 2
#define NYX_WEV_CLOSE 3
#define NYX_WEV_RESIZE 4
#define NYX_WEV_X(e)       ((e) & 0xFFF)
#define NYX_WEV_Y(e)       (((e) >> 12) & 0xFFF)
#define NYX_WEV_BUTTONS(e) (1)               /* подія миші - завжди клік лівою */
#define NYX_WEV_W(e)       ((e) & 0xFFF)
#define NYX_WEV_H(e)       (((e) >> 12) & 0xFFF)
#define NYX_WIN_TITLE_MAX 48

static inline int nyx_win_create(int w, int h, const char* title) {
    return nyx_syscall3(NYX_SC(37), (nyx_u32) w, (nyx_u32) h, (nyx_u32) title);
}
/* Буфер обміну (0.1.0.11): текст до 16 КБ, спільний для всіх програм. get повертає довжину (не з нулем у кінці). */
#define NYX_HAS_CLIPBOARD 1
static inline int nyx_clip_set(const char* text, int len) { return nyx_syscall3(NYX_SC(56), (nyx_u32) text, (nyx_u32) len, 0); }
static inline int nyx_clip_get(char* buf, int cap) { return nyx_syscall3(NYX_SC(57), (nyx_u32) buf, (nyx_u32) cap, 0); }
/* 0.1.0.18: лічильник змін вікон (показ кадру, поява, закриття, рух, розмір) - для столу: нема змін і вводу - можна
 * не перемальовувати екран. */
#define NYX_HAS_WIN_SEQ 1
static inline unsigned nyx_win_seq(void) { return (unsigned) nyx_syscall3(NYX_SC(58), 0, 0, 0); }
/* 0.1.0.19 X2: стеження за системними викликами (nyxtrace). Дитина робить nyx_trace(NYX_TRACE_ME, 0, 0) і exec;
 * батько читає записи nyx_trace(NYX_TRACE_READ, buf, байтів) -> скільки записів; NYX_TRACE_STOP - кінець.
 * seq іде підряд - пропуск означає втрачені записи (кільце на 256). */
#define NYX_TRACE_STOP 0
#define NYX_TRACE_READ 2
#define NYX_TRACE_ME   3
typedef struct { nyx_u32 seq; unsigned short pid, num; nyx_u32 a1, a2, a3; int ret; char str[24]; } nyx_trace_rec_t;
static inline int nyx_trace(int op, void* buf, int cap) { return nyx_syscall3(NYX_SC(59), (nyx_u32) op, (nyx_u32) buf, (nyx_u32) cap); }
/* 0.1.0.24 W4: сповіщення. nyx_notify - короткий рядок (до 95 байт) - стіл покаже карткою; nyx_notify_get - перше з
 * номером > after у buf, повертає його номер (0 - нових нема). Ядро саме сповіщає: флешка, час з мережі, аварія програми. */
#define NYX_HAS_NOTIFY 1
static inline int nyx_notify(const char* text) { return nyx_syscall3(NYX_SC(60), (nyx_u32) text, 0, 0); }
static inline unsigned nyx_notify_get(unsigned after, char* buf, int cap) { return (unsigned) nyx_syscall3(NYX_SC(61), (nyx_u32) after, (nyx_u32) buf, (nyx_u32) cap); }
/* 0.1.0.27 B3: бібліотеки .nyxlib з вікна. op: NYX_LIB_INSTALL (arg - шлях до файлу; confirmed=1 - «TAK» для
 * непідписаного) чи NYX_LIB_REMOVE (arg - назва). password - пароль поточного адміністратора (як sudo; ядро стирає
 * його в req). Вивід (що сталося) - у out. 0 - вдалося, -2 - не той пароль / не адміністратор, -3 - відмова (див. out). */
#define NYX_HAS_LIB 1
#define NYX_LIB_INSTALL 1
#define NYX_LIB_REMOVE  2
#define NYX_APP_INSTALL 3   /* 0.1.0.30: додаток .nyxapp (arg - файл; confirmed - «TAK» для непідписаного чи нового автора) */
#define NYX_APP_REMOVE  4   /* 0.1.0.30: arg - назва додатка */
/* 0.1.0.30: процес додатка з /apps отримує -13 (EACCES) на те, чого не просив у маніфесті (needs: net, sound, files,
 * gamepad, fullscreen); без files бачить лише /system, /proc, /dev, /apps/<назва>/ і ~/.apps/<назва>/. */
#define NYX_EACCES (-13)
typedef struct { nyx_u32 op, confirmed; char arg[200]; char password[64]; } nyx_lib_req_t;
static inline int nyx_lib(nyx_lib_req_t* req, char* out, int cap) { return nyx_syscall3(NYX_SC(62), (nyx_u32) req, (nyx_u32) out, (nyx_u32) cap); }
/* 0.1.0.29: звук. Один потік на систему: nyx_audio_open(44100 або 48000) - 0, -19 нема звукової карти, -16 грає інша
 * програма. Далі дописуй PCM (16 біт, стерео L,R) - nyx_audio_write не чекає й повертає, скільки байтів узято (0 - буфер
 * ~0,6 с повний, поспи кілька мс). Не встиг - ядро грає тишу. nyx_audio_pending - скільки байтів ще не програно (0 - усе).
 * Закривається й при виході програми. nyx_audio_volume(0..100) - гучність системи, -1 - лише дізнатись. */
#define NYX_HAS_AUDIO 1
static inline int nyx_audio_open(int rate) { return nyx_syscall3(NYX_SC(63), 1, (nyx_u32) rate, 0); }
static inline int nyx_audio_write(const short* pcm, int bytes) { return nyx_syscall3(NYX_SC(63), 2, (nyx_u32) pcm, (nyx_u32) bytes); }
static inline int nyx_audio_pending(void) { return nyx_syscall3(NYX_SC(63), 3, 0, 0); }
static inline int nyx_audio_close(void) { return nyx_syscall3(NYX_SC(63), 4, 0, 0); }
static inline int nyx_audio_volume(int v) { return nyx_syscall3(NYX_SC(63), 5, (nyx_u32) v, 0); }
/* 0.1.0.31: нова тека (будь-яке ім'я, і з пробілами; батьківська має існувати) -> 0, -1 - не вдалося (нема права, уже є). */
#define NYX_HAS_MKDIR 1
static inline int nyx_mkdir(const char* path) { return nyx_syscall3(NYX_SC(64), (nyx_u32) path, 0, 0); }

/* Змінити розмір свого вікна (0.1.0.6; на подію NYX_WEV_RESIZE). Вміст після цього - чистий, перемалюй. */
static inline int nyx_win_resize(int id, int w, int h) {
    return nyx_syscall3(NYX_SC(55), (nyx_u32) id, (nyx_u32) w, (nyx_u32) h);
}
static inline int nyx_win_present(int id, const nyx_u32* pixels) {
    return nyx_syscall3(NYX_SC(38), (nyx_u32) id, (nyx_u32) pixels, 0);
}
static inline int nyx_win_event(int id) {
    return nyx_syscall3(NYX_SC(39), (nyx_u32) id, 0, 0);
}
static inline int nyx_win_close(int id) {
    return nyx_syscall3(NYX_SC(40), (nyx_u32) id, 0, 0);
}

/* Р4.3: скільки байтів у трубі можна прочитати не чекаючи (0 - поки нема, -2 - кінець, -1 - не труба). */
static inline int nyx_pipe_avail(int fd) { return nyx_syscall3(NYX_SC(46), (nyx_u32) fd, 0, 0); }
/* Р4.3: вбудована команда ядра (ls, cat, df, cd, ...) з виводом у buf; -1 - не дозволена програмам. */
static inline int nyx_syscmd(const char* cmd, char* buf, int cap) {
    return nyx_syscall3(NYX_SC(47), (nyx_u32) cmd, (nyx_u32) buf, (nyx_u32) cap);
}

/* Р4.4 службове: */
static inline int nyx_power(int op) { return nyx_syscall3(NYX_SC(48), (nyx_u32) op, 0, 0); }   /* 1 - перезавантажити, 2 - вимкнути */
/* Сторінка http/https (тіло без заголовків) у buf; довжина або <0: -2 нема карти, -3 нема IP, -4 не вдалось, -5 зайнято. */
static inline int nyx_http_get(const char* url, char* buf, int cap) {
    return nyx_syscall3(NYX_SC(49), (nyx_u32) url, (nyx_u32) buf, (nyx_u32) cap);
}
typedef struct { int disks, chosen, exists, free_mb, disk_mb, has_esp; char model[48]; } nyx_install_info_t;
#define NYX_INSTALL_SELECT   1
#define NYX_INSTALL_CREATE   2
#define NYX_INSTALL_BOOTABLE 3
#define NYX_INSTALL_BOOTMODE 4
/* Інсталятор (лише адміністратор): стан диска / дія. 0 - вдалось, -1 - ні або не адміністратор. */
static inline int nyx_install_info(nyx_install_info_t* i) { return nyx_syscall3(NYX_SC(50), (nyx_u32) i, 0, 0); }
static inline int nyx_install_do(int op, int arg, const char* password) {
    return nyx_syscall3(NYX_SC(51), (nyx_u32) op, (nyx_u32) arg, (nyx_u32) password);
}

/* Для столу (window manager): */
typedef struct { int id, pid, x, y, w, h, z; char title[NYX_WIN_TITLE_MAX]; } nyx_winfo_t;
/* 0.1.0.3 - оверлей поверх гри (Ігровий центр): гаряча клавіша Win, копія екрана, лічильник кадрів. */
#define NYX_HOTKEY_RELEASE 0
#define NYX_HOTKEY_CLAIM   1   /* поки тримаєш - Win не йде в чергу подій (гра не бачить) */
#define NYX_HOTKEY_TAKE    2   /* скільки разів натиснули з минулого виклику; -1 - клавіша не твоя */
static inline int nyx_hotkey(int op) { return nyx_syscall3(NYX_SC(52), (nyx_u32) op, 0, 0); }
/* Екран у buf (0x00RRGGBB, ширина*висота слів; cap - байтів). Повертає (ширина << 16) | висота або -1. */
static inline int nyx_screen_grab(nyx_u32* buf, int cap) { return nyx_syscall3(NYX_SC(53), (nyx_u32) buf, (nyx_u32) cap, 0); }
/* Скільки кадрів показано через nyx_present від старту системи (різниця за секунду = кадри/с гри). */
static inline nyx_u32 nyx_frames(void) { return (nyx_u32) nyx_syscall3(NYX_SC(54), 0, 0, 0); }

static inline int nyx_win_manage(void) { return nyx_syscall3(NYX_SC(41), 0, 0, 0); }
static inline int nyx_win_list(nyx_winfo_t* out, int max) { return nyx_syscall3(NYX_SC(42), (nyx_u32) out, (nyx_u32) max, 0); }
static inline int nyx_win_compose(nyx_u32* frame, int fw, int fh, int id) {
    return nyx_syscall3(NYX_SC(43), (nyx_u32) frame, ((nyx_u32) fw << 16) | (nyx_u32) fh, (nyx_u32) id);
}
static inline int nyx_win_set(int id, int x, int y) { return nyx_syscall3(NYX_SC(44), (nyx_u32) id, (nyx_u32) x, (nyx_u32) y); }
static inline int nyx_win_send(int id, nyx_u32 ev) { return nyx_syscall3(NYX_SC(45), (nyx_u32) id, ev, 0); }
static inline int nyx_mouse(nyx_mouse_t* m) {
    return nyx_syscall3(NYX_SC(35), (nyx_u32) m, 0, 0);
}

/* ------------------------------------------------------------------
 * Труби й перенаправлення. nyx_pipe(fds): fds[0] - читати, fds[1] -
 * писати. Читання з порожньої труби чекає (процес спить, не крутить
 * цикл), поки хтось не запише; коли всі кінці запису закриті - 0 (кінець).
 * nyx_dup2(old, new): new стає копією old - напр. nyx_dup2(fds[1], 1)
 * спрямовує printf у трубу. ------------------------------------------ */
static inline int nyx_pipe(int fds[2]) {
    return nyx_syscall3(NYX_SC(26), (nyx_u32) fds, 0, 0);
}

static inline int nyx_dup2(int oldfd, int newfd) {
    return nyx_syscall3(NYX_SC(27), (nyx_u32) oldfd, (nyx_u32) newfd, 0);
}

/* ------------------------------------------------------------------
 * Сигнали (номери як у Linux). Типова дія - завершити програму з кодом
 * 128+сигнал (Ctrl+C - SIGINT, код 130). nyx_signal ставить власний
 * обробник void f(int sig), NYX_SIG_IGN - ігнорувати, NYX_SIG_DFL -
 * повернути типову дію. SIGKILL не перехоплюється. ------------------ */
#define NYX_SIGINT  2
#define NYX_SIGKILL 9
#define NYX_SIGUSR1 10
#define NYX_SIGUSR2 12
#define NYX_SIGPIPE 13
#define NYX_SIGTERM 15
#define NYX_SIGCONT 18 /* 0.1.0.3: продовжити після паузи */
#define NYX_SIGSTOP 19 /* 0.1.0.3: пауза (процес не отримує час; не перехоплюється) */
#define NYX_SIG_DFL ((void (*)(int)) 0)
#define NYX_SIG_IGN ((void (*)(int)) 1)

/* Трамплін: сюди повертається обробник сигналу - syscall sigreturn
 * відновлює все, що програма робила до сигналу. */
__asm__ (
    ".text\n"
    ".globl nyx_sigreturn_trampoline\n"
    "nyx_sigreturn_trampoline:\n"
    "    mov $25, %eax\n"
    "    int $0x80\n"
    /* трамплін є в КОЖНІЙ програмі -> sigreturn (25) потрібен кожній; і під яку версію ABI зібрано */
    ".pushsection .nyxabi,\"\",@progbits\n.long 25\n.popsection\n"
    ".pushsection .nyxabi_ver,\"\",@progbits\n.long " NYX_STR(NYX_ABI) "\n.popsection\n"
);
void nyx_sigreturn_trampoline(void);

static inline void (*nyx_signal(int sig, void (*handler)(int)))(int) {
    return (void (*)(int)) nyx_syscall3(NYX_SC(24), (nyx_u32) sig, (nyx_u32) handler,
                                        (nyx_u32) nyx_sigreturn_trampoline);
}

/* 0 - надіслано, -1 - нема такого процесу чи він чужий. */
static inline int nyx_kill(int pid, int sig) {
    return nyx_syscall3(NYX_SC(23), (nyx_u32) pid, (nyx_u32) sig, 0);
}

#endif
