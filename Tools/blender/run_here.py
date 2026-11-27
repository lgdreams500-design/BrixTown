"""Run a Tools/blender script against the checkout this file is in (the main one or a worktree): every
C:/Projects/Wasted/ path, in the scripts' text and in files they open, points at this checkout instead.

    blender-launcher.exe -b --factory-startup --python <checkout>/Tools/blender/run_here.py
    with WASTED_SCRIPT=<script in Tools/blender, e.g. hours_50.py> and that script's own WASTED_* settings.
Used by Tools/blender/reference_render.ps1."""
import builtins, io, os
MAIN = "C:/Projects/Wasted/"
HERE = os.path.dirname(os.path.abspath(__file__)).replace("\\", "/")
WT = HERE[:-len("Tools/blender")]                      # this checkout's root, with its trailing /
_open = builtins.open


def fix(p):
    if isinstance(p, str):
        q = p.replace("\\", "/")
        if q.startswith(MAIN) and not q.startswith(WT):
            return WT + q[len(MAIN):]
    return p


def wopen(file, mode="r", *a, **k):
    file = fix(file)
    if isinstance(file, str) and file.endswith(".py") and "r" in mode and "b" not in mode and file.startswith(WT):
        with _open(file, mode, *a, **k) as f:
            src = f.read()
        return io.StringIO(src if WT == MAIN else src.replace(MAIN, WT).replace(WT + WT[len(MAIN):], WT))
    return _open(file, mode, *a, **k)


if WT != MAIN:
    builtins.open = wopen
script = WT + "Tools/blender/" + os.environ["WASTED_SCRIPT"]
src = wopen(script).read()
exec(compile(src, script, "exec"), {"__name__": "__main__", "__file__": script})
