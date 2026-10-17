#!/usr/bin/env python3
"""Fetch the Visual C++ 7.0 toolchain needed by build.py from public archive.org images.

Python 3, standard library only; needs 7-Zip (7z.exe) for extraction. Nothing from Microsoft is part of this
repository: this script downloads the retail images named in toolchain_manifest.json, verifies their SHA-256,
extracts ONLY the files listed in the manifest (no installer or executable from the images is ever run),
verifies every extracted file against the manifest hashes and deletes the downloads again.

    python build/fetch_toolchain.py                  # -> build/toolchain/vc7
    python build/fetch_toolchain.py --7z "C:\\Program Files\\7-Zip\\7z.exe" --keep

Result layout (the --vc7 folder of build.py):  toolchain/vc7/{bin,include,lib,PlatformSDK}
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

HERE = Path(__file__).resolve().parent
MANIFEST = HERE / 'toolchain_manifest.json'
DEFAULT_DEST = HERE / 'toolchain'
URL = 'https://archive.org/download/%s/%s'


def log(*a):
    print(*a, flush=True)


def sha256(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def remove(p):
    """Delete a temporary file; on Windows a scanner may hold it briefly, so retry and never fail the fetch."""
    for _ in range(10):
        try:
            os.chmod(p, 0o666)
            Path(p).unlink()
            return
        except FileNotFoundError:
            return
        except OSError:
            time.sleep(2)
    log('  warning: could not delete', p)


def find_7z(arg):
    cands = [arg] if arg else []
    cands += [os.environ.get('SEVENZIP'), shutil.which('7z'), shutil.which('7za'), shutil.which('7zz'),
              r'C:\Program Files\7-Zip\7z.exe', r'C:\Program Files (x86)\7-Zip\7z.exe']
    for c in cands:
        if c and Path(c).is_file():
            return str(Path(c))
    raise SystemExit('7-Zip not found. Install it (https://www.7-zip.org/), put 7z on PATH or pass --7z <path to 7z.exe>.')


def run7z(sevenzip, args, cwd):
    # exit code 1 is a warning (e.g. "data after the end of archive" on the CD images): the files are verified later
    r = subprocess.run([sevenzip, *args], cwd=str(cwd), stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if r.returncode not in (0, 1):
        raise SystemExit('7z failed (exit %d): %s\n%s' % (r.returncode, ' '.join(args), r.stdout.decode('mbcs', 'replace')[-2000:]))


def download(src, target):
    """Download with resume and retries; verify size and sha256."""
    url = URL % (src['item'], urllib.parse.quote(src['filename']))
    if target.exists() and target.stat().st_size == src['size'] and sha256(target) == src['sha256']:
        log('  already downloaded:', target.name)
        return
    part = target.with_suffix(target.suffix + '.part')
    for attempt in range(1, 7):
        have = part.stat().st_size if part.exists() else 0
        if have >= src['size']:
            break
        req = urllib.request.Request(url, headers={'Range': 'bytes=%d-' % have} if have else {})
        last = -10
        try:
            with urllib.request.urlopen(req, timeout=60) as r, open(part, 'ab' if have else 'wb') as f:
                if have and r.status != 206:
                    f.seek(0)
                    f.truncate()
                while True:
                    chunk = r.read(1 << 20)
                    if not chunk:
                        break
                    f.write(chunk)
                    pct = int(100 * f.tell() / src['size'])
                    if pct >= last + 10:
                        last = pct - pct % 10
                        print(' %d%%' % last, end='', flush=True)
            print()
        except (urllib.error.URLError, OSError, TimeoutError) as e:
            print()
            log('  download problem (%s), retry %d' % (e, attempt))
            time.sleep(5 * attempt)
    if not part.exists() or part.stat().st_size != src['size']:
        raise SystemExit('download incomplete: ' + url)
    part.replace(target)
    got = sha256(target)
    if got != src['sha256']:
        target.unlink()
        raise SystemExit('SHA-256 mismatch for %s: got %s, expected %s' % (src['filename'], got, src['sha256']))
    log('  sha256 ok:', src['filename'])


def mdf_to_iso(mdf, iso):
    """Strip the raw CD sector framing (MDF/BIN, 2352 or 2448 bytes per sector) to a plain 2048-byte-sector ISO."""
    with open(mdf, 'rb') as f:
        for ssz in (2448, 2352):
            for off in (16, 24):                    # mode 1 / mode 2 form 1 user data
                f.seek(16 * ssz + off)
                if f.read(6)[1:] == b'CD001':
                    break
            else:
                continue
            break
        else:
            raise SystemExit('not a recognised CD image: ' + str(mdf))
        f.seek(0)
        with open(iso, 'wb') as out:
            while True:
                buf = f.read(ssz * 2048)
                if not buf:
                    break
                for i in range(0, len(buf) - ssz + 1, ssz):
                    out.write(buf[i + off:i + off + 2048])


def extract_list(sevenzip, archive, names, outdir):
    lst = outdir.parent / (outdir.name + '.list')
    lst.write_text('\n'.join(names) + '\n', encoding='utf-8')
    run7z(sevenzip, ['x', '-y', '-scsUTF-8', '-o' + str(outdir), str(archive), '@' + str(lst)], outdir.parent)
    lst.unlink()


def fetch_source(name, src, files, sevenzip, work, keep):
    log('[%s] %s  (%.0f MB)' % (name, src['title'], src['size'] / 1e6))
    arc = work / src['filename']
    download(src, arc)
    img = work / (name + '_img')
    img.mkdir(parents=True, exist_ok=True)
    run7z(sevenzip, ['x', '-y', '-r', '-o' + str(img), str(arc), src['image']], work)
    mdfs = [p for p in img.rglob('*') if p.is_file() and p.suffix.lower() in ('.mdf', '.bin', '.iso')]
    if len(mdfs) != 1:
        raise SystemExit('expected exactly one disc image in ' + src['filename'])
    iso = work / (name + '.iso')
    if mdfs[0].suffix.lower() == '.iso':
        iso = mdfs[0]
    else:
        mdf_to_iso(mdfs[0], iso)
    shutil.rmtree(img, ignore_errors=True) if iso != mdfs[0] else None
    if not keep:
        remove(arc)
    out = work / (name + '_out')
    shutil.rmtree(out, ignore_errors=True)
    out.mkdir(parents=True)
    if name == 'psdk':
        cabs = sorted({f['cab'] for f in files})
        extract_list(sevenzip, iso, cabs, out)
        for cab in cabs:
            members = [f['member'] for f in files if f['cab'] == cab]
            cdir = out / ('m_' + Path(cab).stem)
            cdir.mkdir()
            extract_list(sevenzip, out / cab, members, cdir)
    else:
        extract_list(sevenzip, iso, sorted({f['path'] for f in files}), out)
    return iso, out


def install(files, name, out, dest):
    for f in files:
        if name == 'psdk':
            srcp = out / ('m_' + Path(f['cab']).stem) / f['member']
        else:
            srcp = out / Path(*f['path'].split('/'))
        if not srcp.is_file():
            raise SystemExit('missing in image: ' + (f.get('member') or f['path']))
        got = sha256(srcp)
        if got != f['sha256']:
            raise SystemExit('SHA-256 mismatch for %s (from %s): got %s, expected %s' % (f['dest'], name, got, f['sha256']))
        target = (dest / f['dest']).resolve()
        if dest.resolve() not in target.parents:
            raise SystemExit('refusing to write outside the toolchain folder: ' + f['dest'])
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(srcp, target)


def verify(files, dest):
    bad = []
    for f in files:
        p = dest / f['dest']
        if not p.is_file() or p.stat().st_size != f['size'] or sha256(p) != f['sha256']:
            bad.append(f['dest'])
    return bad


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--dest', default=str(DEFAULT_DEST), help='toolchain folder (default build/toolchain)')
    ap.add_argument('--7z', dest='sevenzip', help='path to 7z.exe (default: PATH, %%SEVENZIP%%, Program Files\\7-Zip)')
    ap.add_argument('--keep', action='store_true', help='keep the downloaded .7z archives (default: delete them)')
    ap.add_argument('--verify-only', action='store_true', help='only check the files already in --dest')
    ap.add_argument('--work', help='temporary folder for downloads and extraction (default <dest>/_download)')
    args = ap.parse_args()

    man = json.loads(MANIFEST.read_text(encoding='utf-8'))
    dest = Path(args.dest).resolve()
    files = man['files']
    bad = verify(files, dest)
    if not bad:
        log('toolchain in %s is complete: %d files verified' % (dest, len(files)))
        return 0
    if args.verify_only:
        log('%d of %d files missing or differing, e.g. %s' % (len(bad), len(files), bad[:3]))
        return 1

    sevenzip = find_7z(args.sevenzip)
    work = Path(args.work).resolve() if args.work else dest / '_download'
    work.mkdir(parents=True, exist_ok=True)
    free = shutil.disk_usage(work).free
    log('7-Zip: %s; free disk space: %.1f GB (about 2.5 GB needed temporarily)' % (sevenzip, free / 1e9))
    if free < 3e9:
        raise SystemExit('not enough free disk space on ' + str(work))
    todo = sorted({f['source'] for f in files if f['dest'] in set(bad)})
    for name in todo:
        sel = [f for f in files if f['source'] == name and f['dest'] in set(bad)]
        iso, out = fetch_source(name, man['sources'][name], sel, sevenzip, work, args.keep)
        install(sel, name, out, dest)
        shutil.rmtree(out, ignore_errors=True)
        if iso.exists() and work in iso.parents:
            remove(iso)
        log('  installed %d files from %s' % (len(sel), name))
    if not args.keep:
        shutil.rmtree(work, ignore_errors=True)
    bad = verify(files, dest)
    if bad:
        raise SystemExit('verification failed: %s' % bad[:5])
    log('toolchain ready: %s (%d files, all SHA-256 verified)' % (dest / 'vc7', len(files)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
