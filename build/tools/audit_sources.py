"""Portability audit of src (stdlib only, read-only).

Reports: absolute paths in sources, quoted #includes that do not resolve (with the build's search order:
including file's dir, TU's dir, src), include case mismatches, includes that leave src,
mixed or unusual line endings, non-ASCII bytes, and source files that no build TU reaches.

    python audit_sources.py [--root <repo root>]
"""
import argparse
import json
import os
import re
import sys

INC = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]', re.M)
ABS = re.compile(r'(?<![A-Za-z0-9])[A-Za-z]:[\\/][^\s"\']|\\\\[A-Za-z0-9_.-]+\\')


def real_case(base, rel):
    """Resolve rel under base case-sensitively component by component; return (path or None, case_ok)."""
    cur = base
    ok = True
    for part in rel.replace('\\', '/').split('/'):
        if part in ('', '.'):
            continue
        if part == '..':
            cur = os.path.dirname(cur)
            continue
        try:
            names = os.listdir(cur)
        except OSError:
            return None, ok
        if part in names:
            cur = os.path.join(cur, part)
        else:
            m = [n for n in names if n.lower() == part.lower()]
            if not m:
                return None, ok
            ok = False
            cur = os.path.join(cur, m[0])
    return (cur if os.path.exists(cur) else None), ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', default=os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..')))
    a = ap.parse_args()
    src = os.path.join(a.root, 'src')
    man = json.load(open(os.path.join(a.root, 'build', 'sources.json')))
    tu_dirs = {os.path.dirname(os.path.join(a.root, t['source'])) for t in man['translation_units']}
    files = []
    for d, _, fs in os.walk(src):
        for f in fs:
            files.append(os.path.join(d, f))
    findings = {k: [] for k in ('absolute_path', 'unresolved_include', 'case_mismatch', 'outside_src', 'line_endings',
                                'non_ascii', 'unreached_file')}
    reached = set()
    tus = [os.path.join(a.root, t['source']) for t in man['translation_units']] + \
          [os.path.join(a.root, s) for s in man.get('not_linked_sources', [])]
    stack = [(t, os.path.dirname(t), (os.path.dirname(t),)) for t in tus]
    seen = set()
    while stack:
        f, tudir, chain = stack.pop()
        if (f, tudir) in seen or not os.path.isfile(f):
            continue
        seen.add((f, tudir))
        reached.add(os.path.normcase(f))
        text = open(f, 'rb').read().decode('latin1')
        for kind, name in INC.findall(text):
            if kind == '<':
                continue
            hit = None
            for base in (os.path.dirname(f), *reversed(chain), tudir, src):  # MSVC also searches the includers' dirs
                p, ok = real_case(base, name)
                if p:
                    hit = p
                    if not ok:
                        findings['case_mismatch'].append('%s: "%s" -> %s' % (os.path.relpath(f, a.root), name, os.path.relpath(p, a.root)))
                    break
            if hit is None:
                findings['unresolved_include'].append('%s: "%s"' % (os.path.relpath(f, a.root), name))
                continue
            if not os.path.abspath(hit).startswith(os.path.abspath(src) + os.sep):
                findings['outside_src'].append('%s: "%s"' % (os.path.relpath(f, a.root), name))
            stack.append((hit, tudir, chain + (os.path.dirname(hit),)))
    for f in files:
        raw = open(f, 'rb').read()
        rel = os.path.relpath(f, a.root)
        if not f.lower().endswith(('.c', '.h', '.cpp', '.hpp', '.inl')):
            continue
        crlf = raw.count(b'\r\n')
        lf = raw.count(b'\n') - crlf
        cr = raw.count(b'\r') - crlf
        if (crlf and lf) or cr:
            findings['line_endings'].append('%s: CRLF=%d LF-only=%d lone-CR=%d' % (rel, crlf, lf, cr))
        if any(b > 127 for b in raw):
            findings['non_ascii'].append(rel)
        text = raw.decode('latin1')
        for i, line in enumerate(text.splitlines(), 1):
            m = ABS.search(line)
            if m and '://' not in line and 'a-z' not in line.lower()[:0]:
                findings['absolute_path'].append('%s:%d: %s' % (rel, i, line.strip()[:100]))
        if os.path.normcase(f) not in reached:
            findings['unreached_file'].append(rel)
    n = 0
    for k, v in findings.items():
        print('== %s (%d)' % (k, len(v)))
        for x in v[:60]:
            print('   ' + x)
        n += len(v)
    return 0


if __name__ == '__main__':
    sys.exit(main())
