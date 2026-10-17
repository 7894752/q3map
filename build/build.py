#!/usr/bin/env python3
"""Standalone build of q3map.exe with your own Visual C++ 7.0 (VS .NET 2002) install.

Python 3, standard library only. Reads build/sources.json (static manifest: per-TU flag sets, link
order, link options); needs no other tooling. Never executes the output.

    python build.py --vc7 "C:\\Program Files\\Microsoft Visual Studio .NET\\Vc7"
    python build.py --vc7 <Vc7> --identical      # also write q3map_identical.exe (timestamp/GUID/age/Rich stamp)

The VC7 root is the folder that contains bin\\cl.exe, include\\, lib\\ and PlatformSDK\\ (or the
VCINSTALLDIR environment variable of a VS command prompt). --sdk overrides <vc7>\\PlatformSDK
(include\\ and lib\\ of the Platform SDK that provides GL\\glaux.h and glaux.lib).
"""
from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import threading

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
MANIFEST = HERE / 'sources.json'

# sha256 of the output of the original (1.5 MB-class, 2003) q3map.exe. Only the hash is stored.
ORIGINAL_SHA256 = 'b7a3cbb2d003e241e97b87d824aa6c61b29cb9b42481641eaca6aed07bc6226a'

# Toolchain files the byte-exact result was verified with (VC 7.00.9466 / cl 13.00.9466). A mismatch is a
# warning (error with --strict): other builds will compile, but are not expected to give identical bytes.
EXPECTED = {
    'bin/cl.exe': '05be307f04d8c3a9c211b77e85a63992be4642f14b4de31c04f9bb3d7bb1d65f',
    'bin/link.exe': '1560d85164dfb763dd13f90f33ff9c6f1cd80f2afada03eed8d0a314593581f1',
    'bin/lib.exe': '8973d6e9643ea23e1815623252ee30cecc64dd1ed2071003e889ec2ca3013dca',
    'lib/libcmt.lib': '8815af7b9b6e0e28b77708ede25ab7ecfc4b05e1d8811f092c516cff5ce19d94',
    'lib/libcpmt.lib': '92d0b6d397e420138046b286a824bc8cc70c90a96f69042139d181b88e2eb4d8',
    'lib/oldnames.lib': '61d891d7f53dee0b38d22d6dce04374ce85e101a5a1b6c981269274979f073c5',
}
EXPECTED_SDK = {
    'lib/glaux.lib': 'da077333b948e6f99c26d676cedb47aebf528a5d4b5ffb114b7ae686afff1482',
}

CREATE_NO_WINDOW = 0x08000000 if os.name == 'nt' else 0
print_lock = threading.Lock()

# Optional Wine mode (experimental, untested by the maintainers): every tool runs as `wine tool.exe ...` and
# every path handed to a tool is converted to a Windows path (`winepath -w`, fallback: Z: drive mapping).
WINE = None        # list such as ['wine'] when active
DRY_RUN = False    # print the commands instead of running them
_wpath_cache = {}


def wpath(p):
    """Path as the tool must see it: unchanged on Windows, Windows-style under Wine."""
    p = str(p)
    if WINE is None:
        return p
    if p in _wpath_cache:
        return _wpath_cache[p]
    if len(p) > 1 and p[1] == ':' and p[0].isalpha():      # already a Windows path
        w = p
    elif DRY_RUN:
        w = 'Z:' + p.replace('/', chr(92))
    else:
        try:
            r = subprocess.run(['winepath', '-w', p], capture_output=True, timeout=60)
            w = r.stdout.decode('utf-8', 'replace').strip() if r.returncode == 0 else ''
        except OSError:
            w = ''
        if not w:
            w = 'Z:' + p.replace('/', chr(92))
    _wpath_cache[p] = w
    return w


def sub(base, name):
    """base/name, resolved case-insensitively (Platform SDK folders are `Include`/`Lib` in some installs)."""
    d = Path(base) / name
    if d.exists() or not Path(base).is_dir():
        return d
    for e in Path(base).iterdir():
        if e.name.lower() == name.lower():
            return e
    return d


def log(*a):
    with print_lock:
        print(*a, flush=True)


def sha256(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


FETCHED = HERE / 'toolchain' / 'vc7'    # written by fetch_toolchain.py


def find_vc7(arg):
    if DRY_RUN and arg:
        return Path(arg.strip('"').rstrip(chr(92) + '/'))
    cand = arg or os.environ.get('VCINSTALLDIR')
    if not cand and (FETCHED / 'bin' / 'cl.exe').is_file():
        cand = str(FETCHED)
    if not cand:
        raise SystemExit('Pass --vc7 <path to the Vc7 folder>, set VCINSTALLDIR, or run with --fetch-toolchain.')
    p = Path(cand.strip('"').rstrip('\\/'))
    for c in (p, p / 'Vc7'):
        if (c / 'bin' / 'cl.exe').is_file():
            return c.resolve()
    raise SystemExit('No bin\\cl.exe under ' + str(p))


def tool_env(vc7):
    env = os.environ.copy()
    for k in ('CL', '_CL_', 'LINK', '_LINK_', 'INCLUDE', 'LIB', 'LIBPATH'):
        env.pop(k, None)
    extra = [vc7 / 'bin']
    for rel in ('../Common7/IDE', '../Common7/Tools', '../Common7/Tools/bin'):  # mspdb70.dll lives in Common7\IDE
        d = (vc7 / rel).resolve()
        if d.is_dir():
            extra.append(d)
    if WINE is None:
        env['PATH'] = os.pathsep.join([str(d) for d in extra] + [env.get('PATH', '')])
    else:
        # Wine extends the Windows PATH of the prefix from WINEPATH (Windows paths, ';' separated)
        env['WINEPATH'] = ';'.join([wpath(d) for d in extra] + ([env['WINEPATH']] if env.get('WINEPATH') else []))
    return env


def run(cmd, cwd, env, timeout=600):
    """cmd[0] is the tool exe (native path); the other items are already-converted arguments."""
    if WINE is not None:
        tool, args = cmd[0], list(cmd[1:])
        cmd = [*WINE, tool, *args]
        if sum(len(a) + 1 for a in cmd) > 1800:          # long command lines go through a response file
            rsp = Path(cwd) / ('cmd_%d.rsp' % (abs(hash(tuple(args))) % 10**8))
            if not DRY_RUN:
                rsp.write_text(chr(10).join('"' + a + '"' for a in args) + chr(10), encoding='ascii')
            cmd = [*WINE, tool, '@' + wpath(rsp)]
    if DRY_RUN:
        log('DRY-RUN (cwd %s): %s' % (cwd, ' '.join('"%s"' % a if ' ' in a else a for a in cmd)))
        return 0, ''
    r = subprocess.run(cmd, cwd=str(cwd), env=env, capture_output=True, timeout=timeout, creationflags=CREATE_NO_WINDOW)
    return r.returncode, (r.stdout + r.stderr).decode('mbcs' if os.name == 'nt' else 'utf-8', 'replace')


def compile_one(tu, flag_sets, vc7, sdk, out, env):
    src = ROOT / tu['source']
    if not DRY_RUN and not src.is_file():
        return tu, 1, 'missing source ' + str(src)
    stem = tu['obj'][:-4]
    work = out / 'work' / stem
    work.mkdir(parents=True, exist_ok=True)
    flags = flag_sets[tu['flags']]
    obj = out / 'obj' / tu['obj']
    # Same include search as the verified build: the TU's own directory, src, then the toolchain.
    cmd = [str(vc7 / 'bin' / 'cl.exe'), *flags, '/I' + wpath(src.parent), '/I' + wpath(ROOT / 'src'),
           '/I' + wpath(sub(vc7, 'include')), '/I' + wpath(sub(sdk, 'include')),
           '/Fo' + wpath(obj), '/Fd' + wpath(work / 'compiler.pdb')]
    e = dict(env)
    if '/YX' in flags:
        cmd.append('/Fp' + wpath(work / 'auto.pch'))   # private PCH per TU, deleted afterwards
    else:
        e['TMP'] = e['TEMP'] = wpath(work)             # cl.exe hangs with /YX under a TMP override
    cmd.append(wpath(src))
    code, text = run(cmd, work, e)
    if DRY_RUN:
        return tu, 0, ''
    (work / 'compile.log').write_text(text, encoding='utf-8')
    for junk in ('auto.pch', 'compiler.pdb'):
        try:
            (work / junk).unlink()
        except OSError:
            pass
    return tu, code if obj.exists() else (code or 1), text


def compile_all(man, vc7, sdk, out, env, jobs, keep_going):
    (out / 'obj').mkdir(parents=True, exist_ok=True)
    tus = man['translation_units']
    failed = []
    done = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        futs = [ex.submit(compile_one, tu, man['compile_flag_sets'], vc7, sdk, out, env) for tu in tus]
        for f in concurrent.futures.as_completed(futs):
            tu, code, text = f.result()
            done += 1
            if code:
                failed.append(tu['source'])
                log('FAILED  [%d/%d] %s\n%s' % (done, len(tus), tu['source'], text))
                if not keep_going:
                    for g in futs:
                        g.cancel()
                    break
            else:
                log('compiled [%d/%d] %s (%s)' % (done, len(tus), tu['source'], tu['flags']))
    return failed


def link_all(man, vc7, sdk, out, env, pdbaltpath, objdir=None):
    obj = Path(objdir) if objdir else out / 'obj'

    # --- the original solution built jpeg-6 (+jpgload), cmdlib and NvTriStrip as static libraries; the linker
    # pulls their members in reference order, which gives the original object order.
    libdir = out / 'lib'
    libdir.mkdir(parents=True, exist_ok=True)
    libpath = {}
    for name, members in man['static_libraries'].items():
        lib = libdir / (name + '.lib')
        if lib.exists():
            lib.unlink()
        code, text = run([str(vc7 / 'bin' / 'lib.exe'), '/NOLOGO', '/OUT:' + wpath(lib), *[wpath(obj / m) for m in members]],
                         libdir, env)
        if code:
            raise SystemExit('lib.exe failed for %s:%s%s' % (name, chr(10), text))
        libpath['@' + name] = wpath(lib)

    flags = list(man['link_flags'])
    flags += ['/PDB:' + wpath(out / 'q3map.pdb'), '/OUT:' + wpath(out / 'q3map.exe'), '/MAP:' + wpath(out / 'q3map.map'),
              '/LIBPATH:' + wpath(sub(vc7, 'lib')), '/LIBPATH:' + wpath(sub(sdk, 'lib'))]
    flags = [f for f in flags if not f.startswith('/PDBALTPATH:')]
    if pdbaltpath:
        flags.append('/PDBALTPATH:' + pdbaltpath)
    flags += ['/INCLUDE:' + s for s in man.get('link_include_roots', [])]
    inputs = [wpath(obj / t['obj']) for t in man['translation_units'] if t['project'] == 'q3map']
    libs = [libpath.get(l, l) for l in man['link_libraries']]
    rsp = out / 'q3map.rsp'
    rsp.write_text('\n'.join('"' + a + '"' for a in flags + inputs + libs) + '\n', encoding='ascii')
    code, text = run([str(vc7 / 'bin' / 'link.exe'), '@' + wpath(rsp)], out, env)
    if DRY_RUN:
        return out / 'q3map.exe'
    (out / 'link.log').write_text(text, encoding='utf-8')
    if code:
        print(text)
        raise SystemExit('link.exe failed (exit %d)' % code)
    return out / 'q3map.exe'


def check_toolchain(vc7, sdk, strict):
    bad = []
    for base, table in ((vc7, EXPECTED), (sdk, EXPECTED_SDK)):
        for rel, want in table.items():
            p = base / rel
            if not p.is_file():
                bad.append('missing  ' + str(p))
            elif sha256(p) != want:
                bad.append('differs  ' + str(p))
    if bad:
        log('WARNING: toolchain files differ from the verified VC7.0 (13.00.9466 / 7.00.9466) install:')
        for b in bad:
            log('   ' + b)
        if strict:
            raise SystemExit('--strict: toolchain mismatch')
    else:
        log('toolchain: all verified files match')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--vc7', help='Vc7 root (contains bin\\cl.exe); default: %%VCINSTALLDIR%%')
    ap.add_argument('--sdk', help='Platform SDK root (include\\, lib\\); default <vc7>\\PlatformSDK')
    ap.add_argument('--out', default=str(HERE / 'out'), help='output directory (default build\\out)')
    ap.add_argument('--identical', action='store_true',
                    help='also write q3map_identical.exe: timestamp, RSDS GUID/age and Rich header of the original')
    ap.add_argument('--fetch-toolchain', action='store_true',
                    help='download the VC7.0 toolchain into build/toolchain first (see fetch_toolchain.py; needs 7-Zip)')
    ap.add_argument('--7z', dest='sevenzip', help='path to 7z.exe for --fetch-toolchain')
    ap.add_argument('--keep-downloads', action='store_true', help='with --fetch-toolchain: keep the downloaded archives')
    ap.add_argument('--jobs', type=int, default=2, help='parallel cl.exe processes (default 2)')
    ap.add_argument('--keep-going', action='store_true', help='continue after a failed compile')
    ap.add_argument('--strict', action='store_true', help='fail if toolchain file hashes differ from the verified install')
    ap.add_argument('--link-only', action='store_true', help='reuse the objects already in <out>\\obj')
    ap.add_argument('--clean', action='store_true', help='delete <out> first')
    ap.add_argument('--wine', nargs='?', const='wine', default=None, metavar='WINE_CMD',
                    help='EXPERIMENTAL (untested): run the VC7 tools under Wine (default command "wine"); '
                         'enabled automatically when not on Windows')
    ap.add_argument('--dry-run', action='store_true',
                    help='print the tool command lines (after path conversion) without running anything')
    args = ap.parse_args()
    global WINE, DRY_RUN
    DRY_RUN = args.dry_run
    if args.wine is None and os.name != 'nt':
        args.wine = 'wine'
        log('Not running on Windows: using Wine to run the VC7 tools (experimental, untested; see build/README.md).')
    if args.wine is not None:
        import shlex
        WINE = shlex.split(args.wine)

    man = json.loads(MANIFEST.read_text(encoding='utf-8'))
    if args.fetch_toolchain:
        import fetch_toolchain
        sys.argv = ['fetch_toolchain.py'] + (['--7z', args.sevenzip] if args.sevenzip else []) + (['--keep'] if args.keep_downloads else [])
        fetch_toolchain.main()
        if not args.vc7:
            args.vc7 = str(FETCHED)
    vc7 = find_vc7(args.vc7)
    sdk = Path(args.sdk).resolve() if args.sdk else sub(vc7, 'PlatformSDK')
    if not DRY_RUN and (not sub(sdk, 'lib').is_dir() or not sub(sdk, 'include').is_dir()):
        raise SystemExit('Platform SDK include\\ and lib\\ not found under ' + str(sdk) + ' (use --sdk)')
    out = Path(args.out).resolve()
    if args.clean and out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True, exist_ok=True)
    log('VC7   :', vc7)
    log('SDK   :', sdk)
    log('output:', out)
    if not DRY_RUN:
        check_toolchain(vc7, sdk, args.strict)
    env = tool_env(vc7)

    if not args.link_only:
        failed = compile_all(man, vc7, sdk, out, env, max(1, min(args.jobs, 2)), args.keep_going)
        if failed:
            raise SystemExit('%d translation unit(s) failed: %s' % (len(failed), ', '.join(failed)))
    exe = link_all(man, vc7, sdk, out, env, r'd:\projects\mk\bin\q3map.pdb')
    if DRY_RUN:
        log('dry run finished: no tool was executed')
        return 0
    result = exe
    log('linked', exe)
    if args.identical:
        import stamp_meta
        result = out / 'q3map_identical.exe'
        result.write_bytes(stamp_meta.apply_policy(exe.read_bytes(),
                                                   json.loads((HERE / 'meta_stamp.json').read_text(encoding='utf-8'))))
        log('stamped', result)
    digest = sha256(result)
    log('')
    log('sha256 ', digest)
    log('original', ORIGINAL_SHA256)
    if digest == ORIGINAL_SHA256:
        log('RESULT: IDENTICAL to the original q3map.exe')
        return 0
    log('RESULT: differs from the original q3map.exe (without --identical the timestamp, PDB GUID and age differ by design; see build\\README.md)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
