"""Restore a Lab's reference code, backing up current files first (stdlib only)."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import sys

# Windows may inherit a legacy code page while Lab titles and workspace paths are Chinese.
sys.stdout.reconfigure(encoding="utf-8")
sys.stderr.reconfigure(encoding="utf-8")

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("lab", nargs="?", help="00..09, full folder name, or common")
parser.add_argument("--list", action="store_true", help="list available Labs")
parser.add_argument("--dry-run", action="store_true", help="show files without changing anything")
args = parser.parse_args()
index = json.loads((ROOT / "labs/index.json").read_text(encoding="utf-8"))
if args.list:
    for item in index:
        print(item["number"], item["folder"], item["title"])
    print("common shared MAVLink/UDP/Client helpers")
    raise SystemExit(0)
if args.lab == "common":
    directory = ROOT / "common"
else:
    matches = [item for item in index if args.lab in (item["number"], item["folder"], str(int(item["number"])))]
    if len(matches) != 1:
        parser.error("choose 00..09, a full Lab folder name, or common; use --list")
    directory = ROOT / "labs" / matches[0]["folder"]
reference = directory / "reference"
manifest = json.loads((reference / "manifest.json").read_text(encoding="utf-8"))
plan = []
for relative, digest in manifest.items():
    source = (reference / relative).resolve()
    destination = (directory / relative).resolve()
    if not source.is_relative_to(reference.resolve()) or not destination.is_relative_to(directory.resolve()):
        raise SystemExit("Refusing path outside the selected Lab")
    if not directory.resolve().is_relative_to(ROOT) or destination.is_relative_to(reference.resolve()):
        raise SystemExit("Refusing invalid restore destination")
    if hashlib.sha256(source.read_bytes()).hexdigest() != digest:
        raise SystemExit("Reference changed; refusing to restore: " + str(source))
    plan.append((source, destination, relative))
if args.dry_run:
    for _, destination, _ in plan:
        print("Would restore", destination.relative_to(ROOT))
    raise SystemExit(0)
stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
backup = ROOT / "practice_backups" / stamp / directory.name
# Finish all backups before the first overwrite. Newly created learner files remain untouched.
for _, destination, relative in plan:
    if destination.exists():
        saved = backup / relative
        saved.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(destination, saved)
for source, destination, _ in plan:
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
print("Restored", directory.relative_to(ROOT))
print("Backup:", backup)
print("Rebuild the affected target. Extra files, notes and shared code were not removed.")
