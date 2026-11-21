#!/usr/bin/env python3
"""make-nyxapp.py — додатки .nyxapp (F1, ядро NyxCore >= 0.1.0.30).

Формат (little-endian): «NYXA», u32 1, u32 довжина маніфесту, маніфест «ключ=значення» (name, version, author, type,
entry, needs, category, desc), u32 кількість файлів, файли {u16 довжина шляху, шлях, u32 розмір, дані}; у кінці
необов'язковий підпис: «SIG1» + 64 байти (офіційний ключ) або «AUT1» + 32 байти відкритого ключа автора + 64 байти
підпису Ed25519 усього, що перед «AUT1».

  make-nyxapp.py keygen автор.key
      новий ключ автора (закритий, hex) — бережи його: оновлення додатка приймаються лише з тим самим ключем.
  make-nyxapp.py pack вихід.nyxapp --name гра --version 1.0 --entry bin/гра [--author "Ім'я"] [--needs sound,fullscreen]
                 [--type native] [--category Ігри] [--desc "..."] [--author-key автор.key | --sign офіційний.key]
                 шлях_у_додатку=файл ...
      needs: net, sound, files (усі файли користувача), gamepad, fullscreen. Без них додаток бачить лише /system і
      свої теки (/apps/<назва>/, ~/.apps/<назва>/).
  make-nyxapp.py verify файл.nyxapp       що всередині й чим підписано
  make-nyxapp.py from-nyxpkg старий.nyxpkg вихід.nyxapp [--author-key автор.key] [--needs ...]
      перетворити пакет .nyxpkg (одна програма) на додаток (entry = bin/<назва>).
На NyxOS: «Файли» -> Enter на .nyxapp або `sudo app install файл.nyxapp [TAK]`.
"""
import struct
import sys

KEYS = ('name', 'version', 'author', 'type', 'entry', 'needs', 'category', 'desc')


def priv(path):
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
    return Ed25519PrivateKey.from_private_bytes(bytes.fromhex(open(path).read().strip()))


def pub_bytes(key):
    from cryptography.hazmat.primitives import serialization
    return key.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)


def fingerprint(pub):
    h = pub[:8].hex().upper()
    return '-'.join(h[i:i + 4] for i in range(0, 16, 4))


def build(meta, files, author_key=None, official_key=None):
    meta.setdefault('type', 'native')
    meta.setdefault('category', 'Інше')
    manifest = ''.join(f'{k}={meta[k]}\n' for k in KEYS if meta.get(k)).encode('utf-8')
    body = b'NYXA' + struct.pack('<II', 1, len(manifest)) + manifest + struct.pack('<I', len(files))
    for dst, data in files:
        p = dst.encode('utf-8')
        body += struct.pack('<H', len(p)) + p + struct.pack('<I', len(data)) + data
    if official_key:
        body += b'SIG1' + priv(official_key).sign(body)
    elif author_key:
        k = priv(author_key)
        body += b'AUT1' + pub_bytes(k) + k.sign(body)
    return body


def parse(b):
    if b[:4] != b'NYXA' or struct.unpack('<I', b[4:8])[0] != 1:
        sys.exit('це не .nyxapp версії 1')
    sig = 'немає'
    end = len(b)
    if len(b) > 80 and b[-68:-64] == b'SIG1':
        sig, end = 'офіційний (SIG1)', len(b) - 68
    elif len(b) > 112 and b[-100:-96] == b'AUT1':
        pub = b[-96:-64]
        try:
            from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey
            Ed25519PublicKey.from_public_bytes(pub).verify(b[-64:], b[:-100])
            good = 'збігається'
        except Exception:  # noqa: BLE001
            good = 'НЕ ЗБІГАЄТЬСЯ'
        sig, end = f'автора, ключ {fingerprint(pub)} ({good})', len(b) - 100
    ml = struct.unpack('<I', b[8:12])[0]
    manifest = b[12:12 + ml].decode('utf-8', 'replace')
    n = struct.unpack('<I', b[12 + ml:16 + ml])[0]
    p, files = 16 + ml, []
    for _ in range(n):
        pl = struct.unpack('<H', b[p:p + 2])[0]
        name = b[p + 2:p + 2 + pl].decode('utf-8', 'replace')
        sz = struct.unpack('<I', b[p + 2 + pl:p + 6 + pl])[0]
        files.append((name, sz))
        p += 6 + pl + sz
    if p != end:
        print(f'  УВАГА: дані закінчуються на {p}, а мали б на {end}')
    return manifest, files, sig


def opts(a, names):
    out, rest = {}, []
    while a:
        x = a.pop(0)
        if x.startswith('--') and x[2:] in names:
            out[x[2:]] = a.pop(0)
        else:
            rest.append(x)
    return out, rest


def main():
    a = sys.argv[1:]
    if not a:
        sys.exit(__doc__)
    cmd = a.pop(0)
    if cmd == 'keygen':
        from cryptography.hazmat.primitives import serialization
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
        k = Ed25519PrivateKey.generate()
        raw = k.private_bytes(serialization.Encoding.Raw, serialization.PrivateFormat.Raw, serialization.NoEncryption())
        with open(a[0], 'w') as f:
            f.write(raw.hex() + '\n')
        print(f'  ключ автора: {a[0]} (НЕ в git!), відбиток {fingerprint(pub_bytes(k))}')
    elif cmd == 'pack':
        out = a.pop(0)
        o, rest = opts(a, set(KEYS) | {'author-key', 'sign'})
        files = []
        for x in rest:
            if '=' not in x:
                sys.exit(f'make-nyxapp: незрозуміло: {x}')
            dst, src = x.split('=', 1)
            files.append((dst, open(src, 'rb').read()))
        if not o.get('name') or not o.get('version') or not o.get('entry') or not files:
            sys.exit('make-nyxapp: потрібні --name, --version, --entry і файли')
        if o['entry'] not in [f[0] for f in files]:
            sys.exit(f'make-nyxapp: entry {o["entry"]} нема серед файлів')
        b = build({k: o[k] for k in KEYS if k in o}, files, o.get('author-key'), o.get('sign'))
        open(out, 'wb').write(b)
        print(f'  {out}: {o["name"]} {o["version"]}, файлів {len(files)}, {len(b)} байт')
    elif cmd == 'verify':
        manifest, files, sig = parse(open(a[0], 'rb').read())
        print(manifest.rstrip())
        print(f'файлів: {len(files)}')
        for n, s in files:
            print(f'  {n} ({s} байт)')
        print(f'підпис: {sig}')
    elif cmd == 'from-nyxpkg':
        src, out = a.pop(0), a.pop(0)
        o, _ = opts(a, {'author-key', 'needs', 'author'})
        b = open(src, 'rb').read()
        if b[:4] != b'NYXP':
            sys.exit('це не .nyxpkg')
        name = b[4:36].rstrip(b'\0').decode()
        ver = b[36:52].rstrip(b'\0').decode()
        size = struct.unpack('<I', b[52:56])[0]
        meta = {'name': name, 'version': ver, 'entry': f'bin/{name}', 'author': o.get('author', ''),
                'needs': o.get('needs', ''), 'desc': f'перетворено з {name}.nyxpkg'}
        res = build(meta, [(f'bin/{name}', b[56:56 + size])], o.get('author-key'))
        open(out, 'wb').write(res)
        print(f'  {out}: {name} {ver} (з .nyxpkg), {len(res)} байт')
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
