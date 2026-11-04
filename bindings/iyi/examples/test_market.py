"""Black-box CLI acceptance checks; build market first, then pass its path.

Usage: python3 bindings/iyi/examples/test_market.py /path/to/market
No network, Python packages, or fl installation required.
"""
import json
from pathlib import Path
import subprocess
import sys
import unittest

BINARY = str(Path(sys.argv.pop(1)).resolve())
FIXTURES = Path(__file__).parent / "fixtures"


class MarketCLI(unittest.TestCase):
    def run_cli(self, *args, text=None):
        return subprocess.run([BINARY, *args], input=text, text=True, encoding="utf-8",
                              capture_output=True, timeout=15)

    def assert_error(self, text, expected, *args):
        result = self.run_cli(*args, text=text)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertEqual(result.stdout, "")
        self.assertIn(expected, result.stderr)

    def test_help(self):
        result = self.run_cli("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("fl market history", result.stdout)

    def test_history_stdin_equals_file_for_each_renderer(self):
        path = FIXTURES / "history.json"
        for kind in ("line", "candle", "spark"):
            with self.subTest(kind=kind):
                args = ("--type", kind, "--width", "24", "--height", "6")
                piped = self.run_cli(*args, text=path.read_text(encoding="utf-8"))
                file = self.run_cli(str(path), *args)
                self.assertEqual(piped.returncode, 0, piped.stderr)
                self.assertEqual(file.returncode, 0, file.stderr)
                self.assertEqual(piped.stdout, file.stdout)
                self.assertTrue(any(ord(c) > 127 for c in file.stdout))
                self.assertEqual(file.stderr, "")

    def test_plain_disables_ansi(self):
        result = self.run_cli(str(FIXTURES / "history.json"), "--plain")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("\x1b", result.stdout)

    def test_quote(self):
        result = self.run_cli(str(FIXTURES / "quote.json"), "--type", "line")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("108.50", result.stdout)
        self.assertIn("103.00", result.stdout)

    def test_invalid_json_and_shapes(self):
        for text, message in (("{", "invalid JSON"), ("[]", "must not be empty"),
                              ("42", "expected history array"), ("[1]", "expected object"),
                              ("[{}]", "missing open")):
            with self.subTest(text=text):
                self.assert_error(text, message)

    def test_bad_numeric_value(self):
        rows = json.loads((FIXTURES / "history.json").read_text(encoding="utf-8"))
        rows[1]["close"] = "108"
        self.assert_error(json.dumps(rows), "row 2: close must be a number")

    def test_bad_timestamp(self):
        rows = json.loads((FIXTURES / "history.json").read_text(encoding="utf-8"))
        rows[0]["ts"] = "tomorrow"
        self.assert_error(json.dumps(rows), "invalid ts timestamp")

    def test_epoch_timestamp(self):
        rows = json.loads((FIXTURES / "history.json").read_text(encoding="utf-8"))
        for i, row in enumerate(rows):
            row["ts"] = 1791234000 + i * 86400
        result = self.run_cli(text=json.dumps(rows))
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_argument_errors(self):
        for args, message in ((("--width",), "needs a value"),
                              (("--width", "zero"), "must be an integer"),
                              (("--height", "0"), "between 1 and 4096"),
                              (("--type", "pie"), "must be candle"),
                              (("--whoops",), "unknown option"),
                              (("no-such-market-data.json",), "does not exist"),
                              (("a", "b"), "only one input file")):
            with self.subTest(args=args):
                self.assert_error("", message, *args)


if __name__ == "__main__":
    unittest.main()
