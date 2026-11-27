"""Build or slim a ROM-free Windows setup ZIP for RecompLauncher."""

import argparse
from contextlib import contextmanager
from copy import copy
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import struct
import subprocess
import sys
import tempfile
from zipfile import ZIP_DEFLATED, BadZipFile, ZipFile


ROOT = Path(__file__).resolve().parents[1]
PROFILE = ROOT / "recomplauncher.json"
TARGET = "SuperMarioWorld2YoshiSIslandSNESRecomp"
PROJECT_UPDATES = ("CMakeLists.txt", "src/main.c", "VERSION", "recomplauncher.json")
RELEASE_VERSION = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
RELEASE_NAME = f"islandSnesRecomp-{RELEASE_VERSION}-windows-x64.zip"
LEGACY_NAME = f"supermarioworld2yoshisisland-{RELEASE_VERSION}-windows-x64.zip"
PREVIOUS_SETUP = "supermarioworld2yoshisisland-0.1.0-windows-x64.zip"
FRAMEWORK_PATHS = (PurePosixPath("modules/snesrecomp"), PurePosixPath("snesrecomp"))
ANALYZER_TARGET = PurePosixPath("recompiler-rs/target")
ANALYZER = ANALYZER_TARGET / "release/snesrecomp-analyze.exe"
RELEASE_FILES = {
    "CMakeLists.txt", "LICENSE", "README.txt", "SOURCE_REVISIONS",
    "VERSION", "recomplauncher.json", "rom_identity.txt",
}
RELEASE_DIRECTORIES = {
    "assets", "dependences", "launcher_assets", "mods", "modules",
    "recomp", "recomp-ui", "snesrecomp", "src", "toolchain",
}
DEVELOPMENT_DIRECTORIES = {
    ("dependences", "SDL3", "test"),
    ("dependences", "libjuice-1.7.2", "test"),
    ("dependences", "libjuice-1.7.2", "fuzzer"),
    ("modules", "snesrecomp", "docs"),
    ("modules", "recomp-ui", "docs"),
    ("modules", "recomp-ui", "test_data"),
}
DEPENDENCIES = {
    "SDL3": ("CMakeLists.txt", "LICENSE.txt", "cmake", "include", "src", "wayland-protocols", "build-scripts"),
    "lua-5.4.9": ("src/lua.h",),
    "libjuice-1.7.2": ("CMakeLists.txt", "LICENSE", "cmake", "include", "src"),
}


def dependency_files(base):
    for name, required in DEPENDENCIES.items():
        tree = base / name
        for entry in required:
            if not (tree / entry).exists():
                raise ValueError(f"Missing rebuild dependency: {tree / entry}")
        if name == "lua-5.4.9":
            files = [*tree.glob("src/*.c"), *tree.glob("src/*.h")]
        else:
            files = []
            for entry in required:
                path = tree / entry
                files.extend(path.rglob("*") if path.is_dir() else [path])
        for path in sorted(files):
            if path.is_symlink() or not path.resolve().is_relative_to(tree.resolve()):
                raise ValueError(f"Unexpected dependency path: {path}")
            if path.is_file():
                yield path, path.relative_to(base).as_posix()


def inspect_base(archive, executable, *, repack=False):
    entries = [PurePosixPath(item.filename) for item in archive.infolist()]
    roots = {path.parts[0] for path in entries if path.parts}
    if len(roots) != 1:
        raise ValueError("Expected a Windows setup ZIP with a single top-level folder.")
    prefix = roots.pop()
    if prefix in (".", "..", "/"):
        raise ValueError("Invalid setup ZIP folder.")
    for path in entries:
        if (
            not path.parts
            or ".." in path.parts
            or any("\\" in part or ":" in part for part in path.parts)
            or path.suffix.lower() in {".sfc", ".smc", ".fig", ".srm", ".sav"}
            or (len(path.parts) == 2 and path.name.lower() in {
                "rom.cfg", "disc.cfg", "config.ini", "config.local.ini", "keybinds.ini", "input.ini"
            })
            or path.parts[1:3] == ("src", "gen")
            or (len(path.parts) > 1 and path.parts[1] in {"build", "saves"})
            or path.parts[1:] == ("recomp", "funcs.h")
            or (path.name.casefold() == PROFILE.name and path.parts[1:] != (PROFILE.name,))
        ):
            raise ValueError(f"Unexpected entry in the setup ZIP: {path}")
    names = set(entries)
    for filename in (executable, "VERSION", "CMakeLists.txt", "dependences/CMakeLists.txt", "recomp/bank00.cfg"):
        if PurePosixPath(prefix, filename) not in names:
            raise ValueError(f"Missing setup ZIP entry: {filename}")
    framework = next((folder for folder in FRAMEWORK_PATHS
                      if PurePosixPath(prefix) / folder / "snesrecomp_cli.py" in names), None)
    if framework is None:
        raise ValueError("Missing SNESRecomp CLI in the setup ZIP.")
    if len(entries) != len(names):
        raise ValueError("Duplicate entries in the setup ZIP.")
    archive_version = archive.read(f"{prefix}/VERSION").decode().strip()
    if repack and archive_version != RELEASE_VERSION:
        raise ValueError(f"Setup ZIP is {archive_version}. Build {RELEASE_VERSION} before repacking.")
    if repack and archive.read(f"{prefix}/{PROFILE.name}") != PROFILE.read_bytes():
        raise ValueError("The setup ZIP must already contain the current RecompLauncher profile.")
    return prefix, archive_version, framework


def release_file(path, executable):
    path = PurePosixPath(path.as_posix())
    if len(path.parts) == 1:
        return path.name in RELEASE_FILES or path.name == executable or path.suffix.lower() == ".dll"
    for framework in FRAMEWORK_PATHS:
        if path.is_relative_to(framework / ANALYZER_TARGET):
            return path == framework / ANALYZER
    return (
        path.parts[0] in RELEASE_DIRECTORIES
        and path.parts[:3] not in DEVELOPMENT_DIRECTORIES
        and ".git" not in path.parts
        and "__pycache__" not in path.parts
        and path.suffix.lower() not in {".log", ".pdb", ".pyc", ".dmp"}
    )


def repack_setup(source, output_dir):
    output = release_output(source, output_dir)
    executable, = json.loads(PROFILE.read_text(encoding="utf-8"))["start"]["program"]
    with ZipFile(source) as original:
        prefix, _, framework = inspect_base(original, executable, repack=True)
        with release_zip(output, framework) as archive:
            for entry in original.infolist():
                relative = PurePosixPath(entry.filename).relative_to(prefix)
                if entry.is_dir() or not release_file(relative, executable):
                    continue
                member = copy(entry)
                member.filename = f"{output.stem}/{relative.as_posix()}"
                with original.open(entry) as reader, archive.open(member, "w") as writer:
                    shutil.copyfileobj(reader, writer)
    return output


def build_host(project, build_dir, toolchain):
    binaries = toolchain / "bin"
    cmake = binaries / "cmake.exe"
    ninja = binaries / "ninja.exe"
    clang = binaries / "clang.exe"
    clangxx = binaries / "clang++.exe"
    for program in (cmake, ninja, clang, clangxx):
        if not program.is_file():
            raise ValueError(f"Missing Windows build tool: {program}")
    env = dict(os.environ, PATH=f"{binaries}{os.pathsep}{os.environ['PATH']}")
    subprocess.run([
        str(cmake), "-S", str(project), "-B", str(build_dir), "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release", "-DSNESRECOMP_SETUP_HOST=ON",
        "-DCMAKE_DISABLE_FIND_PACKAGE_SDL3=ON",
        f"-DCMAKE_MAKE_PROGRAM={ninja}", f"-DCMAKE_C_COMPILER={clang}",
        f"-DCMAKE_CXX_COMPILER={clangxx}", "-DCMAKE_EXE_LINKER_FLAGS=-static",
    ], env=env, check=True)
    subprocess.run([str(cmake), "--build", str(build_dir), "--parallel", "--target", TARGET],
                   env=env, check=True)


def release_output(source, output_dir):
    output = output_dir / RELEASE_NAME
    if source.resolve() == output.resolve():
        raise ValueError("Choose a different output path to preserve the base ZIP.")
    if not source.name.lower().endswith("-windows-x64.zip"):
        raise ValueError("Expected a Windows x64 setup ZIP.")
    output_dir.mkdir(parents=True, exist_ok=True)
    return output


def package(source, output_dir, dependency_root, toolchain):
    output = release_output(source, output_dir)
    executable, = json.loads(PROFILE.read_text(encoding="utf-8"))["start"]["program"]
    files = list(dependency_files(dependency_root))

    # Keep the build paths short, including on Windows without long paths enabled.
    with tempfile.TemporaryDirectory(prefix="yi-", dir=ROOT / "dist") as temporary:
        workspace = Path(temporary)
        with ZipFile(source) as archive:
            prefix, base_version, framework = inspect_base(archive, executable)
            project = workspace / "source"
            for entry in archive.infolist():
                relative = PurePosixPath(entry.filename).relative_to(prefix)
                if entry.is_dir() or relative == framework / ANALYZER or not release_file(relative, executable):
                    continue
                destination = project.joinpath(*relative.parts)
                destination.parent.mkdir(parents=True, exist_ok=True)
                with archive.open(entry) as reader, destination.open("xb") as writer:
                    shutil.copyfileobj(reader, writer)
        for relative in PROJECT_UPDATES:
            shutil.copyfile(ROOT / relative, project / relative)
        # Build against the same framework sources shipped in the setup ZIP.
        analyzer_env = dict(os.environ, CARGO_TARGET_DIR=str(project / framework / ANALYZER_TARGET))
        analyzer_env.pop("CARGO_BUILD_TARGET", None)
        subprocess.run([sys.executable, str(project / framework / "tools/build_native_analyzer.py")],
                       env=analyzer_env, check=True)
        analyzer_path = project / framework / ANALYZER
        readme = project / "README.txt"
        title, _, instructions = readme.read_text(encoding="utf-8").partition("\n")
        if f" {base_version} " not in title:
            raise ValueError(f"Setup README title does not identify version {base_version}.")
        if "prepare" in json.loads(PROFILE.read_text(encoding="utf-8")) and "First run:" in instructions:
            instructions = instructions.replace(
                "First run:",
                "RecompLauncher Easy Setup: select your USA .sfc file and wait for the\n"
                "local build. Keep the selected ROM available for Play. Build diagnostics\n"
                "are written to build/recomplauncher-build.log.\n\nManual setup:",
                1,
            )
        readme.write_text(
            title.replace(f" {base_version} ", f" {RELEASE_VERSION} ", 1) + "\n"
            + instructions.replace(
                "SOURCE_REVISIONS lists the exact project and framework commits this pack was cut from.",
                "SOURCE_REVISIONS records the base setup pack and the local source overrides.",
            ),
            encoding="utf-8",
        )
        with source.open("rb") as stream:
            base_sha256 = hashlib.file_digest(stream, "sha256").hexdigest()
        with (project / "SOURCE_REVISIONS").open("a", encoding="utf-8") as revisions:
            revisions.write(f"\nbase_setup={source.name}\nbase_setup_sha256={base_sha256}\n")
            for relative in PROJECT_UPDATES:
                digest = hashlib.sha256((project / relative).read_bytes()).hexdigest()
                revisions.write(f"local_sha256 {relative}={digest}\n")
            digest = hashlib.sha256(analyzer_path.read_bytes()).hexdigest()
            revisions.write(f"local_sha256 {framework / ANALYZER}={digest}\n")
        for path, relative in files:
            destination = project / "dependences" / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, destination)
        build_dir = workspace / "build"
        build_host(project, build_dir, toolchain)
        shutil.copyfile(build_dir / executable, project / executable)
        with release_zip(output, framework) as archive:
            for path in sorted(project.rglob("*")):
                relative = path.relative_to(project)
                if path.is_file() and release_file(relative, executable):
                    archive.write(path, f"{output.stem}/{relative.as_posix()}")
    return output


def verify_analyzer(archive, member):
    if member not in archive.namelist():
        raise ValueError(f"Missing native analyzer in the setup ZIP: {member}")
    with archive.open(member) as binary:
        header = binary.read(64)
        if len(header) == 64 and header.startswith(b"MZ"):
            pe_offset, = struct.unpack_from("<I", header, 60)
            if 64 <= pe_offset <= archive.getinfo(member).file_size - 6:
                binary.seek(pe_offset)
                if binary.read(6) == b"PE\0\0\x64\x86":
                    return
    raise ValueError(f"Expected a Windows x64 native analyzer: {member}")


@contextmanager
def release_zip(output, framework):
    with tempfile.TemporaryDirectory(dir=output.parent) as temporary:
        staged = Path(temporary) / output.name
        with ZipFile(staged, "w", compression=ZIP_DEFLATED) as archive:
            yield archive
        with ZipFile(staged) as archive:
            verify_analyzer(archive, f"{output.stem}/{framework / ANALYZER}")
            broken = archive.testzip()
            if broken is not None:
                raise ValueError(f"Corrupt ZIP entry: {broken}")
        with staged.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        checksum = staged.with_suffix(".zip.sha256")
        checksum.write_text(f"{digest}  {output.name}\n", encoding="ascii")
        os.replace(staged, output)
        os.replace(checksum, output.with_suffix(".zip.sha256"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("zip", type=Path, nargs="?")
    parser.add_argument("--repack", action="store_true", help="Slim an existing setup ZIP without compiling.")
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--dependencies", type=Path, default=ROOT / "dependences")
    parser.add_argument("--toolchain", type=Path, default=os.environ.get("RETCOMM_TOOLCHAIN_DIR"))
    args = parser.parse_args()
    try:
        dist = ROOT / "dist"
        if args.repack:
            built = dist / "recomplauncher"
            current_zip = built / RELEASE_NAME
            print(repack_setup(
                args.zip or (current_zip if current_zip.is_file() else built / LEGACY_NAME),
                args.output_dir or dist / "recomplauncher-clean",
            ))
        else:
            if args.toolchain is None:
                raise ValueError("Set RETCOMM_TOOLCHAIN_DIR or pass --toolchain with the Windows x64 toolchain.")
            candidates = (dist / LEGACY_NAME, dist / PREVIOUS_SETUP,
                          dist / "recomplauncher" / PREVIOUS_SETUP)
            base_zip = args.zip or next((path for path in candidates if path.is_file()), candidates[0])
            print(package(
                base_zip, args.output_dir or dist / "recomplauncher",
                args.dependencies, args.toolchain.resolve(),
            ))
    except (OSError, ValueError, KeyError, BadZipFile, subprocess.CalledProcessError) as error:
        parser.exit(1, f"{parser.prog}: {error}\n")


if __name__ == "__main__":
    main()
