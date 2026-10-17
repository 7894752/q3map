"""Unit test of the Wine command construction in build.py (no Wine needed). Run: python test_wine_cmds.py"""
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import build as B

B.WINE = ['wine']
B.DRY_RUN = True
BS = chr(92)

# 1. path conversion (dry-run uses the Z: mapping; real runs prefer `winepath -w`)
assert B.wpath('/home/u/q3map/src/q3map/bsp.c') == 'Z:' + BS + BS.join(['home', 'u', 'q3map', 'src', 'q3map', 'bsp.c']), B.wpath('/home/u/q3map/src/q3map/bsp.c')
assert B.wpath('C:' + BS + 'Vc7' + BS + 'bin') == 'C:' + BS + 'Vc7' + BS + 'bin'      # already Windows
B.WINE = None
assert B.wpath('/home/u/x') == '/home/u/x'                                           # untouched on Windows
B.WINE = ['wine']

# 2. command construction: capture the lines the dry-run prints
lines = []
B.log = lambda *a: lines.append(' '.join(str(x) for x in a))
with tempfile.TemporaryDirectory() as td:
    td = Path(td)
    code, _ = B.run(['/opt/vc7/bin/cl.exe', '/c', '/I' + B.wpath('/opt/vc7/include'), B.wpath('/src/a.c')], td, {})
    assert code == 0
    assert lines[-1].endswith('wine /opt/vc7/bin/cl.exe /c /IZ:' + BS + 'opt' + BS + 'vc7' + BS + 'include Z:' + BS + 'src' + BS + 'a.c'), lines[-1]
    long_args = ['/I' + B.wpath('/very/long/path/%d' % i) for i in range(200)]
    B.run(['/opt/vc7/bin/lib.exe'] + long_args, td, {})
    assert '@' in lines[-1] and len(lines[-1]) < 400, lines[-1][:200]              # response file used
    env = B.tool_env(Path('/opt/vc7'))
    assert 'WINEPATH' in env and env['WINEPATH'].startswith('Z:' + BS + 'opt' + BS + 'vc7' + BS + 'bin'), env.get('WINEPATH')
print('test_wine_cmds: ok')
