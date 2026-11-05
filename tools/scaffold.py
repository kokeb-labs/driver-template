#!/usr/bin/env python3
"""Turn driver-template into a new chip driver or library (Python 3.9+, standard library only).

    python3 tools/scaffold.py --kind driver --name tmp117 --bus i2c --description "C++20 driver for ..."
    python3 tools/scaffold.py --kind driver --name tmp117 --bus i2c --platforms host   # no ESP32 example
    python3 tools/scaffold.py --kind driver --name hcsr501 --bus none   # GPIO only, no data bus
    python3 tools/scaffold.py --kind library --name ring_buffer --include-root ring --description "..."
    python3 tools/scaffold.py --selftest          # scaffold every kind into temp dirs and check each

Most people run tools/setup.py instead, which infers the kind and name from the repository name, asks for the
rest, runs this script and then builds and tests the result.

By default the repository is rewritten in place; --out DIR writes a scaffolded copy instead. What it does:

1. Copies the overlay for the kind and bus from tools/scaffold/ (files ending in .tmpl) over the tree and
   deletes what the kind does not use (a library has no port, chip ID, register mock or ESP32 port), and what
   the platforms do not use (without esp32: examples/esp32/, its CI job, the ESP-IDF branch of CMakeLists.txt).
2. Replaces the marked blocks (<!-- scaffold:NAME --> ... <!-- /scaffold:NAME -->, or the same with // or #)
   with the overlay's block of that name, and drops every marker.
3. Removes the template-only parts (docs/template.md, tools/setup.py, this script, tools/scaffold/, the
   template-selftest and template-init workflows).
4. Renames every file and symbol: XMPL100 -> NAME, Xmpl100 -> Name, xmpl100 -> name, the C++ namespace, the
   CMake package and target (<name>, <name>::<name>), the bus (SpiInterface -> I2cInterface,
   ..._spi_interface -> ..._i2c_interface), the repository links (--org, --repo), the copyright holder
   (--author), the licence (--license) and the description; the README and docs titles follow.
5. Regenerates the docs figures and generated regions when kokeb-docs is installed, and prints a checklist.
"""

from __future__ import annotations

import argparse
import datetime
import os
import re
import shutil
import subprocess
import sys
import tempfile
import textwrap
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OVERLAYS = ROOT / "tools" / "scaffold"

# ---------------------------------------------------------------------------------------------------------------
# Organisation settings. --org, --repo, --author and --license override them; --org defaults to the owner in
# `git remote get-url origin`, then to DEFAULT_ORG. An organisation that is not listed gets "The <Org> Authors"
# with its name title-cased.
# ---------------------------------------------------------------------------------------------------------------
DEFAULT_ORG = "kokeb-labs"
ORGS = {
    "kokeb-labs": {"display": "Kokeb Labs"},
}
DEFAULT_LICENSE = "MIT"
LICENSES = {
    # id: (licence text in tools/scaffold/license/, README badge, README sentence)
    "MIT": ("MIT.txt", "[![License: MIT](https://img.shields.io/badge/license-MIT-blue)](LICENSE)",
            "MIT. See [LICENSE](LICENSE)."),
    "Apache-2.0": ("Apache-2.0.txt",
                   "[![License: Apache-2.0](https://img.shields.io/badge/license-Apache--2.0-blue)](LICENSE)",
                   "Apache-2.0. See [LICENSE](LICENSE)."),
}
# What the template itself says; the scaffold replaces each of these.
TEMPLATE_ORG = "kokeb-labs"
TEMPLATE_REPO = "driver-template"
TEMPLATE_AUTHOR = "The Kokeb Labs Authors"
TEMPLATE_LICENSE = "MIT"
TEMPLATE_DESCRIPTION = (
    "C++20 driver for the XMPL100, a fictional example SPI temperature sensor: chip ID, one-shot and continuous"
    " reads, CRC."
)

BUSES = ("spi", "i2c", "uart", "adc", "none")
# The port of each bus: inc/<name>_<file>_interface.hpp with <Title>Interface and the <Title>Port concept. A driver
# without a data bus (`none`) talks to the part through GPIO only.
BUS_FILE = {"spi": "spi", "i2c": "i2c", "uart": "uart", "adc": "adc", "none": "gpio"}
BUS_TITLE = {"spi": "Spi", "i2c": "I2c", "uart": "Uart", "adc": "Adc", "none": "Gpio"}
# Example platforms: one self-contained examples/<platform>/ folder each, with its own port. host is always there.
PLATFORMS = ("host", "esp32")
DEFAULT_PLATFORMS = {"driver": "host,esp32", "library": "host"}
# What only a platform uses; removed when it is not selected, with every `scaffold:platform-<name>...` block.
PLATFORM_ONLY = {
    "esp32": ["examples/esp32", ".gitmodules", ".github/workflows/esp32-examples-build-ci.yml"],
}

# The repository root holds only these (kokeb-labs/.github standards/adr/0011-repository-root-layout.md): what
# GitHub or a tool needs at the root, and what a reader looks for first. Tool configuration lives in _config/.
ROOT_FILES = {"README.md", "LICENSE", "CHANGELOG.md", "CMakeLists.txt", "AGENTS.md", "CLAUDE.md", ".gitignore",
              ".gitmodules"}
ROOT_DIRS = {".github", ".claude", "_config", "cmake", "docs", "examples", "inc", "src", "tests", "tools"}
# Local editor copies of the _config/ files (.gitignore lists them): never copied into a scaffold.
LOCAL_ONLY = {".clang-format", ".clang-tidy"}
# Code: never carries a link to the organisation's standard or CI, or the organisation's name.
CODE_DIRS = ("inc", "src", "tests", "cmake", "examples")

# Never copied, never rewritten.
SKIP_DIRS = {".git", ".venv", "venv", "site", "_site", "__pycache__", "node_modules"}
SKIP_PREFIXES = ("build",)
# Copied but never rewritten: the ESP-IDF scripts submodule.
FROZEN = ("examples/esp32/scripts",)
# Template-only: removed from every scaffolded repository.
TEMPLATE_ONLY = [
    "docs/template.md",
    "tools/setup.py",
    "tools/scaffold.py",
    "tools/scaffold",
    ".github/workflows/template-selftest.yml",
    ".github/workflows/template-init.yml",
]
# Kept verbatim apart from the licence and author: they describe the template itself.
NO_RENAME = {"docs/repo-setup.md", ".claude/skills/new-driver-from-template/SKILL.md"}
# A library keeps the shell and drops the chip; its host tests live in tests/host/.
LIBRARY_MOVE = [("tests/test_harness.hpp", "tests/host/test_harness.hpp"),
                ("tests/test_main.cpp", "tests/host/test_main.cpp")]
LIBRARY_REMOVE = [
    "inc",
    "src",
    "tests/compile_fail",
    "tests/docs",
    "tests/mock_xmpl100.hpp",
    "tests/mock_xmpl100_bus.hpp",
    "tests/test_errors.cpp",
    "tests/test_xmpl100.cpp",
    "tests/test_kconfig.cpp",
    "tests/test_version.cpp",
    "docs/architecture.md",
    "docs/configuration.md",
    "docs/diagrams",
    "docs/hardware.md",
    "docs/installation.md",
    "docs/porting.md",
    "docs/troubleshooting.md",
    "examples/esp32/main/esp32_xmpl100_port.hpp",
    "examples/esp32/main/xmpl100_basic_example.cpp",
    "examples/host/host_xmpl100_port.hpp",
    ".claude/skills/add-driver-feature",
    ".claude/skills/add-port",
    ".claude/skills/datasheet-conformance",
]
# Blocks every scaffolded repository gets, whatever the kind (an overlay's block of the same name wins).
COMMON_BLOCKS = {
    # The template has no Pages site; a repository made from it publishes.
    "docs-deploy": "      deploy: true\n",
}

BLOCK_RE = re.compile(
    r"^[ \t]*(?P<open><!--|//|#) scaffold:(?P<name>[\w-]+)(?: -->)?[ \t]*\n"
    r"(?P<body>.*?)"
    r"^[ \t]*(?:<!--|//|#) /scaffold:(?P=name)(?: -->)?[ \t]*\n",
    re.S | re.M,
)
TEMPLATE_ONLY_BLOCK_RE = re.compile(r"^[ \t]*# >>> template-only.*?^[ \t]*# <<< template-only[^\n]*\n", re.S | re.M)
TEMPLATE_ONLY_LINE_RE = re.compile(r"^[^\n]*# template-only[^\n]*\n", re.M)
# Names that must not survive in a scaffolded repository.
LEFTOVER_RE = re.compile(r"xmpl100|XMPL100|Xmpl100|scaffold:|@[A-Z]+@")
# Names of other organisations, spelled in pieces so that this file does not match itself.
FOREIGN_RE = re.compile("|".join(("hard" + "foc", r"\bh" + r"f[_:-]", "con" + "med")), re.I)


def fail(message: str) -> None:
    sys.exit(f"scaffold: {message}")


def pascal(name: str) -> str:
    return "".join(part[:1].upper() + part[1:] for part in name.split("_") if part)


def skipped(rel: Path) -> bool:
    return any(p in SKIP_DIRS or p.startswith(SKIP_PREFIXES) for p in rel.parts)


def frozen(rel: Path) -> bool:
    s = rel.as_posix()
    return any(s == f or s.startswith(f + "/") for f in FROZEN)


def remove(path: Path) -> None:
    if path.is_dir() and not path.is_symlink():
        shutil.rmtree(path)
    elif path.exists() or path.is_symlink():
        path.unlink()


def copy_tree(src: Path, dst: Path) -> None:
    for item in sorted(src.rglob("*")):
        rel = item.relative_to(src)
        if skipped(rel) or (len(rel.parts) == 1 and rel.name in LOCAL_ONLY):
            continue
        target = dst / rel
        if frozen(rel) and rel.as_posix() not in FROZEN:
            continue  # submodule contents: CI checks out without submodules, and so does the copy
        if item.is_symlink():
            target.parent.mkdir(parents=True, exist_ok=True)
            os.symlink(os.readlink(item), target)
        elif item.is_dir():
            target.mkdir(parents=True, exist_ok=True)
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(item, target)


def text_files(root: Path):
    for path in sorted(root.rglob("*")):
        rel = path.relative_to(root)
        if path.is_symlink() or not path.is_file() or skipped(rel) or frozen(rel):
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        yield path, rel, text


def apply_overlay(overlay: Path, dest: Path) -> dict[str, str]:
    """Copy overlay/files/**.tmpl over dest; return overlay/blocks/* as {name: text}."""
    files = overlay / "files"
    if files.is_dir():
        for item in sorted(files.rglob("*.tmpl")):
            rel = item.relative_to(files).with_suffix("")  # drop .tmpl
            target = dest / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            remove(target)
            shutil.copy2(item, target)
    blocks = {}
    bdir = overlay / "blocks"
    if bdir.is_dir():
        for item in sorted(bdir.iterdir()):
            if item.is_file():
                blocks[item.stem] = item.read_text(encoding="utf-8")
    return blocks


def replace_blocks(text: str, blocks: dict[str, str], platforms: set[str] | None = None) -> str:
    def sub(m: re.Match) -> str:
        name = m.group("name")
        if name == "remove":
            return ""
        pm = re.match(r"platform-([a-z0-9]+)", name)
        if pm and platforms is not None and pm.group(1) not in platforms:
            return ""  # a block for a platform the repository does not have
        body = blocks.get(name, m.group("body"))
        return body if body.endswith("\n") or not body else body + "\n"

    previous = None
    while previous != text:  # blocks never nest, but a replacement may not leave a marker behind
        previous = text
        text = BLOCK_RE.sub(sub, text)
    return text


# ---------------------------------------------------------------------------------------------------------------
# Organisation, repository and author defaults
# ---------------------------------------------------------------------------------------------------------------

REMOTE_RE = re.compile(r"(?:github\.com[:/]|^[\w.-]+@[\w.-]+:)(?P<org>[\w.-]+)/(?P<repo>[\w.-]+?)(?:\.git)?/?$")


def parse_remote(url: str) -> tuple[str, str] | None:
    """(org, repo) of a GitHub remote URL (https, ssh or scp form), or None."""
    m = REMOTE_RE.search(url.strip())
    return (m.group("org"), m.group("repo")) if m else None


def git_remote(cwd: Path) -> tuple[str, str] | None:
    """(org, repo) of `git remote get-url origin` in cwd, or None without git, a remote or a GitHub URL."""
    if not (cwd / ".git").exists():
        return None
    try:
        out = subprocess.run(["git", "remote", "get-url", "origin"], cwd=cwd, capture_output=True, text=True,
                             check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return None
    return parse_remote(out)


def default_author(org: str) -> str:
    display = ORGS.get(org, {}).get("display") or " ".join(w[:1].upper() + w[1:] for w in re.split(r"[-_.]+", org) if w)
    return f"The {display} Authors"


def default_repo(kind: str, name: str) -> str:
    slug = name.replace("_", "-")
    return f"driver-{slug}" if kind == "driver" else slug


# ---------------------------------------------------------------------------------------------------------------
# Renaming
# ---------------------------------------------------------------------------------------------------------------


class Names:
    def __init__(self, a: argparse.Namespace):
        self.kind = a.kind
        self.name = a.name
        self.upper = a.name.upper()
        self.pascal = pascal(a.name)
        self.bus = a.bus
        self.ns = a.namespace
        self.root = a.include_root or a.name
        self.org = a.org
        self.repo = a.repo
        self.author = a.author
        self.license = a.license
        self.description = a.description
        self.title = f"{self.upper} Driver" if a.kind == "driver" else f"{self.pascal} Library"
        self.platforms = set(a.platforms)

    def rename_text(self, text: str, rel: str) -> str:
        text = text.replace(TEMPLATE_DESCRIPTION, self.description)
        text = text.replace(TEMPLATE_AUTHOR, self.author)
        if self.license != TEMPLATE_LICENSE:
            _, badge, sentence = LICENSES[TEMPLATE_LICENSE]
            _, new_badge, new_sentence = LICENSES[self.license]
            text = text.replace(badge, new_badge).replace(sentence, new_sentence)
            text = text.replace(f"SPDX-License-Identifier: {TEMPLATE_LICENSE}", f"SPDX-License-Identifier: {self.license}")
        if self.kind == "driver" and self.bus != "spi":
            t, f = BUS_TITLE[self.bus], BUS_FILE[self.bus]
            text = text.replace("SpiInterface", f"{t}Interface").replace("SpiPort", f"{t}Port")
            text = text.replace("_spi_interface", f"_{f}_interface")
        for token, value in (
            ("@ROOT@", self.root),
            ("@NS@", self.ns),
            ("@TITLE@", self.title),
            ("@DESCRIPTION@", self.description),
            ("@ORG@", self.org),
            ("@REPO@", self.repo),
        ):
            text = text.replace(token, value)
        if rel in NO_RENAME:
            return text
        # Repository links. The standard (kokeb-labs/.github) and the shared CI (kokeb-labs/ci-tools) stay.
        text = text.replace(f"{TEMPLATE_ORG}.github.io/{TEMPLATE_REPO}", f"{self.org}.github.io/{self.repo}")
        text = text.replace(f"{TEMPLATE_ORG}/{TEMPLATE_REPO}", f"{self.org}/{self.repo}")
        text = text.replace(f"orgs/{TEMPLATE_ORG}/discussions", f"orgs/{self.org}/discussions")
        text = text.replace(TEMPLATE_REPO, self.repo)
        # The CMake package and target are always <name> and <name>::<name>, whatever the C++ namespace.
        text = text.replace("xmpl100::xmpl100", "\0TARGET\0").replace("NAMESPACE xmpl100::", "\0EXPORT\0")
        # The namespace next, so that `xmpl100::` and `namespace xmpl100` take --namespace.
        text = re.sub(r"\bnamespace xmpl100\b", f"namespace {self.ns}", text)
        text = re.sub(r"(?<![\w:])xmpl100::(?=[\w~])", f"{self.ns}::", text)
        text = text.replace("\0TARGET\0", f"{self.name}::{self.name}").replace("\0EXPORT\0", f"NAMESPACE {self.name}::")
        return text.replace("XMPL100", self.upper).replace("Xmpl100", self.pascal).replace("xmpl100", self.name)

    def rename_path(self, rel: str) -> str:
        rel = rel.replace("@ROOT@", self.root)
        if self.kind == "driver" and self.bus != "spi":
            rel = rel.replace("_spi_interface", f"_{BUS_FILE[self.bus]}_interface")
        return rel.replace("XMPL100", self.upper).replace("Xmpl100", self.pascal).replace("xmpl100", self.name)


def datasheet_blocks(a: argparse.Namespace) -> dict[str, str]:
    """The datasheet link on the Hardware page and in the datasheet-conformance skill. Only the link: the PDF is
    never downloaded or committed."""
    if a.kind != "driver":
        return {}
    if a.datasheet:
        return {
            "datasheet": f"The datasheet: [XMPL100 datasheet]({a.datasheet}). Every pin, setting and timing on this"
            " page cites a\nsection of it.\n",
            "datasheet-link": f"The datasheet: <{a.datasheet}> (link only; never commit the vendor PDF).\n",
        }
    return {
        "datasheet": "The datasheet: add its link here (`tools/setup.py --datasheet <url>` does it at setup). Link"
        " the\nvendor's page or PDF; never commit the PDF.\n",
        "datasheet-link": "The datasheet: not linked yet. Add its URL to `docs/hardware.md` and here; never commit"
        " the\nvendor PDF.\n",
    }


def scaffold(a: argparse.Namespace, dest: Path) -> list[str]:
    n = Names(a)
    blocks: dict[str, str] = dict(COMMON_BLOCKS)
    blocks.update(datasheet_blocks(a))

    # 1. Overlays and removals.
    if a.kind == "library":
        for src, dst in LIBRARY_MOVE:
            (dest / dst).parent.mkdir(parents=True, exist_ok=True)
            shutil.move(str(dest / src), str(dest / dst))
        for rel in LIBRARY_REMOVE:
            remove(dest / rel)
        blocks.update(apply_overlay(OVERLAYS / "library", dest))
    else:
        blocks.update(apply_overlay(OVERLAYS / "driver", dest))
    if a.kind == "driver" and a.bus != "spi":
        remove(dest / "inc/xmpl100_spi_interface.hpp")
        blocks.update(apply_overlay(OVERLAYS / "bus" / a.bus, dest))
        listed = OVERLAYS / "bus" / a.bus / "remove.txt"
        if listed.is_file():
            for line in listed.read_text().split():
                remove(dest / line)

    # 2. The licence text.
    text = (OVERLAYS / "license" / LICENSES[a.license][0]).read_text(encoding="utf-8")
    year = str(datetime.date.today().year)
    (dest / "LICENSE").write_text(text.replace("@YEAR@", year).replace("@AUTHOR@", a.author), encoding="utf-8")

    # 3. Template-only parts, and what the platforms left out use.
    for rel in TEMPLATE_ONLY:
        remove(dest / rel)
    for platform, paths in PLATFORM_ONLY.items():
        if platform not in n.platforms:
            for rel in paths:
                remove(dest / rel)
            for overlay in (OVERLAYS / "platform" / f"no-{platform}", OVERLAYS / a.kind / f"no-{platform}"):
                blocks.update(apply_overlay(overlay, dest))

    # 4. Rewrite every text file, then rename paths (deepest first).
    for path, rel, text in list(text_files(dest)):
        if rel.as_posix() == "LICENSE":
            continue
        new = replace_blocks(text, blocks, n.platforms)
        new = TEMPLATE_ONLY_BLOCK_RE.sub("", new)
        new = TEMPLATE_ONLY_LINE_RE.sub("", new)
        new = n.rename_text(new, rel.as_posix())
        if new != text:
            path.write_text(new, encoding="utf-8")
    for path in sorted(dest.rglob("*"), key=lambda p: len(p.parts), reverse=True):
        rel = path.relative_to(dest)
        if skipped(rel) or frozen(rel):
            continue
        new_rel = n.rename_path(rel.name)
        if new_rel != rel.name:
            path.rename(path.with_name(new_rel))

    # 5. The README hero line is the description: wrap it at 120 columns for markdownlint.
    readme = dest / "README.md"
    lines = readme.read_text().split("\n")
    readme.write_text("\n".join(textwrap.fill(l, 120) if l == n.description else l for l in lines))

    # 6. A fresh changelog.
    changelog = dest / "CHANGELOG.md"
    lines = changelog.read_text().split("## [Unreleased]")[0]
    changelog.write_text(lines + "## [Unreleased]\n\n### Added\n\n" f"- {n.title}: first version.\n")

    # 7. The new names change the include order and the macro alignment: format the C++ sources.
    formatted = format_sources(dest)

    # 8. Figures and generated regions.
    regenerated = regenerate(dest)

    bus = "" if a.kind == "library" else ", " + ("GPIO only" if a.bus == "none" else a.bus.upper())
    todo = [
        f"Scaffolded {n.title} ({a.kind}{bus}; platforms {', '.join(p for p in PLATFORMS if p in n.platforms)};"
        f" {n.org}/{n.repo}, {a.license}) in {dest}",
        "Run the verify skill (.claude/skills/verify/SKILL.md) before changing anything, and commit this state.",
    ]
    if a.kind == "driver":
        if not a.datasheet:
            todo.append("docs/hardware.md and .claude/skills/datasheet-conformance/SKILL.md: link the datasheet.")
        if a.bus not in ("adc", "none"):
            todo.append(f"inc/{n.name}_registers.hpp: the real register map, with datasheet sections.")
        todo += [
            f"inc/{n.name}_types.hpp: chip ID, timing, enums, Config with the datasheet reset values, presets,"
            " Validate(); chip-specific Error values from 16, each returned by some call.",
            f"tests/mock_{n.name}.hpp: the real chip's model; then tests; then inc/{n.name}.hpp and"
            f" src/{n.name}.ipp (datasheet-conformance skill).",
            f"inc/{n.name}_{BUS_FILE[a.bus]}_interface.hpp: keep the standard hooks for the bus.",
            "examples/host/: the host port over the new model; the example program and its expected output.",
        ]
        if "esp32" in n.platforms:
            todo.append(f"examples/esp32/main/esp32_{n.name}_port.hpp: the board's pins; the example app.")
        todo += [
            "docs/: index (hero, chip card), quickstart wiring, hardware, porting, api-guide, configuration,"
            f" troubleshooting, contract: remove every statement about the example part.",
            "_config/Kconfig and inc/" + n.name + "_kconfig.hpp: the options that make sense for the chip.",
        ]
    else:
        todo += [
            f"inc/{n.root}/: replace the MovingAverage example with the library's code (PascalCase files).",
            "tests/host/: the library's tests; tests/docs/: every snippet the docs show; examples/host/: the example.",
            "docs/: index, quickstart, concepts, contract, api-guide, integration.",
        ]
    todo += [
        "README.md: the hero line, What it does, the documentation table.",
        "_config/mkdocs.yml: site_description if --description was generic.",
        "CHANGELOG.md: describe the first version under ## [Unreleased].",
        "Ask an org owner to run docs/repo-setup.md (metadata, merge settings, security, rulesets, labels).",
    ]
    if "esp32" not in n.platforms and dest == ROOT:
        todo.insert(2, "git rm --cached examples/esp32/scripts (the ESP32 example and its submodule are gone)")
    if not regenerated:
        todo.insert(2, "Install kokeb-mkdocs-theme and run: kokeb-docs figures && kokeb-docs gen")
    if not formatted:
        todo.insert(2, "Install clang-format 20 and format inc/ src/ tests/ examples/ (--style=file:_config/.clang-format)")
    return todo


def format_sources(dest: Path) -> bool:
    exe = shutil.which("clang-format")
    if not exe:
        return False
    version = subprocess.run([exe, "--version"], capture_output=True, text=True, check=False).stdout
    major = re.search(r"version (\d+)", version)
    if not major or int(major.group(1)) < 20:  # the style file uses keys added in clang-format 20
        return False
    files = [
        str(path)
        for path, rel, _ in text_files(dest)
        if rel.parts[0] in ("inc", "src", "tests", "examples") and path.suffix in (".h", ".hpp", ".ipp", ".cpp")
    ]
    if files:
        subprocess.run([exe, "--style=file:" + str(dest / "_config/.clang-format"), "-i", *files], cwd=dest, check=True)
    return True


def root_problems(root: Path, entries: list[str] | None = None) -> list[str]:
    """Entries of the repository root outside the allowed list, and symlinks anywhere in the tree."""
    if entries is None:
        entries = [p.name for p in root.iterdir()]
    bad = []
    for name in sorted(set(entries)):
        path = root / name
        if path.is_symlink():
            bad.append(f"{name}: a symlink at the root")
        elif path.is_dir():
            if name not in ROOT_DIRS:
                bad.append(f"{name}/: not an allowed root folder")
        elif name not in ROOT_FILES:
            bad.append(f"{name}: not an allowed root file (tool configuration goes in _config/)")
    for path in root.rglob("*"):
        rel = path.relative_to(root)
        if path.is_symlink() and not skipped(rel) and not frozen(rel):
            bad.append(f"{rel}: a symlink")
    return bad


def regenerate(dest: Path) -> bool:
    exe = shutil.which("kokeb-docs")
    if not exe:
        return False
    env = dict(os.environ, KOKEB_DOCS_ROOT=str(dest))
    for args in (["figures"], ["gen"]):
        if args == ["figures"] and not (dest / "docs/diagrams/figures").is_dir():
            continue
        subprocess.run([exe, *args], cwd=dest, env=env, check=True, stdout=subprocess.DEVNULL)
    return True


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--kind", choices=("driver", "library"))
    p.add_argument("--name", help="chip part number or library name: lower case, digits, underscores")
    p.add_argument("--bus", choices=BUSES,
                   help="driver bus (default spi; none: GPIO only, no data bus); a library is 'none'")
    p.add_argument("--namespace", help="C++ namespace (default: the name)")
    p.add_argument("--description", help="one sentence: what it is and does (README, docs, CMake)")
    p.add_argument("--include-root", help="library only: headers in inc/<root>/ (default: the name), e.g. ring")
    p.add_argument("--org", help=f"GitHub organisation (default: from git remote origin, else {DEFAULT_ORG})")
    p.add_argument("--repo", help="repository name (default: from git remote origin, else driver-<name> or <name>)")
    p.add_argument("--author", help="copyright holder (default: 'The <Org> Authors')")
    p.add_argument("--license", choices=tuple(LICENSES), help=f"licence (default {DEFAULT_LICENSE})")
    p.add_argument("--datasheet", help="driver only: datasheet URL for the Hardware page (only linked, never fetched)")
    p.add_argument(
        "--platforms",
        help="example platforms, comma-separated, from: " + ", ".join(PLATFORMS)
        + " (default host,esp32 for a driver, host for a library; host is always included)",
    )
    p.add_argument("--out", type=Path, help="write a scaffolded copy here instead of rewriting in place")
    p.add_argument("--selftest", action="store_true", help="scaffold every kind into temp dirs and check each")
    return p


def resolve(a: argparse.Namespace, p: argparse.ArgumentParser, remote: tuple[str, str] | None) -> argparse.Namespace:
    """Check the options and fill in every default. `remote` is (org, repo) of the destination's origin."""
    if not a.kind or not a.name:
        p.error("--kind and --name are required")
    if not re.fullmatch(r"[a-z][a-z0-9_]*", a.name) or a.name == "xmpl100":
        p.error("--name: lower case letters, digits and underscores, starting with a letter (not xmpl100)")
    if a.kind == "driver":
        a.bus = a.bus or "spi"
        bus = "GPIO only" if a.bus == "none" else a.bus.upper()
        a.description = a.description or f"C++20 driver for the {a.name.upper()} ({bus}), on any MCU."
        if a.datasheet and not re.fullmatch(r"https?://\S+", a.datasheet):
            p.error("--datasheet: an http(s) URL")
    else:
        if a.bus not in (None, "none"):
            p.error("a library has no bus: leave --bus out or pass --bus none")
        if a.datasheet:
            p.error("--datasheet is for a driver")
        a.bus = "none"
        a.include_root = a.include_root or a.name
        if not re.fullmatch(r"[a-z][a-z0-9_]*", a.include_root):
            p.error("--include-root: lower case letters, digits and underscores")
        a.description = a.description or f"C++20 library {a.name}: platform-free, no heap."
    a.namespace = a.namespace or a.name
    if not re.fullmatch(r"[a-z][a-z0-9_]*(::[a-z][a-z0-9_]*)*", a.namespace):
        p.error("--namespace: lower-case C++ namespace, e.g. tmp117 or acme::utils")
    if remote and remote[1] == TEMPLATE_REPO:
        remote = (remote[0], None)  # the template's own checkout: its name is not the new repository's
    a.org = a.org or (remote[0] if remote else None) or DEFAULT_ORG
    a.repo = a.repo or (remote[1] if remote else None) or default_repo(a.kind, a.name)
    for value, what in ((a.org, "--org"), (a.repo, "--repo")):
        if not re.fullmatch(r"[A-Za-z0-9][\w.-]*", value):
            p.error(f"{what}: a GitHub name (letters, digits, '-', '_', '.')")
    a.author = a.author or default_author(a.org)
    a.license = a.license or DEFAULT_LICENSE
    wanted = [x.strip() for x in (a.platforms or DEFAULT_PLATFORMS[a.kind]).split(",") if x.strip()]
    unknown = [x for x in wanted if x not in PLATFORMS]
    if unknown:
        p.error(
            f"--platforms: no template support for {', '.join(unknown)} (known: {', '.join(PLATFORMS)}). Scaffold"
            " with the known ones, then add the platform with the add-port skill (.claude/skills/add-port/SKILL.md):"
            " examples/<platform>/ with its port and a CI job that builds it."
        )
    a.platforms = ["host"] + [x for x in PLATFORMS if x in wanted and x != "host"]
    return a


# ---------------------------------------------------------------------------------------------------------------
# Self-test: what the template-selftest CI job runs.
# ---------------------------------------------------------------------------------------------------------------

# Drivers with the default platforms (host, esp32) on every bus, a host-only driver, a driver for another
# organisation under Apache-2.0 with a datasheet link, the default (host-only) library and a library for ESP-IDF.
# Each case runs with --org kokeb-labs unless it names another, so the template checkout's remote never leaks in.
SELFTEST_CASES = [
    ["--kind", "driver", "--name", "tmp117", "--bus", "i2c", "--description",
     "C++20 driver for the TMP117 I2C temperature sensor, on any MCU."],
    ["--kind", "driver", "--name", "ab1234", "--bus", "uart", "--namespace", "acme::ab1234"],
    ["--kind", "driver", "--name", "lm35", "--bus", "adc"],
    ["--kind", "driver", "--name", "hcsr501", "--bus", "none"],
    ["--kind", "driver", "--name", "max31865", "--bus", "spi", "--platforms", "host"],
    ["--kind", "driver", "--name", "bme280", "--bus", "i2c", "--org", "acme-robotics", "--license", "Apache-2.0",
     "--datasheet", "https://example.com/bme280-datasheet.pdf", "--platforms", "host"],
    ["--kind", "library", "--name", "ring_buffer", "--include-root", "ring"],
    ["--kind", "library", "--name", "idf_utils", "--include-root", "idfu", "--platforms", "host,esp32"],
]
REMOTE_CASES = {
    "https://github.com/acme/driver-tmp117.git": ("acme", "driver-tmp117"),
    "git@github.com:kokeb-labs/ring-buffer.git": ("kokeb-labs", "ring-buffer"),
    "ssh://git@github.com/Some-Org/driver-x1/": ("Some-Org", "driver-x1"),
    "https://example.com/not-github": None,
}


def run(cmd: list[str], cwd: Path, env: dict | None = None) -> None:
    print("+", " ".join(cmd), flush=True)
    subprocess.run(cmd, cwd=cwd, env=env, check=True)


def tracked_root_entries() -> list[str] | None:
    """The top-level names git tracks in the template checkout, or None without git."""
    try:
        out = subprocess.run(["git", "ls-files"], cwd=ROOT, capture_output=True, text=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return None
    return sorted({line.split("/", 1)[0] for line in out.splitlines() if line})


def check_output(out: Path, a: argparse.Namespace) -> list[str]:
    """What a scaffolded repository must and must not contain."""
    bad = []
    for path, rel, text in text_files(out):
        r = rel.as_posix()
        if r not in NO_RENAME and LEFTOVER_RE.search(text):
            bad.append(f"{r}: template name or marker left in")
        if FOREIGN_RE.search(text) or FOREIGN_RE.search(r):
            bad.append(f"{r}: another organisation's name")
        if rel.parts[0] in CODE_DIRS or r == "CMakeLists.txt":
            if "github.com/kokeb-labs" in text:
                bad.append(f"{r}: a link to the organisation's standard or CI in code")
            if a.org != TEMPLATE_ORG and re.search(r"kokeb", text, re.I):
                bad.append(f"{r}: the template organisation's name in code")
    name = a.name
    for rel in (f"cmake/{name}_build_settings.cmake", f"cmake/{name}Config.cmake.in"):
        if not (out / rel).is_file():
            bad.append(f"{rel}: missing")
    cmake = (out / "CMakeLists.txt").read_text()
    for needle in (f"add_library({name}::{name} ALIAS", f"EXPORT_NAME {name})", f"NAMESPACE {name}::"):
        if needle not in cmake:
            bad.append(f"CMakeLists.txt: no `{needle}`")
    if a.kind == "driver":
        header = out / f"inc/{name}_{BUS_FILE[a.bus]}_interface.hpp"
        if not header.is_file():
            bad.append(f"{header.relative_to(out)}: missing")
        if f"namespace {a.namespace} " not in (out / f"inc/{name}.hpp").read_text():
            bad.append(f"inc/{name}.hpp: not in namespace {a.namespace}")
        hardware = (out / "docs/hardware.md").read_text()
        if a.datasheet and a.datasheet not in hardware:
            bad.append("docs/hardware.md: no datasheet link")
    elif not (out / f"inc/{a.include_root}/Result.h").is_file():
        bad.append(f"inc/{a.include_root}/Result.h: missing")
    licence = (out / "LICENSE").read_text()
    if a.author not in licence and a.license == "MIT":
        bad.append("LICENSE: no copyright holder")
    if (a.license == "Apache-2.0") != ("Apache License" in licence):
        bad.append(f"LICENSE: not {a.license}")
    if f"github.com/{a.org}/{a.repo}" not in (out / "README.md").read_text():
        bad.append(f"README.md: no link to {a.org}/{a.repo}")
    return bad


def selftest(only: str | None) -> int:
    docs = shutil.which("kokeb-docs") and shutil.which("mkdocs")
    for url, want in REMOTE_CASES.items():
        if parse_remote(url) != want:
            print(f"parse_remote({url!r}) = {parse_remote(url)!r}, expected {want!r}")
            return 1
    if default_author("kokeb-labs") != "The Kokeb Labs Authors" or default_author("acme-robotics") != "The Acme Robotics Authors":
        print("default_author: wrong")
        return 1
    bad = root_problems(ROOT, tracked_root_entries())
    if bad:
        print("the template's root breaks the root rule:", *bad, sep="\n  ")
        return 1
    with tempfile.TemporaryDirectory(prefix="scaffold-") as tmp:
        for case in SELFTEST_CASES:
            label = f"{case[1]}-{case[3]}"
            if only and only not in label:
                continue
            if "--org" not in case:
                case = [*case, "--org", TEMPLATE_ORG]
            out = Path(tmp) / case[3]
            print(f"\n=== {label}", flush=True)
            run([sys.executable, str(Path(__file__).resolve()), *case, "--out", str(out)], ROOT)
            p = build_parser()
            a = resolve(p.parse_args(case), p, None)
            bad = check_output(out, a) + root_problems(out)
            if bad:
                print("the scaffolded repository is wrong:", *bad, sep="\n  ")
                return 1
            if (out / "examples/esp32").exists() != ("esp32" in a.platforms) or not (out / "examples/host").is_dir():
                print("platforms: examples/esp32 missing or left over, or examples/host missing")
                return 1
            print("root:", " ".join(sorted(p.name + ("/" if p.is_dir() else "") for p in out.iterdir())))
            build = out / "build"
            run(["cmake", "-S", ".", "-B", str(build), f"-D{a.name.upper()}_BUILD_TESTS=ON"], out)
            run(["cmake", "--build", str(build), "--parallel"], out)
            run(["ctest", "--test-dir", str(build), "--output-on-failure"], out)
            if docs:
                env = dict(os.environ, KOKEB_DOCS_ROOT=str(out), NO_MKDOCS_2_WARNING="1")
                run(["kokeb-docs", "check"], out, env)
                run(["mkdocs", "build", "-f", str(out / "_config/mkdocs.yml"), "--strict", "-d", str(out / "site")],
                    out, env)
            else:
                print("kokeb-docs / mkdocs not installed: docs checks skipped")
            shutil.rmtree(out)
    print("\nselftest passed")
    return 0


def main(argv: list[str] | None = None) -> int:
    p = build_parser()
    a = p.parse_args(sys.argv[1:] if argv is None else argv)
    if a.selftest:
        return selftest(os.environ.get("SCAFFOLD_SELFTEST_ONLY"))
    if not (ROOT / "inc/xmpl100.hpp").is_file() or not OVERLAYS.is_dir():
        fail("run this from an unmodified driver-template checkout (inc/xmpl100.hpp or tools/scaffold/ missing)")
    dest = a.out.resolve() if a.out else ROOT
    a = resolve(a, p, None if a.out else git_remote(ROOT))
    if a.out:
        if dest.exists() and any(dest.iterdir()):
            fail(f"{dest} exists and is not empty")
        copy_tree(ROOT, dest)
    todo = scaffold(a, dest)
    print("\n".join(["", "Next:"] + [f"  [ ] {t}" for t in todo]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
