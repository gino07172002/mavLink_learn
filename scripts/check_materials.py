"""Validate course file links and immutable reference hashes; learner edits are allowed."""
import hashlib
import json
from pathlib import Path
import re
import sys

sys.stdout.reconfigure(encoding="utf-8")
sys.stderr.reconfigure(encoding="utf-8")
root = Path(__file__).resolve().parents[1]
documents = [root / "README.md", root / "third_party/README.md"]
for directory in ("common", "labs", "docs"):
    documents.extend((root / directory).rglob("*.md"))
errors = []
for document in documents:
    for link in re.findall(r"\]\(([^)]+)\)", document.read_text(encoding="utf-8")):
        target = link.split("#", 1)[0]
        if not target or "://" in target or target.startswith("mailto:"):
            continue
        if not (document.parent / target).exists():
            errors.append(f"Missing link: {document.relative_to(root)} -> {target}")
index = json.loads((root / "labs/index.json").read_text(encoding="utf-8"))
manifests = [root / "labs" / item["folder"] / "reference/manifest.json" for item in index]
manifests.append(root / "common/reference/manifest.json")
count = 0
for manifest in manifests:
    for relative, digest in json.loads(manifest.read_text(encoding="utf-8")).items():
        reference = (manifest.parent / relative).resolve()
        if not reference.is_relative_to(manifest.parent.resolve()) or not reference.is_file():
            errors.append(f"Invalid reference path: {manifest.relative_to(root)} -> {relative}")
        elif hashlib.sha256(reference.read_bytes()).hexdigest() != digest:
            errors.append(f"Reference hash mismatch: {reference.relative_to(root)}")
        count += 1
if errors:
    print("\n".join(errors), file=sys.stderr)
    raise SystemExit(1)
print(f"PASS: {len(documents)} documents, {len(manifests)} manifests, {count} reference files")
