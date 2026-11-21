# NyxOS SDK

SDK для програм і ігор під **NyxOS** — власну операційну систему з нуля (ядро **NyxCore**, x86 32-bit).
Ядро закрите; цей SDK відкритий — щоб будь-хто міг писати програми для NyxOS.

*SDK for writing programs and games for NyxOS, a from-scratch operating system (NyxCore kernel, x86 32-bit).
The kernel is closed source; this SDK is public.*

## Що всередині

| Тека | Що це |
|---|---|
| `include/libnyx.h` | системні виклики ядра: файли, процеси (fork/exec/wait), труби, сигнали, час, екран, клавіатура, миша, вікна (зокрема зміна розміру — «на весь екран»), буфер обміну, стеження за системними викликами (nyx_trace), сповіщення (nyx_notify) |
| `include/nyxc.h` | міні-libc: `printf`, `malloc`/`free`, рядки |
| `include/libnyxgame.h` | ігри й графіка: кадр на весь екран із цілим збільшенням, фігури, текст (згладжений шрифт системи або піксельний 8x16), картинки `.nyxi`, ввід |
| `lib/` | `start.s` (вхід у `main(argc, argv)`) і `user.ld` (програма — плоский бінарник з адреси 0x40000000) |
| `tools/nyx-build.sh` | збірка + **перевірка ABI**: програма, якій бракує системного виклику ядра, не збереться |
| `tools/make-image.py` | PNG → `.nyxi` (картинки для програм) |
| `tools/make-nyxlib.py` | зібрати бібліотеку чи інструмент `.nyxlib` (у NyxOS: `sudo lib install файл.nyxlib`) |
| `abi.txt` | таблиця системних викликів ядра цієї версії (`VERSION`) |
| `template/main.c` | шаблон: квадрат рухається стрілками |
| `examples/` | приклади: `hello` (файли), `ngtest` (графіка), `frametest` (кадр), `inputtest` (клавіатура й миша) |

## Як зібрати програму

Потрібно: Linux, `gcc` (з підтримкою `-m32`), `binutils`, `python3`.

```bash
tools/nyx-build.sh build template/main.c    # -> build/main.bin
```

Готовий `main.bin` поклади в NyxOS у `/games` — з'явиться в Ігровому центрі; у `/bin` — стане командою.

## Версії

SDK випускається разом із ядром NyxCore: `VERSION` — версія ядра, `abi.txt` — його системні виклики.
Новий виклик з'являється в кінці таблиці; номери ніколи не перевикористовуються.

## Ліцензії

- Піксельний шрифт `include/nyx_font8x16.h` згенеровано з [JetBrains Mono](https://github.com/JetBrains/JetBrainsMono)
  (Apache License 2.0, текст — `licenses/Apache-2.0.txt`).
- Сам SDK — **zlib** (`LICENSE`): можна використовувати будь-де, і в комерційних іграх; змінювати й поширювати; у
  готових програмах (бінарниках) нічого вказувати не треба — лише не видавати SDK за свій і не прибирати повідомлення з
  вихідних текстів. *The SDK itself is under the zlib license (see `LICENSE`).*
- Ядро NyxCore, яке виконує програми, — закрите й до SDK не входить.
