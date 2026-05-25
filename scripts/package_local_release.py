#!/usr/bin/env python3
"""Create a local bbsolver release archive and SHA-256 checksum."""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import tarfile
import tempfile
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Package an already-built bbsolver binary into a release archive. "
            "Run once per target platform."
        )
    )
    parser.add_argument("--binary", type=Path, required=True, help="Built bbsolver binary")
    parser.add_argument("--version", required=True, help="Release version, e.g. 1.0.0")
    parser.add_argument(
        "--platform",
        required=True,
        choices=("macos-arm64", "macos-x64", "linux-x64", "windows-x64"),
        help="Platform label used in the archive filename.",
    )
    parser.add_argument("--out-dir", type=Path, default=ROOT / "dist")
    return parser.parse_args()


def copy_if_exists(src: Path, dst: Path) -> None:
    if src.exists():
        if src.is_dir():
            shutil.copytree(src, dst, ignore=shutil.ignore_patterns(".DS_Store", "__pycache__"))
        else:
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)


def write_checksum(path: Path) -> Path:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    checksum_path = path.with_suffix(path.suffix + ".sha256")
    checksum_path.write_text(f"{digest.hexdigest()}  {path.name}\n", encoding="utf-8")
    return checksum_path


def make_tar_gz(staging: Path, out_path: Path) -> None:
    with tarfile.open(out_path, "w:gz") as archive:
        archive.add(staging, arcname=staging.name)


def make_zip(staging: Path, out_path: Path) -> None:
    with zipfile.ZipFile(out_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in staging.rglob("*"):
            if path.is_file():
                archive.write(path, path.relative_to(staging.parent))


def main() -> int:
    args = parse_args()
    binary = args.binary.resolve()
    if not binary.is_file():
        raise SystemExit(f"bbsolver-release: binary not found: {binary}")

    args.out_dir.mkdir(parents=True, exist_ok=True)
    package_name = f"bbsolver-v{args.version}-{args.platform}"
    archive_ext = ".zip" if args.platform == "windows-x64" else ".tar.gz"
    archive_path = args.out_dir / f"{package_name}{archive_ext}"

    with tempfile.TemporaryDirectory(prefix="bbsolver-release-") as tmp:
        staging = Path(tmp) / package_name
        bin_dir = staging / "bin"
        bin_dir.mkdir(parents=True)
        binary_name = "bbsolver.exe" if args.platform == "windows-x64" else "bbsolver"
        shutil.copy2(binary, bin_dir / binary_name)
        if args.platform != "windows-x64":
            mode = os.stat(bin_dir / binary_name).st_mode
            os.chmod(bin_dir / binary_name, mode | 0o111)

        for filename in ("README.md", "LICENSE", "CHANGELOG.md", "CONTRIBUTING.md", "SECURITY.md"):
            copy_if_exists(ROOT / filename, staging / filename)
        for dirname in ("docs", "examples", "protocol", "schemas", "scripts"):
            copy_if_exists(ROOT / dirname, staging / dirname)

        manifest = staging / "RELEASE_MANIFEST.txt"
        manifest.write_text(
            "\n".join(
                [
                    f"bbsolver {args.version}",
                    f"platform: {args.platform}",
                    f"binary: bin/{binary_name}",
                    "contents: CLI, docs, examples, protocol, JSON schemas, scripts",
                    "",
                ]
            ),
            encoding="utf-8",
        )

        if archive_ext == ".zip":
            make_zip(staging, archive_path)
        else:
            make_tar_gz(staging, archive_path)

    checksum_path = write_checksum(archive_path)
    print(archive_path)
    print(checksum_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
