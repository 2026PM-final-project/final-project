"""Package exactly the reviewable source, docs, specification and tested binaries."""
from pathlib import Path
import hashlib
import zipfile

root = Path(__file__).resolve().parents[1]
required = [
    "README.md", "final_project.pdf", "Run Demo.cmd", "build.ps1",
    "CMakeLists.txt", ".gitignore", "build/BombermanDemo.exe", "build/bomber_tests.exe",
]
paths = [root / name for name in required]
for folder in ["src", "tests", "scripts", "docs"]:
    paths.extend(path for path in (root / folder).rglob("*")
                 if path.is_file() and "__pycache__" not in path.parts)
missing = [str(path) for path in paths if not path.is_file()]
if missing:
    raise SystemExit("Required deliverables missing: " + ", ".join(missing))
dist = root / "dist"
dist.mkdir(exist_ok=True)
archive = dist / "BomberLab-demo.zip"
with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
    for path in sorted(set(paths)):
        bundle.write(path, "BomberLab/" + path.relative_to(root).as_posix())
with zipfile.ZipFile(archive) as bundle:
    bad = bundle.testzip()
    if bad:
        raise SystemExit("Archive CRC failed: " + bad)
    names = bundle.namelist()
    assert "BomberLab/build/BombermanDemo.exe" in names
    assert not any(name.endswith(("scores.tsv", "capture.bmp")) for name in names)
digest = hashlib.sha256(archive.read_bytes()).hexdigest()
(dist / "SHA256SUMS.txt").write_text(digest + "  " + archive.name + "\n", encoding="ascii")
print(f"Verified {len(names)} files, {archive.stat().st_size:,} bytes: {archive}")
