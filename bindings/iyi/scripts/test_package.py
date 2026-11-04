#!/usr/bin/env python3
"""Exercise actual iyi get against a temporary local git mirror, never GitHub.

The mirror has a synthetic v3.0.0 tag containing this working tree's package;
this does not assert that an already-published upstream tag contains iyi support.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

BINDING = Path(__file__).resolve().parents[1]
ROOT = BINDING.parents[1]


def main():
    with tempfile.TemporaryDirectory(prefix="ccharts-iyi-package-") as tmp:
        tmp = Path(tmp)
        mirror = tmp / "mirror"
        package = mirror / "github.com/dethrandir/ccharts"
        package.mkdir(parents=True)
        for name in ("iyi.mod", "ccharts.iyi"):
            shutil.copy2(ROOT / name, package / name)
        shutil.copytree(BINDING, package / "bindings/iyi",
                        ignore=shutil.ignore_patterns("build", "__pycache__", "*.pyc", "iyi.sum"))
        env = os.environ.copy()
        env["XDG_CACHE_HOME"] = str(tmp / "cache")
        env["IYI_MOD_MIRROR"] = str(mirror)
        env["IYI_LIBRARY_PATH"] = str(BINDING / "build")

        def run(args, cwd):
            return subprocess.run(args, cwd=cwd, env=env, check=True,
                                  capture_output=True, text=True, encoding="utf-8")

        run(["git", "init", "-q", "-b", "main"], package)
        run(["git", "add", "."], package)
        run(["git", "-c", "user.name=Package Test", "-c", "user.email=test@example.invalid",
             "-c", "commit.gpgsign=false", "commit", "-qm", "Package test snapshot"], package)
        run(["git", "-c", "tag.gpgsign=false", "tag", "v3.0.0"], package)
        consumer = tmp / "consumer"
        consumer.mkdir()
        (consumer / "iyi.mod").write_text("module example.test/ccharts-demo\n", encoding="utf-8")
        (consumer / "main.iyi").write_text('''module main
import github.com/dethrandir/ccharts/v3::{Chart, Settings, Slice, Charts, ChartError}
assert_eq("3.0.0", Charts.version)
chart = Chart.from_arrays([1.0, 2.0], [3.0, 4.0], [0.5, 1.0], [1.5, 3.0]).or_panic
assert_eq(2, chart.size)
assert(!chart.candle(20, 4, Settings.new(plain: true)).or_panic.empty?)
chart.close
assert_error(ChartError) { chart.line(20, 4) }
assert(!Chart.pie([Slice.new("Türkçe", 1.0)], 20, 6).or_panic.empty?)
puts "package consumer passed"
''', encoding="utf-8")
        result = run(["iyi", "get", "github.com/dethrandir/ccharts/v3@v3.0.0"], consumer)
        assert "github.com/dethrandir/ccharts/v3" in (consumer / "iyi.mod").read_text()
        result = run(["iyi", "run", "main.iyi"], consumer)
        assert "package consumer passed" in result.stdout, result.stdout + result.stderr
        assert (consumer / "iyi.sum").exists(), "resolver did not record package integrity"
        # A cached dependency must still work once its original mirror is gone.
        shutil.rmtree(mirror)
        binary = consumer / ("cached-consumer.exe" if os.name == "nt" else "cached-consumer")
        run(["iyi", "build", "--force", "main.iyi", "-o", str(binary)], consumer)
        result = run([str(binary)], consumer)
        assert "package consumer passed" in result.stdout
        print("package consumer: iyi get, integrity pin, public API and cached rebuild passed")


if __name__ == "__main__":
    main()
