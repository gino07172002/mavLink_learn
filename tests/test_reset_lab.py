"""Exercise restore safety in a disposable project fixture, never in learner files."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[1] / "scripts/reset_lab.py"

class ResetLabTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="mavlink-reset-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        (self.root / "scripts").mkdir()
        shutil.copy2(SOURCE, self.root / "scripts/reset_lab.py")
        self.lab = self.root / "labs/lab01_telemetry"
        (self.lab / "src").mkdir(parents=True)
        (self.lab / "reference/src").mkdir(parents=True)
        (self.root / "labs/index.json").write_text(json.dumps([
            {"number": "01", "folder": "lab01_telemetry", "title": "遙測練習"}
        ]), encoding="utf-8")
        self.original = b"int main() { return 0; }\n"
        self.modified = b"// learner changes\nint main() { return 1; }\n"
        (self.lab / "reference/src/main.cpp").write_bytes(self.original)
        (self.lab / "src/main.cpp").write_bytes(self.modified)
        (self.lab / "notes.md").write_text("keep my notes", encoding="utf-8")
        self.manifest = {"src/main.cpp": hashlib.sha256(self.original).hexdigest()}
        self.write_manifest()

    def write_manifest(self):
        (self.lab / "reference/manifest.json").write_text(json.dumps(self.manifest), encoding="utf-8")

    def run_reset(self, *args):
        return subprocess.run([sys.executable, str(self.root / "scripts/reset_lab.py"), *args],
                              capture_output=True, text=True, encoding="utf-8", timeout=10)

    def test_list_handles_unicode_title(self):
        result = self.run_reset("--list")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("遙測練習", result.stdout)
        self.assertFalse((self.root / "practice_backups").exists())

    def test_restore_backs_up_changes_and_preserves_notes(self):
        result = self.run_reset("01")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.lab / "src/main.cpp").read_bytes(), self.original)
        backups = list((self.root / "practice_backups").glob("*/lab01_telemetry/src/main.cpp"))
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_bytes(), self.modified)
        self.assertEqual((self.lab / "notes.md").read_text(), "keep my notes")

    def test_dry_run_changes_nothing(self):
        result = self.run_reset("01", "--dry-run")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.lab / "src/main.cpp").read_bytes(), self.modified)
        self.assertFalse((self.root / "practice_backups").exists())

    def test_corrupt_reference_refuses_before_overwrite(self):
        (self.lab / "reference/src/main.cpp").write_bytes(b"changed answer")
        result = self.run_reset("01")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Reference changed", result.stderr)
        self.assertEqual((self.lab / "src/main.cpp").read_bytes(), self.modified)

    def test_path_escape_refuses(self):
        self.manifest = {"../../outside.cpp": "not-used"}
        self.write_manifest()
        result = self.run_reset("01")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Refusing path", result.stderr)
        self.assertEqual((self.lab / "src/main.cpp").read_bytes(), self.modified)

    def test_unknown_lab_refuses(self):
        result = self.run_reset("99")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual((self.lab / "src/main.cpp").read_bytes(), self.modified)

if __name__ == "__main__":
    unittest.main(verbosity=2)
