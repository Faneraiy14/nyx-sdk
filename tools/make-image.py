#!/usr/bin/env python3
"""tools/make-image.py — картинка для програм NyxOS (.nyxi) з PNG/JPG (Е20, 20.11.2026).

  tools/make-image.py <вхід.png> <вихід.nyxi> [--size ШxВ] [--circle] [--cover ШxВ [--anchor 0..1]]

256 кольорів (палітра, згладжене приглушення похибки), індекс 0 — прозорий, якщо в картинці є прозорість (або
--circle: усе поза вписаним колом прозоре). Стиснення повторів (з диска програма читає до 256 КБ; з /system - без межі, ядро >= 0.1.0.5).
Формат (little-endian): "NYXI", u8 версія=1, u8 прапорці (1 - індекс 0 прозорий), u16 ширина, u16 висота,
  u16 кольорів, палітра кольорів x 3 (R,G,B), далі пари (кількість 1..255, індекс) рядок за рядком без розривів.
Малює libnyxgame.h: ng_image_load / ng_image_draw.
"""
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw


def main():
    args = sys.argv[1:]
    size, circle, cover, anchor = None, False, None, 0.4
    if '--anchor' in args:   # для --cover: де вирізати по висоті (0 - верх, 0.5 - центр, 1 - низ)
        i = args.index('--anchor'); anchor = float(args[i + 1]); del args[i:i + 2]
    if '--cover' in args:   # обрізати під пропорцію ШxВ по центру (шпалери 16:9 з квадрата), потім --size
        i = args.index('--cover'); cover = tuple(int(v) for v in args[i + 1].split('x')); del args[i:i + 2]
    if '--size' in args:
        i = args.index('--size'); size = tuple(int(v) for v in args[i + 1].split('x')); del args[i:i + 2]
    if '--circle' in args:
        args.remove('--circle'); circle = True
    src, out = args
    im = Image.open(src).convert('RGBA')
    if cover:
        w0, h0 = im.size
        if w0 * cover[1] > h0 * cover[0]:
            nw = h0 * cover[0] // cover[1]; im = im.crop(((w0 - nw) // 2, 0, (w0 - nw) // 2 + nw, h0))
        else:
            nh = w0 * cover[1] // cover[0]; top = int((h0 - nh) * anchor); im = im.crop((0, top, w0, top + nh))
    if size:
        im = im.resize(size, Image.LANCZOS)
    w, h = im.size
    alpha = im.split()[3]
    if circle:
        m = Image.new('L', (w * 4, h * 4), 0)
        ImageDraw.Draw(m).ellipse((0, 0, w * 4 - 1, h * 4 - 1), fill=255)
        alpha = Image.composite(alpha, Image.new('L', (w, h), 0), m.resize((w, h), Image.LANCZOS))
    transparent = alpha.getextrema()[0] < 128
    rgb = Image.new('RGB', (w, h), (0, 0, 0)); rgb.paste(im.convert('RGB'), (0, 0))
    q = rgb.quantize(colors=255 if transparent else 256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG)
    pal = q.getpalette()[:3 * (255 if transparent else 256)]
    idx = np.asarray(q).reshape(-1).tolist()
    a = np.asarray(alpha).reshape(-1).tolist()
    if transparent:
        idx = [0 if a[k] < 128 else v + 1 for k, v in enumerate(idx)]
        pal = [0, 0, 0] + pal
    ncol = len(pal) // 3
    body = bytearray()
    k = 0
    while k < len(idx):
        v = idx[k]; n = 1
        while k + n < len(idx) and idx[k + n] == v and n < 255:
            n += 1
        body += bytes((n, v)); k += n
    data = b'NYXI' + struct.pack('<BBHHH', 1, 1 if transparent else 0, w, h, ncol) + bytes(pal) + bytes(body)
    open(out, 'wb').write(data)
    print(f'  {out}: {w}x{h}, {ncol} кольорів{", прозорість" if transparent else ""}, {len(data)} байтів')
    # з ядра 0.1.0.5 файли /system читаються без межі розміру (UF_INITRD); з диска - до 256 КБ


if __name__ == '__main__':
    main()
