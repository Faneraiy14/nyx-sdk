#!/usr/bin/env python3
"""make-nyxlib.py — зібрати бібліотеку/інструмент .nyxlib (B1, ядро NyxCore >= 0.1.0.26).

Формат (little-endian): «NYXL», u32 1, u32 довжина маніфесту, маніфест «ключ=значення» (name, version, category, desc),
u32 кількість файлів, файли {u16 довжина шляху, шлях, u32 розмір, дані}; необов'язково «SIG1» + Ed25519 усього до нього.

  tools/make-nyxlib.py вихід.nyxlib --name hello-tools --version 1.0 [--category Розробка] [--desc "..."]
                       [--sign ключ.hex] шлях_у_пакеті=файл ...

  bin/<команда>=програма.bin  — команда (shell знайде її через реєстр /etc/commands після `sudo lib install`)
  --sign  — закритий ключ Ed25519 (hex, як ~/.nyxos-signing-key у make-pkg.py). Без підпису ядро встановить пакет лише
            з «TAK» (непідписаний); підпис іншим ключем — відмова (ядро довіряє лише офіційному).
"""
import struct
import sys


def main():
    a = sys.argv[1:]
    if not a or a[0].startswith('-'):
        sys.exit(__doc__)
    out = a.pop(0)
    meta, sign, files = {'category': 'Інше', 'desc': ''}, None, []
    while a:
        x = a.pop(0)
        if x in ('--name', '--version', '--category', '--desc'):
            meta[x[2:]] = a.pop(0)
        elif x == '--sign':
            sign = a.pop(0)
        elif '=' in x:
            dst, src = x.split('=', 1)
            files.append((dst, open(src, 'rb').read()))
        else:
            sys.exit(f'make-nyxlib: незрозуміло: {x}')
    if 'name' not in meta or 'version' not in meta or not files:
        sys.exit('make-nyxlib: потрібні --name, --version і хоч один файл')
    manifest = ''.join(f'{k}={meta[k]}\n' for k in ('name', 'version', 'category', 'desc')).encode('utf-8')
    body = b'NYXL' + struct.pack('<II', 1, len(manifest)) + manifest + struct.pack('<I', len(files))
    for dst, data in files:
        p = dst.encode('utf-8')
        body += struct.pack('<H', len(p)) + p + struct.pack('<I', len(data)) + data
    if sign:
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
        key = Ed25519PrivateKey.from_private_bytes(bytes.fromhex(open(sign).read().strip()))
        body += b'SIG1' + key.sign(body)
    open(out, 'wb').write(body)
    print(f'  {out}: {meta["name"]} {meta["version"]}, файлів {len(files)}, {"підписано" if sign else "без підпису"}, {len(body)} байт')


if __name__ == '__main__':
    main()
