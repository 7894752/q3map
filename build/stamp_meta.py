"""Post-link metadata stamp for the "identical" build. Standalone (stdlib only). Never executes an image.

link.exe 7.00.9466 cannot reproduce three things of the original q3map.exe: the COFF/debug timestamp,
and the RSDS GUID and age. (The Rich header is NOT stamped any more: the natural link output equals the original's.)
`apply` writes the values in meta_stamp.json into a COPY of the freshly linked image. The RSDS path
string is produced by the linker itself (/PDBALTPATH, see build.py). The image checksum is 0 in the
original (no /RELEASE), so it needs no stamp.

  stamp_meta.py apply --input q3map.exe [--output q3map_identical.exe]
  stamp_meta.py check --input X.exe --original ORIGINAL.exe     # header/debug/Rich diff (needs your own copy)
  stamp_meta.py extract --original ORIGINAL.exe                 # regenerate meta_stamp.json (maintainers)
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct
import sys

POLICY = Path(__file__).resolve().parent / 'meta_stamp.json'


def rol(v, n):
    n &= 31
    return ((v << n) | (v >> (32 - n))) & 0xffffffff if n else v


def parse_rich(data):
    """Return (start, end, key, entries) of the Rich header, or None."""
    rich = data.find(b'Rich', 0x40, 0x400)
    if rich < 0:
        return None
    key = struct.unpack_from('<I', data, rich + 4)[0]
    dans = None
    for off in range(0x40, rich, 4):
        if struct.unpack_from('<I', data, off)[0] ^ key == 0x536e6144:  # 'DanS'
            dans = off
            break
    if dans is None:
        return None
    words = [struct.unpack_from('<I', data, o)[0] ^ key for o in range(dans + 16, rich, 4)]
    entries = [(words[i], words[i + 1]) for i in range(0, len(words), 2)]
    return dans, rich + 8, key, entries


def rich_key(data, dans, entries):
    cs = dans
    for i in range(dans):
        if 0x3c <= i < 0x40:
            continue  # e_lfanew is excluded from the checksum
        cs = (cs + rol(data[i], i)) & 0xffffffff
    for comp, count in entries:
        cs = (cs + rol(comp, count)) & 0xffffffff
    return cs


def build_rich(data, dans, entries):
    key = rich_key(data, dans, entries)
    words = [0x536e6144, 0, 0, 0]
    for comp, count in entries:
        words += [comp, count]
    blob = b''.join(struct.pack('<I', w ^ key) for w in words) + b'Rich' + struct.pack('<I', key)
    return blob, key


def locate(data):
    lf = struct.unpack_from('<I', data, 0x3c)[0]
    assert data[lf:lf + 4] == b'PE\0\0'
    nsec, = struct.unpack_from('<H', data, lf + 6)
    optsz, = struct.unpack_from('<H', data, lf + 20)
    opt = lf + 24
    sections = []
    for i in range(nsec):
        o = opt + optsz + 40 * i
        vsz, va, rawsz, rawptr = struct.unpack_from('<IIII', data, o + 8)
        sections.append((va, max(vsz, rawsz), rawptr))
    dbg_rva, dbg_size = struct.unpack_from('<II', data, opt + 96 + 6 * 8)

    def fileoff(rva):
        for va, sz, raw in sections:
            if va <= rva < va + sz:
                return raw + rva - va
        raise ValueError(hex(rva))
    dbg = []
    for i in range(dbg_size // 28):
        o = fileoff(dbg_rva) + 28 * i
        ts, maj, mnr, typ, sz, rva, ptr = struct.unpack_from('<IHHIIII', data, o + 4)
        dbg.append({'entry_offset': o, 'type': typ, 'size': sz, 'raw_ptr': ptr})
    return {'pe': lf, 'coff_timestamp': lf + 8, 'debug': dbg}



def extract_policy(path):
    data = path.read_bytes()
    loc = locate(data)
    cv = [d for d in loc['debug'] if d['type'] == 2]
    assert len(cv) == 1
    p = cv[0]['raw_ptr']
    assert data[p:p + 4] == b'RSDS'
    guid = data[p + 4:p + 20]
    age, = struct.unpack_from('<I', data, p + 20)
    ts, = struct.unpack_from('<I', data, loc['coff_timestamp'])
    policy = {
        'source': 'original q3map.exe (values only)',
        'timestamp': ts,
        'rsds_guid_hex': guid.hex(),
        'rsds_age': age,
        'notes': 'timestamp/GUID/age cannot be produced by link.exe 7.00.9466; stamped by stamp_meta.py apply',
    }
    return policy


def apply_policy(data, policy):
    data = bytearray(data)
    loc = locate(data)
    struct.pack_into('<I', data, loc['coff_timestamp'], policy['timestamp'])
    for d in loc['debug']:
        struct.pack_into('<I', data, d['entry_offset'] + 4, policy['timestamp'])
        if d['type'] == 2:
            p = d['raw_ptr']
            assert data[p:p + 4] == b'RSDS'
            data[p + 4:p + 20] = bytes.fromhex(policy['rsds_guid_hex'])
            struct.pack_into('<I', data, p + 20, policy['rsds_age'])
    return bytes(data)


def diff_report(a, b):
    """Header-level diff of two images (list of strings); a = target, b = candidate."""
    out = []
    la, lb = locate(a), locate(b)
    ra, rb = parse_rich(a), parse_rich(b)
    if ra and rb:
        ea, eb = dict((c, n) for c, n in ra[3]), dict((c, n) for c, n in rb[3])
        for c in sorted(set(ea) | set(eb)):
            if ea.get(c) != eb.get(c):
                out.append('rich %#x prod=%#x build=%d: target=%s candidate=%s' % (c, c >> 16, c & 0xffff, ea.get(c), eb.get(c)))
        if ra[2] != rb[2]:
            out.append('rich key target=%#x candidate=%#x' % (ra[2], rb[2]))
    for name, off in (('coff_timestamp', 8),):
        va = struct.unpack_from('<I', a, la['pe'] + off)[0]
        vb = struct.unpack_from('<I', b, lb['pe'] + off)[0]
        if va != vb:
            out.append('%s target=%#x candidate=%#x' % (name, va, vb))
    for da, db in zip(la['debug'], lb['debug']):
        pa, pb = da['raw_ptr'], db['raw_ptr']
        if a[pa:pa + da['size']] != b[pb:pb + db['size']]:
            out.append('debug type %d record differs: %r vs %r' % (da['type'], a[pa:pa + da['size']], b[pb:pb + db['size']]))
    return out



def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=['extract', 'apply', 'check'])
    ap.add_argument('--input', type=Path)
    ap.add_argument('--output', type=Path)
    ap.add_argument('--original', type=Path, help='your own copy of the original q3map.exe (never stored in the repo)')
    args = ap.parse_args()
    if args.cmd == 'extract':
        if not args.original:
            raise SystemExit('--original required')
        POLICY.write_text(json.dumps(extract_policy(args.original), indent=2) + chr(10), encoding='utf-8')
        print('wrote', POLICY)
        return 0
    if not args.input:
        raise SystemExit('--input required')
    if args.cmd == 'apply':
        dst = args.output or args.input.with_name(args.input.stem + '_identical' + args.input.suffix)
        if dst.resolve() == args.input.resolve():
            raise SystemExit('refusing to overwrite the unstamped link output')
        dst.write_bytes(apply_policy(args.input.read_bytes(), json.loads(POLICY.read_text(encoding='utf-8'))))
        print('stamped', dst)
        return 0
    if not args.original:
        raise SystemExit('--original required')
    rep = diff_report(args.original.read_bytes(), args.input.read_bytes())
    print(chr(10).join(rep) if rep else 'headers/debug/Rich equal to the original')
    return 0


if __name__ == '__main__':
    sys.exit(main())
