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
6. Records the template commit and these arguments in .kokeb/template.yml.

Template drift: `python3 tools/scaffold.py --check-drift DIR` scaffolds the current template again with the
arguments in DIR/.kokeb/template.yml and compares the files the template manages (MANAGED below: workflows,
tool configuration, skills, CMake helpers). A file that differs is drift: take the template's version with
`--fix`, or, when the difference is the repository's own on purpose, list it under `local` with the reason.
CI runs it through kokeb-labs/ci-tools ru-template-drift.
"""

from __future__ import annotations

import argparse
import datetime
import difflib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OVERLAYS = ROOT / "tools" / "scaffold"

# ---------------------------------------------------------------------------------------------------------------
# Organisation settings. --org, --repo, --author and --license override them; --org defaults to the owner in
# `git remote get-url origin`, then to the kind's organisation in DEFAULT_ORGS: chip drivers live in their own
# organisation, libraries with the shared repositories (template, CI, handbook). The copyright holder defaults to
# "The <repo> authors": the repository owns its code whoever hosts it, so it can move and have many owners.
# ---------------------------------------------------------------------------------------------------------------
DEFAULT_ORGS = {"driver": "kokeb-labs-drivers", "library": "kokeb-labs"}
DEFAULT_ORG_HELP = "kokeb-labs-drivers for a driver, kokeb-labs for a library"
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
TEMPLATE_AUTHOR = "The driver-template authors"
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
              ".gitmodules", "driver.yml"}
ROOT_DIRS = {".github", ".claude", ".kokeb", "_config", "cmake", "docs", "examples", "inc", "src", "tests", "tools"}
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
# Template drift (--check-drift): the files every scaffolded repository keeps as the template makes them. Everything
# else (code, tests, docs, README, CHANGELOG) is the repository's own.
TEMPLATE_RECORD = ".kokeb/template.yml"
MANAGED_PREFIXES = (".github/", "_config/", ".claude/skills/", ".claude/agents/", "cmake/")
MANAGED_FILES = {"CLAUDE.md", "docs/knowledge-index.md", "docs/mechanization-policy.md", "docs/recurring-defects.md",
                 "tests/check_doxygen.cmake", "tests/check_install.cmake",
                 "tests/check_version_copy.cmake", "tools/check_standalone.py", "tools/eval_order_check.py",
                 "tools/misra_lint.py", "tools/preflight.sh"}
# Inside those folders but the repository's own from the first commit.
UNMANAGED_RE = re.compile(r"_config/Kconfig|cmake/[a-z0-9_]+_build_settings\.cmake")
# The arguments the record keeps, in order; each is a scaffold option of the same name.
RECORD_ARGS = ("kind", "name", "bus", "namespace", "description", "include_root", "org", "repo", "author", "license",
               "datasheet", "platforms")

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
    "driver.yml",
    "tests/test_bus_faults.cpp",
    "tests/test_errors.cpp",
    "tests/test_xmpl100.cpp",
    "tests/test_kconfig.cpp",
    "tests/test_version.cpp",
    "tests/test_model.cpp",
    # A library has no bus, so no untrusted input to fuzz: no tests/fuzz/ and no fuzz job.
    "tests/fuzz",
    "docs/architecture.md",
    "docs/configuration.md",
    "docs/diagrams",
    "docs/hardware.md",
    "docs/hardware.yml",
    "docs/installation.md",
    "docs/porting.md",
    "docs/troubleshooting.md",
    "examples/esp32/main/esp32_xmpl100_port.hpp",
    "examples/esp32/main/xmpl100_basic_example.cpp",
    "examples/host/host_xmpl100_port.hpp",
    ".claude/skills/add-driver-feature",
    ".claude/skills/add-port",
    ".claude/skills/datasheet-conformance",
    ".claude/skills/hardware-lessons-review",
    # A library has no datasheet (handbook ADR-0019 applies to drivers): no map, no generator, no regions.
    "docs/datasheets.yml",
    "docs/datasheet.md",
    "tools/docs/generators/datasheet.py",
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


def default_author(repo: str) -> str:
    """The copyright holder: the repository's authors, not a company or a person."""
    return f"The {repo} authors"


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
            # The include guard of the bus header (handbook KB-CPP-019): XMPL100_SPI_INTERFACE_HPP.
            text = text.replace("_SPI_INTERFACE_", f"_{f.upper()}_INTERFACE_")
        # What the repository is (handbook KB-REPO-002) and, for a driver, the bus it declares (KB-REPO-018).
        if rel == ".kokeb/handbook.yml":
            text = text.replace("kind: template", f"kind: {self.kind}")
        if rel == "driver.yml":
            text = text.replace("bus: [spi]", f"bus: [{BUS_FILE[self.bus]}]")
        for token, value in (
            ("@ROOT@", self.root),
            ("@NS@", self.ns),
            ("@TITLE@", self.title),
            # A sentence in running text: AGENTS.md goes on with "It follows the ...".
            ("@DESCRIPTION_SENTENCE@", self.description.rstrip(".") + "."),
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
    """The datasheet link: the `url` of docs/datasheets.yml (which the README card, the Datasheet map and the boxes
    on the pages are generated from), the Hardware page and the datasheet-conformance skill. Only the link: the PDF
    is never downloaded or committed. The rest of docs/datasheets.yml stays the fictional placeholder until the
    author fills it in."""
    if a.kind != "driver":
        return {}
    if a.datasheet:
        return {
            "datasheet": f"The datasheet: [XMPL100 datasheet]({a.datasheet}). Every pin, setting and timing on this"
            " page cites a\nsection of it; its revision, its SHA-256 and the sections each page relies on are in"
            "\n[`docs/datasheets.yml`](datasheets.yml).\n",
            "datasheet-link": f"The datasheet: <{a.datasheet}> (link only; never commit the vendor PDF). Record its"
            " revision, SHA-256\nand every topic the driver takes from it in `docs/datasheets.yml`.\n",
            "datasheet-url": f"    url: {a.datasheet}\n",
        }
    return {
        "datasheet": "The datasheet: not filled in yet. Fill in [`docs/datasheets.yml`](datasheets.yml) (vendor,"
        " number, revision,\ndate, `url`, `pdf`, `sha256` and the topics); the card below, the [Datasheet"
        " map](datasheet.md) and the box\nat the top of each page are generated from it. Never commit the PDF.\n",
        "datasheet-link": "The datasheet: not linked yet. Fill in `docs/datasheets.yml` (vendor, number, revision,"
        " date, `url`,\n`pdf`, `sha256` and the topics); never commit the vendor PDF.\n",
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

    # 5. The description, the datasheet link and the names went into running text: wrap every line they made
    #    longer than markdownlint's limit (MD013), whatever their length.
    for path, rel, text in list(text_files(dest)):
        if path.suffix == ".md":
            new = wrap_markdown(text)
            if new != text:
                path.write_text(new, encoding="utf-8")

    # 6. A fresh changelog.
    changelog = dest / "CHANGELOG.md"
    lines = changelog.read_text().split("## [Unreleased]")[0]
    changelog.write_text(lines + "## [Unreleased]\n\n### Added\n\n" f"- {n.title}: first version.\n")

    # 7. The new names change the include order and the macro alignment: format the C++ sources.
    formatted = format_sources(dest)

    # 8. Figures and generated regions.
    regenerated = regenerate(dest)

    # 9. What the drift check needs to scaffold this repository again.
    note = write_record(a, dest)

    bus = "" if a.kind == "library" else ", " + ("GPIO only" if a.bus == "none" else a.bus.upper())
    todo = [
        f"Scaffolded {n.title} ({a.kind}{bus}; platforms {', '.join(p for p in PLATFORMS if p in n.platforms)};"
        f" {n.org}/{n.repo}, {a.license}) in {dest}",
        "Run the verify skill (.claude/skills/verify/SKILL.md) before changing anything, and commit this state.",
    ]
    if a.kind == "driver":
        todo.append(
            "docs/datasheets.yml: the real datasheet's vendor, number, revision, date, "
            + ("" if a.datasheet else "url, ")
            + "pdf (stable direct link or null), sha256 of the PDF, and one topic per section the driver uses"
            " (section, page, driver API, docs pages); then kokeb-docs gen. It still holds the fictional placeholder."
        )
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
    if note:
        todo.insert(2, f"{TEMPLATE_RECORD}: the template commit is {note}")
    if not regenerated:
        todo.insert(2, "Install kokeb-mkdocs-theme and run: kokeb-docs figures && kokeb-docs gen")
    if not formatted:
        todo.insert(2, "Install clang-format 20 and format inc/ src/ tests/ examples/ (--style=file:_config/.clang-format)")
    return todo


# ---------------------------------------------------------------------------------------------------------------
# Markdown line length: what the scaffold puts into running text (the description, a datasheet URL, a long name)
# can push a line past markdownlint's MD013 limit (_config/.markdownlint.json). wrap_markdown() breaks exactly
# those lines at a space, carrying the rest into the next line of the paragraph; md_long_lines() is the same test
# markdownlint makes, for the self-test.
# ---------------------------------------------------------------------------------------------------------------

MD_LIMIT = 120  # _config/.markdownlint.json MD013 line_length
# A word that would start a new block (list item, heading, quote, table, fence, HTML, setext underline) at the start
# of a line: never break a line just before it.
MD_BLOCK_WORD_RE = re.compile(r"(?:[-+*>|]|#{1,6}|\d{1,9}[.)]|=+|-+|`{3,}.*|~{3,}.*|<.*)")
MD_LIST_RE = re.compile(r"^([ \t]*)(?:[-+*]|\d{1,9}[.)])[ \t]+")
MD_FENCE_RE = re.compile(r"^[ \t]*(`{3,}|~{3,})")


def md_too_long(line: str) -> bool:
    """markdownlint MD013 without `strict`: longer than the limit, with whitespace after it (a long URL alone is
    allowed)."""
    return len(line) > MD_LIMIT and re.search(r"\s", line[MD_LIMIT:]) is not None


def md_prose(lines: list[str], checked: bool = False) -> list[bool]:
    """Which lines are wrappable prose: not front matter, code, an HTML comment or block (an HTML paragraph `<p>`
    is prose), a table or a heading. With `checked`: every line MD013 checks (all but front matter, code and
    tables)."""
    prose = [False] * len(lines)
    fence = None
    comment = False
    front = bool(lines) and lines[0] == "---"
    for i, line in enumerate(lines):
        stripped = line.strip()
        if front:
            front = not (i > 0 and stripped == "---")
            continue
        if fence:
            m = MD_FENCE_RE.match(line)
            if m and m.group(1)[0] == fence[0] and len(m.group(1)) >= len(fence):
                fence = None
            continue
        m = MD_FENCE_RE.match(line)
        if m:
            fence = m.group(1)
            continue
        if comment:
            comment = "-->" not in line
            prose[i] = checked
            continue
        if stripped.startswith("<!--"):
            comment = "-->" not in stripped[4:]
            prose[i] = checked
            continue
        if not stripped or (line.startswith("    ") and i > 0 and not lines[i - 1].strip()):
            continue  # blank, or (possibly) an indented code block: left alone
        if stripped[0] == "|":
            continue
        if stripped[0] in "#<" and not re.match(r"<p[ >]", stripped):
            prose[i] = checked
            continue
        prose[i] = True
    return prose


def md_continuation(line: str) -> str:
    """The indent a wrapped part of `line` takes: under the text of a list item or quote, else the line's own."""
    m = MD_LIST_RE.match(line)
    if m:
        return " " * len(m.group(0))
    m = re.match(r"^([ \t]*(?:>[ \t]?)+)", line)
    if m:
        return m.group(1)
    return re.match(r"^[ \t]*", line).group(0)


def md_spans(line: str) -> set[int]:
    """Positions inside inline code, link text or a link target, where a break is legal but reads badly."""
    inside: set[int] = set()
    code = False
    depth = 0
    target = 0
    for i, ch in enumerate(line):
        if ch == "`":
            code = not code
        elif not code and target:
            target += {"(": 1, ")": -1}.get(ch, 0)
        elif not code and ch == "[":
            depth += 1
        elif not code and ch == "]" and depth:
            depth -= 1
            if line[i + 1:i + 2] == "(":
                target = 1
                inside.add(i + 1)
                continue
        if code or depth or target:
            inside.add(i)
    return inside


def md_break(line: str, indent: int) -> int | None:
    """Where to break an over-long line: the last space that leaves at most MD_LIMIT characters, else the first one
    after the limit; outside code and links when it can, never before a word that would start a new block, never
    inside the indent."""
    spaces = [m.start() for m in re.finditer(r" (?=\S)", line) if m.start() > indent]
    ok = [i for i in spaces if not MD_BLOCK_WORD_RE.fullmatch(line[i + 1:].split(" ", 1)[0])]
    inside = md_spans(line)
    for pool in ([i for i in ok if i not in inside], ok):
        before = [i for i in pool if i <= MD_LIMIT]
        if before:
            return before[-1]
        after = [i for i in pool if i > MD_LIMIT]
        if after:
            return after[0]
    return None


def wrap_markdown(text: str) -> str:
    """Break every prose line markdownlint would call too long; the rest of the line goes to the front of the next
    line of the same paragraph (which is checked in turn), or onto a new line."""
    lines = text.split("\n")
    prose = md_prose(lines)
    i = 0
    while i < len(lines):
        line = lines[i]
        if not prose[i] or not md_too_long(line):
            i += 1
            continue
        marker = MD_LIST_RE.match(line)
        at = md_break(line, len(marker.group(0)) if marker else len(re.match(r"^[ \t]*(?:>[ \t]?)*", line).group(0)))
        if at is None:
            i += 1
            continue
        indent = md_continuation(line)
        head, rest = line[:at].rstrip(), line[at + 1:].strip()
        lines[i] = head
        nxt = lines[i + 1] if i + 1 < len(lines) else None
        joins = (nxt is not None and prose[i + 1] and nxt.strip() and not MD_LIST_RE.match(nxt)
                 and not nxt.lstrip().startswith(">") and re.match(r"^[ \t]*", nxt).group(0) == indent
                 and not head.endswith("  ") and not line.endswith("  "))
        if joins:
            lines[i + 1] = indent + rest + " " + nxt.strip()
        else:
            lines.insert(i + 1, indent + rest)
            prose.insert(i + 1, True)
        i += 1
    return "\n".join(lines)


def md_long_lines(root: Path) -> list[str]:
    """Every Markdown prose line markdownlint's MD013 would reject (what wrap_markdown() leaves must pass)."""
    bad = []
    for path, rel, text in text_files(root):
        if path.suffix != ".md":
            continue
        lines = text.split("\n")
        for n, (line, prose) in enumerate(zip(lines, md_prose(lines, checked=True)), 1):
            if prose and md_too_long(line):
                bad.append(f"{rel.as_posix()}:{n}: {len(line)} characters (MD013 limit {MD_LIMIT})")
    return bad


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
    if not exe or os.environ.get("SCAFFOLD_NO_DOCS"):
        return False
    env = dict(os.environ, KOKEB_DOCS_ROOT=str(dest))
    for args in (["figures"], ["gen"]):
        if args == ["figures"] and not (dest / "docs/diagrams/figures").is_dir():
            continue
        subprocess.run([exe, *args], cwd=dest, env=env, check=True, stdout=subprocess.DEVNULL)
    return True


# ---------------------------------------------------------------------------------------------------------------
# Template drift.
# ---------------------------------------------------------------------------------------------------------------

def git_out(cwd: Path, *args: str) -> str | None:
    """`git <args>` in cwd, stripped, or None without git or on failure."""
    try:
        return subprocess.run(["git", *args], cwd=cwd, capture_output=True, text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def template_commit() -> str:
    """The commit of the template checkout this script runs in (--out, --check-drift --fix)."""
    return git_out(ROOT, "rev-parse", "HEAD") or "unknown"


def local_template_commits(checkout: Path, limit: int = 1000) -> list[tuple[str, str, int]]:
    """(commit, tree, committer time) of the newest commits of a template checkout, newest first."""
    out = git_out(checkout, "log", f"-n{limit}", "--format=%H %T %ct", "HEAD") or ""
    return [(c, tr, int(ts)) for c, tr, ts in (line.split() for line in out.splitlines() if line.count(" ") == 2)]


def remote_template_commits(limit: int = 300) -> list[tuple[str, str, int]] | None:
    """The same from the GitHub API for the template's default branch, or None offline (GITHUB_TOKEN or GH_TOKEN
    is used when set, for the rate limit)."""
    import urllib.request
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    headers = {"Accept": "application/vnd.github+json"}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    commits: list[tuple[str, str, int]] = []
    for page in range(1, limit // 100 + 1):
        req = urllib.request.Request(
            f"https://api.github.com/repos/{TEMPLATE_ORG}/{TEMPLATE_REPO}/commits?per_page=100&page={page}",
            headers=headers)
        try:
            with urllib.request.urlopen(req, timeout=15) as r:
                items = json.load(r)
        except (OSError, ValueError):
            return commits or None
        for item in items:
            when = datetime.datetime.fromisoformat(item["commit"]["committer"]["date"].replace("Z", "+00:00"))
            commits.append((item["sha"], item["commit"]["tree"]["sha"], int(when.timestamp())))
        if len(items) < 100:
            break
    return commits


def find_template_commit(repo: Path, checkout: Path | None, online: bool = True) -> tuple[str, str | None]:
    """The template commit a repository created from the template (`gh repo create --template`, "Use this
    template") was made from, and a note when it is not exact.

    GitHub gives such a repository a fresh first commit with the template's tree, so `git rev-parse HEAD` there is
    never a template commit. The template commit is the one with the same tree as the repository's first commit,
    found in a template checkout (`checkout`, or $SCAFFOLD_TEMPLATE) or else in the template's history on GitHub.
    Fallbacks, each marked in the record: the template's newest commit before the repository's first commit (the
    tree was changed before the scaffold ran), and `unknown` offline (`--check-drift --fix` from a template
    checkout fills it in)."""
    roots = (git_out(repo, "rev-list", "--max-parents=0", "HEAD") or "").split()
    if not roots:
        return "unknown", "not a git repository: no first commit to match against the template"
    first_tree = git_out(repo, "rev-parse", f"{roots[-1]}^{{tree}}")
    first_time = int(git_out(repo, "log", "-1", "--format=%ct", roots[-1]) or 0)
    sources: list[tuple[str, list[tuple[str, str, int]] | None]] = []
    env_checkout = os.environ.get("SCAFFOLD_TEMPLATE")
    for path in (checkout, Path(env_checkout) if env_checkout else None):
        if path and path.resolve() != repo.resolve():
            sources.append((f"the template checkout {path}", local_template_commits(path)))
    if online:
        sources.append((f"{TEMPLATE_ORG}/{TEMPLATE_REPO} on GitHub", remote_template_commits()))
    found = [(where, commits) for where, commits in sources if commits]
    for _, commits in found:
        for commit, tree, _ in commits:
            if tree == first_tree:
                return commit, None
    for where, commits in found:
        older = [c for c, _, ts in commits if ts <= first_time]
        if older:
            return older[0], (f"approximate: no template commit has this repository's first tree; this is the newest"
                              f" commit of {where} before the repository was created")
    return "unknown", ("not found: no template history to match (offline?); run --check-drift --fix from a"
                       " template checkout to record it")


def record_text(values: dict, local: list[dict], commit: str, note: str | None = None) -> str:
    lines = [
        "---",
        "# How this repository was made from kokeb-labs/driver-template. The template's drift check",
        "# (tools/scaffold.py --check-drift, CI: kokeb-labs/ci-tools ru-template-drift) scaffolds the template again",
        "# with these arguments and compares the files it manages. List a file under `local` only when it differs on",
        "# purpose, with the reason.",
        *([f"# template commit {note}"] if note else []),
        f"template: {commit}",
    ]
    for key in RECORD_ARGS:
        v = values.get(key)
        if v is None:
            lines.append(f"{key}: null")
        elif isinstance(v, list):
            lines.append(f"{key}: [{', '.join(v)}]")
        else:
            lines.append(f"{key}: {json.dumps(v)}")
    if local:
        lines.append("local:")
        for item in local:
            lines.append(f"  - path: {item['path']}")
            lines.append(f"    reason: {json.dumps(item['reason'])}")
    else:
        lines.append("local: []")
    return "\n".join(lines) + "\n"


def record_commit(dest: Path, in_place: bool, checkout: Path | None = None,
                  online: bool = True) -> tuple[str, str | None]:
    """The template commit for the record of a scaffold into dest. A copy (--out) and a scaffold in the template's
    own checkout come from the checkout's HEAD; a repository created from the template has its own first commit, so
    its template commit is looked up (find_template_commit)."""
    remote = git_remote(dest)
    if not in_place or (remote and remote[1] == TEMPLATE_REPO):
        return template_commit(), None
    return find_template_commit(dest, checkout, online)


def write_record(a: argparse.Namespace, dest: Path) -> str | None:
    """Write .kokeb/template.yml; return the note when the template commit is not exact."""
    values = {k: getattr(a, k, None) for k in RECORD_ARGS}
    if a.kind != "library":
        values["include_root"] = None
    commit, note = record_commit(dest, in_place=dest.resolve() == ROOT.resolve())
    (dest / TEMPLATE_RECORD).write_text(record_text(values, [], commit, note), encoding="utf-8")
    return note


def read_record(path: Path) -> tuple[dict, list[dict], str]:
    """Read .kokeb/template.yml: the small YAML subset record_text() writes."""
    values: dict = {}
    local: list[dict] = []
    commit = "unknown"

    def scalar(raw: str):
        raw = raw.strip()
        if raw == "null":
            return None
        if raw.startswith('"'):
            return json.loads(raw)
        if raw.startswith("[") and raw.endswith("]"):
            return [x.strip() for x in raw[1:-1].split(",") if x.strip()]
        return raw

    for n, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or line.lstrip().startswith("#") or line == "---":
            continue
        m = re.fullmatch(r"  - path: (\S+)", line)
        if m:
            local.append({"path": m.group(1), "reason": ""})
            continue
        m = re.fullmatch(r"    reason: (.+)", line)
        if m and local:
            local[-1]["reason"] = scalar(m.group(1))
            continue
        m = re.fullmatch(r"([a-z_]+): ?(.*)", line)
        if not m:
            fail(f"{path}:{n}: not understood: {line}")
        key, raw = m.groups()
        if key == "template":
            commit = raw.strip()
        elif key == "local":
            if raw.strip() not in ("", "[]"):
                fail(f"{path}:{n}: local is a list of path/reason entries")
        elif key in RECORD_ARGS:
            values[key] = scalar(raw)
        else:
            fail(f"{path}:{n}: unknown key {key}")
    for item in local:
        if not item["reason"]:
            fail(f"{path}: local entry {item['path']} has no reason")
    return values, local, commit


def managed(rel: str) -> bool:
    if UNMANAGED_RE.fullmatch(rel):
        return False
    return rel in MANAGED_FILES or rel.startswith(MANAGED_PREFIXES)


def check_drift(repo: Path, fix: bool) -> int:
    """Compare the files the template manages in `repo` with a fresh scaffold. 0: no drift (or fixed)."""
    repo = repo.resolve()
    if (repo / "inc/xmpl100.hpp").is_file() and (repo / "tools/scaffold.py").is_file():
        print("this is driver-template itself: nothing to compare")
        return 0
    record = repo / TEMPLATE_RECORD
    if not record.is_file():
        print(f"{TEMPLATE_RECORD}: missing; add it with the arguments this repository was scaffolded with")
        return 1
    values, local, _ = read_record(record)
    args: list[str] = []
    for key in RECORD_ARGS:
        v = values.get(key)
        if v is None:
            continue
        args += [f"--{key.replace('_', '-')}", ",".join(v) if isinstance(v, list) else str(v)]
    exempt = {item["path"]: item["reason"] for item in local}
    gha = bool(os.environ.get("GITHUB_ACTIONS"))
    drift: list[str] = []
    with tempfile.TemporaryDirectory(prefix="drift-") as tmp:
        out = Path(tmp) / "repo"
        env = dict(os.environ, SCAFFOLD_NO_DOCS="1")
        done = subprocess.run([sys.executable, str(Path(__file__).resolve()), *args, "--out", str(out)], cwd=ROOT,
                              env=env, capture_output=True, text=True)
        if done.returncode != 0:
            print(done.stdout + done.stderr)
            fail("the scaffold failed with the recorded arguments")
        for path in sorted(p for p in out.rglob("*") if p.is_file()):
            rel = path.relative_to(out).as_posix()
            if not managed(rel) or skipped(Path(rel)):
                continue
            mine = repo / rel
            want = path.read_text(encoding="utf-8", errors="replace")
            have = mine.read_text(encoding="utf-8", errors="replace") if mine.is_file() else None
            if have == want:
                if rel in exempt:
                    print(f"{rel}: listed under local but matches the template; remove it from local")
                    drift.append(rel)
                continue
            if rel in exempt:
                print(f"{rel}: differs, kept on purpose ({exempt[rel]})")
                continue
            drift.append(rel)
            if fix:
                mine.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, mine)
                print(f"{rel}: replaced with the template's version")
                continue
            what = "missing" if have is None else "differs from the template"
            print(f"\n{rel}: {what}")
            diff = list(difflib.unified_diff((have or "").splitlines(), want.splitlines(), f"{rel} (this repository)",
                                             f"{rel} (template)", lineterm="", n=2))
            print("\n".join(diff[:80] + (["  ..."] if len(diff) > 80 else [])))
            if gha:
                print(f"::warning file={rel}::{what}: take the template's version (scaffold.py --check-drift --fix)"
                      " or list it under local in .kokeb/template.yml with the reason")
    for rel in exempt:
        if not managed(rel):
            print(f"{rel}: listed under local but not a file the template manages; remove it from local")
            drift.append(rel)
    if fix:
        record.write_text(record_text(values, local, template_commit()), encoding="utf-8")
        print(f"{TEMPLATE_RECORD}: template commit updated")
        return 0
    if drift:
        print(f"\ntemplate drift: {len(drift)} file(s)")
        return 1
    print("no template drift")
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--kind", choices=("driver", "library"))
    p.add_argument("--name", help="chip part number or library name: lower case, digits, underscores")
    p.add_argument("--bus", choices=BUSES,
                   help="driver bus (default spi; none: GPIO only, no data bus); a library is 'none'")
    p.add_argument("--namespace", help="C++ namespace (default: the name)")
    p.add_argument("--description", help="one sentence: what it is and does (README, docs, CMake)")
    p.add_argument("--include-root", help="library only: headers in inc/<root>/ (default: the name), e.g. ring")
    p.add_argument("--org", help=f"GitHub organisation (default: from git remote origin, else {DEFAULT_ORG_HELP})")
    p.add_argument("--repo", help="repository name (default: from git remote origin, else driver-<name> or <name>)")
    p.add_argument("--author", help="copyright holder (default: 'The <repo> authors')")
    p.add_argument("--license", choices=tuple(LICENSES), help=f"licence (default {DEFAULT_LICENSE})")
    p.add_argument("--datasheet", help="driver only: datasheet URL for the Hardware page (only linked, never fetched)")
    p.add_argument(
        "--platforms",
        help="example platforms, comma-separated, from: " + ", ".join(PLATFORMS)
        + " (default host,esp32 for a driver, host for a library; host is always included)",
    )
    p.add_argument("--out", type=Path, help="write a scaffolded copy here instead of rewriting in place")
    p.add_argument("--selftest", action="store_true", help="scaffold every kind into temp dirs and check each")
    p.add_argument("--check-drift", type=Path, metavar="DIR",
                   help="compare the files the template manages in DIR with a fresh scaffold (.kokeb/template.yml)")
    p.add_argument("--fix", action="store_true", help="with --check-drift: take the template's version of each")
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
    a.org = a.org or (remote[0] if remote else None) or DEFAULT_ORGS[a.kind]
    a.repo = a.repo or (remote[1] if remote else None) or default_repo(a.kind, a.name)
    for value, what in ((a.org, "--org"), (a.repo, "--repo")):
        if not re.fullmatch(r"[A-Za-z0-9][\w.-]*", value):
            p.error(f"{what}: a GitHub name (letters, digits, '-', '_', '.')")
    a.author = a.author or default_author(a.repo)
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
# Each case runs with its kind's default organisation (DEFAULT_ORGS) unless it names another, so the template
# checkout's remote never leaks in.
# The TMP117 description and the BME280 datasheet URL are long on purpose: the Markdown they go into must still
# pass markdownlint's line length.
SELFTEST_CASES = [
    ["--kind", "driver", "--name", "tmp117", "--bus", "i2c", "--description",
     "C++20 driver for the Texas Instruments TMP117 high-accuracy, low-power digital temperature sensor on I2C, with"
     " the alert pin, the EEPROM and the offset register, on any MCU."],
    ["--kind", "driver", "--name", "ab1234", "--bus", "uart", "--namespace", "acme::ab1234"],
    ["--kind", "driver", "--name", "lm35", "--bus", "adc"],
    ["--kind", "driver", "--name", "hcsr501", "--bus", "none"],
    ["--kind", "driver", "--name", "max31865", "--bus", "spi", "--platforms", "host"],
    ["--kind", "driver", "--name", "bme280", "--bus", "i2c", "--org", "acme-robotics", "--license", "Apache-2.0",
     "--datasheet", "https://www.example.com/media/sensors/downloads/datasheets/environmental/bst-bme280-ds002.pdf",
     "--platforms", "host"],
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


HEADER_SUFFIXES = (".h", ".hh", ".hpp", ".hxx", ".ipp", ".inl")


def include_guard(name: str, rel: str) -> str:
    """The include guard handbook KB-CPP-019 gives a header: <NAME>_<FILE_STEM>_<EXT> in upper case, the name not
    repeated when the stem starts with it (inc/tmp117_types.hpp -> TMP117_TYPES_HPP; a configured .h.in guards as
    its .h). The handbook's checks/source_checks.py is the reference; this copy keeps the scaffold standalone."""
    file = rel.rsplit("/", 1)[-1]
    file = file[:-3] if file.endswith(".in") else file
    stem, ext = file.rsplit(".", 1)

    def snake(s: str) -> str:
        s = re.sub(r"(?<=[a-z0-9])(?=[A-Z])", "_", s)
        return re.sub(r"[^A-Za-z0-9]+", "_", s).strip("_").upper()

    prefix, base = snake(name), snake(stem)
    if base != prefix and not base.startswith(prefix + "_"):
        base = f"{prefix}_{base}"
    return f"{base}_{ext.upper()}"


def guard_problems(out: Path, name: str) -> list[str]:
    """Every header in the code folders has its KB-CPP-019 guard; nothing uses #pragma."""
    bad = []
    for path, rel, text in text_files(out):
        r = rel.as_posix()
        if rel.parts[0] not in ("inc", "src", "tests", "examples") or frozen(rel):
            continue
        if re.search(r"^\s*#\s*pragma\b", text, re.M):
            bad.append(f"{r}: #pragma (headers use #ifndef include guards)")
        plain = r[:-3] if r.endswith(".in") else r
        if not plain.endswith(HEADER_SUFFIXES):
            continue
        want = include_guard(name, r)
        if not re.search(rf"^#ifndef {want}\n#define {want}\n", text, re.M) or not re.search(
                rf"^#endif // {want}\n\Z", text, re.M):
            bad.append(f"{r}: no `#ifndef {want}` / `#define {want}` ... `#endif // {want}` include guard")
    return bad


def check_output(out: Path, a: argparse.Namespace) -> list[str]:
    """What a scaffolded repository must and must not contain."""
    bad = guard_problems(out, a.name)
    for path, rel, text in text_files(out):
        r = rel.as_posix()
        if r not in NO_RENAME and LEFTOVER_RE.search(text):
            bad.append(f"{r}: template name or marker left in")
        if FOREIGN_RE.search(text) or FOREIGN_RE.search(r):
            bad.append(f"{r}: another organisation's name")
        if rel.parts[0] in CODE_DIRS or r == "CMakeLists.txt":
            if "github.com/kokeb-labs" in text:
                bad.append(f"{r}: a link to the organisation's standard or CI in code")
            if a.org not in DEFAULT_ORGS.values() and re.search(r"kokeb", text, re.I):
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
        sheets = out / "docs/datasheets.yml"
        if not sheets.is_file() or not (out / "docs/datasheet.md").is_file():
            bad.append("docs/datasheets.yml or docs/datasheet.md: missing")
        elif a.datasheet and f"url: {a.datasheet}\n" not in sheets.read_text():
            bad.append("docs/datasheets.yml: url is not the --datasheet link")
    else:
        if not (out / f"inc/{a.include_root}/Result.h").is_file():
            bad.append(f"inc/{a.include_root}/Result.h: missing")
        for rel in ("docs/datasheets.yml", "docs/datasheet.md", "tools/docs/generators/datasheet.py"):
            if (out / rel).exists():
                bad.append(f"{rel}: a library has no datasheet")
        for path, rel, text in text_files(out):
            if path.suffix == ".md" and rel.parts[0] in ("README.md", "docs") and re.search(r"generated: datasheet-|Datasheet map|datasheet\.md", text):
                bad.append(f"{rel.as_posix()}: a datasheet region or link in a library")
    licence = (out / "LICENSE").read_text()
    if a.author not in licence and a.license == "MIT":
        bad.append("LICENSE: no copyright holder")
    if (a.license == "Apache-2.0") != ("Apache License" in licence):
        bad.append(f"LICENSE: not {a.license}")
    if f"github.com/{a.org}/{a.repo}" not in (out / "README.md").read_text():
        bad.append(f"README.md: no link to {a.org}/{a.repo}")
    bad += standalone_problems(out, a.repo)
    bad += misra_lint_problems(out)
    bad += eval_order_problems(out)
    # Markdown the description, the datasheet link or the names made too long (markdownlint MD013).
    bad += md_long_lines(out)
    # The record names the template commit the copy was made from: this checkout's HEAD.
    _, _, commit = read_record(out / TEMPLATE_RECORD)
    if commit != template_commit():
        bad.append(f"{TEMPLATE_RECORD}: template {commit}, not this checkout's commit {template_commit()}")
    return bad


def standalone_problems(root: Path, repo: str) -> list[str]:
    """tools/check_standalone.py --strict: no other project's name (boards, test projects, driver repositories),
    no dated history, and the copyright of "The <repo> authors" (or --author) everywhere it is stated."""
    script = root / "tools/check_standalone.py"
    if not script.is_file():
        return ["tools/check_standalone.py: missing"]
    done = subprocess.run([sys.executable, str(script), "--root", str(root), "--repo", repo, "--strict"],
                          capture_output=True, text=True, check=False)
    return [] if done.returncode == 0 else [f"stand-alone check: {line}" for line in done.stdout.splitlines()
                                            if line.strip() and not line.startswith("stand-alone check:")]


def misra_lint_problems(root: Path) -> list[str]:
    """tools/misra_lint.py over the scaffolded tree: the defect classes of the MISRA scan must not be in the generated code."""
    script = root / "tools/misra_lint.py"
    if not script.is_file():
        return ["tools/misra_lint.py: missing"]
    done = subprocess.run([sys.executable, str(script), "--root", str(root), "--quiet"],
                          capture_output=True, text=True, check=False)
    return [] if done.returncode == 0 else [f"misra_lint: {line}" for line in done.stdout.splitlines() if line.strip()]


def eval_order_problems(root: Path) -> list[str]:
    """tools/eval_order_check.py over the tree: no call takes an operation and a read of its state as arguments."""
    script = root / "tools/eval_order_check.py"
    if not script.is_file():
        return ["tools/eval_order_check.py: missing"]
    done = subprocess.run([sys.executable, "-I", str(script), "--root", str(root), "--quiet"],
                          capture_output=True, text=True, check=False)
    return [] if done.returncode == 0 else [f"eval_order_check: {line}" for line in done.stdout.splitlines() if line.strip()]


def record_problems() -> list[str]:
    """A repository created from the template (`gh repo create --template`) gets a fresh first commit with the
    template's tree: its record must name the template commit, never that first commit. Simulated offline here: a
    new repository whose first commit has this checkout's tree (and one whose tree was changed), matched against
    this checkout's history."""
    head = template_commit()
    objects = git_out(ROOT, "rev-parse", "--git-path", "objects")
    if head == "unknown" or not objects:
        print("not a git checkout: template commit lookup not tested")
        return []
    objects_dir = (ROOT / objects).resolve()
    bad = []
    stamp = str(int(datetime.datetime.now().timestamp()))
    env = dict(os.environ, GIT_AUTHOR_NAME="Sim", GIT_AUTHOR_EMAIL="sim@example.com", GIT_COMMITTER_NAME="Sim",
               GIT_COMMITTER_EMAIL="sim@example.com", GIT_AUTHOR_DATE=f"{stamp} +0000",
               GIT_COMMITTER_DATE=f"{stamp} +0000")

    def git(repo: Path, *args: str, stdin: str | None = None, **extra: str) -> str:
        return subprocess.run(["git", *args], cwd=repo, input=stdin, capture_output=True, text=True, check=True,
                              env={**env, **extra}).stdout.strip()

    with tempfile.TemporaryDirectory(prefix="record-") as tmp:
        for changed in (False, True):
            repo = Path(tmp) / f"driver-sim{int(changed)}"
            repo.mkdir()
            git(repo, "init", "-q")
            # This checkout's objects, read through alternates: nothing is written to the checkout.
            (repo / ".git/objects/info/alternates").write_text(f"{objects_dir}\n", encoding="utf-8")
            tree = git(repo, "rev-parse", f"{head}^{{tree}}")
            if changed:  # the template's tree plus one file: no template commit has it
                index = str(Path(tmp) / f"index{int(changed)}")
                blob = git(repo, "hash-object", "-w", "--stdin", stdin="changed before the scaffold ran\n")
                git(repo, "read-tree", tree, GIT_INDEX_FILE=index)
                git(repo, "update-index", "--add", "--cacheinfo", f"100644,{blob},EXTRA.txt", GIT_INDEX_FILE=index)
                tree = git(repo, "write-tree", GIT_INDEX_FILE=index)
            first = git(repo, "commit-tree", tree, "-m", "Initial commit")
            git(repo, "update-ref", "HEAD", first)
            git(repo, "remote", "add", "origin", "https://github.com/acme/driver-sim.git")
            if first == head:
                bad.append("record: the simulated first commit is the template commit; the test proves nothing")
            commit, note = record_commit(repo, in_place=True, checkout=ROOT, online=False)
            if commit != head or (note is None) == changed:
                bad.append(f"record ({'changed' if changed else 'same'} tree): template {commit} ({note}), expected"
                           f" {head}{' with a note' if changed else ''}; the repository's own HEAD is {first}")
            commit, note = record_commit(repo, in_place=True, checkout=None, online=False)
            if commit != "unknown" or not note:
                bad.append(f"record offline: {commit} ({note}), expected unknown with a note")
            path = repo / "record.yml"
            path.write_text(record_text({"kind": "driver"}, [], head, "approximate: a note"), encoding="utf-8")
            if read_record(path)[2] != head:
                bad.append("record: a record with a note does not read back")
    return bad


def tidy_exe() -> str | None:
    """clang-tidy 20 or newer, as tools/preflight.sh finds it (CLANG_TIDY, clang-tidy-20, Homebrew llvm@20)."""
    for exe in (os.environ.get("CLANG_TIDY"), "clang-tidy-20", "/opt/homebrew/opt/llvm@20/bin/clang-tidy",
                "clang-tidy"):
        path = exe and shutil.which(exe)
        if path:
            version = subprocess.run([path, "--version"], capture_output=True, text=True, check=False).stdout
            major = re.search(r"version (\d+)", version)
            if major and int(major.group(1)) >= 20:
                return path
    return None


TIDY_SKIP_RE = re.compile(r"(build[^/]*|docs|_site|examples/esp32|tools/scaffold|tests/compile_fail)(/|$)")


def tidy_problems(out: Path, build: Path, exe: str) -> list[str]:
    """clang-tidy on every header alone, as tools/preflight.sh and CI lint them: each must include what it uses
    and be clean under the configuration of its folder (_config/.clang-tidy at the root, as CI links it)."""
    headers = sorted(str(p) for p in out.rglob("*") if p.suffix in (".hpp", ".h") and p.is_file()
                     and not TIDY_SKIP_RE.match(p.relative_to(out).as_posix()))
    if not headers:
        return []
    extra = []
    if shutil.which("xcrun"):
        sdk = subprocess.run(["xcrun", "--show-sdk-path"], capture_output=True, text=True, check=False).stdout.strip()
        extra = ["--extra-arg=-isysroot", f"--extra-arg={sdk}"] if sdk else []
    link = out / ".clang-tidy"
    link.symlink_to("_config/.clang-tidy")
    try:
        done = subprocess.run([exe, "-p", str(build), "--quiet", *extra, *headers], cwd=out, capture_output=True,
                              text=True, check=False)
    finally:
        link.unlink()
    text = done.stdout + done.stderr
    bad = []
    for line in text.splitlines():
        m = re.match(rf"^{re.escape(str(out))}/(\S+?):\d+:\d+: (?:warning|error):", line)
        if m and not TIDY_SKIP_RE.match(m.group(1)):
            bad.append(f"clang-tidy: {line[len(str(out)) + 1:]}")
    bad = sorted(set(bad))
    if re.search(r"^Error while processing|error: no such file|LLVM ERROR", text, re.M):
        bad.append("clang-tidy: could not process every header" + ("" if bad else ":\n" + text[-2000:]))
    return bad


def markdownlint_problems(out: Path) -> list[str]:
    """markdownlint-cli2 at the version tools/preflight.sh pins, over the scaffolded Markdown (needs npx)."""
    pin = re.search(r"markdownlint-cli2@[\d.]+", (ROOT / "tools/preflight.sh").read_text(encoding="utf-8"))
    done = subprocess.run(["npx", "-y", pin.group(0) if pin else "markdownlint-cli2", "--config",
                           "_config/.markdownlint.json", "**/*.md", "#build*", "#examples/esp32/scripts",
                           "#node_modules"], cwd=out, capture_output=True, text=True, check=False)
    if done.returncode == 0:
        return []
    return [f"markdownlint: {line}" for line in (done.stdout + done.stderr).splitlines() if " MD0" in line] or [
        "markdownlint failed:\n" + (done.stdout + done.stderr)[-2000:]]


def selftest(only: str | None) -> int:
    docs = shutil.which("kokeb-docs") and shutil.which("mkdocs")
    for url, want in REMOTE_CASES.items():
        if parse_remote(url) != want:
            print(f"parse_remote({url!r}) = {parse_remote(url)!r}, expected {want!r}")
            return 1
    p = build_parser()
    for kind, name in (("driver", "tmp117"), ("library", "ring_buffer")):
        if resolve(p.parse_args(["--kind", kind, "--name", name]), p, None).org != DEFAULT_ORGS[kind]:
            print(f"resolve: a {kind} without --org or a remote is not in {DEFAULT_ORGS[kind]}")
            return 1
    if default_author("driver-tmp117") != "The driver-tmp117 authors":
        print("default_author: wrong")
        return 1
    bad = root_problems(ROOT, tracked_root_entries())
    if bad:
        print("the template's root breaks the root rule:", *bad, sep="\n  ")
        return 1
    bad = guard_problems(ROOT, "xmpl100")
    if bad:
        print("the template's include guards are wrong:", *bad, sep="\n  ")
        return 1
    if subprocess.run([sys.executable, str(ROOT / "tools/check_standalone.py"), "--selftest"], check=False).returncode:
        return 1
    if subprocess.run([sys.executable, str(ROOT / "tools/misra_lint.py"), "--selftest"], check=False).returncode:
        return 1
    bad = misra_lint_problems(ROOT)
    if bad:
        print("the template breaks its own MISRA lint:", *bad, sep="\n  ")
        return 1
    if subprocess.run([sys.executable, "-I", str(ROOT / "tools/eval_order_check.py"), "--selftest"],
                      check=False).returncode:
        return 1
    bad = eval_order_problems(ROOT)
    if bad:
        print("the template breaks the evaluation-order rule:", *bad, sep="\n  ")
        return 1
    bad = standalone_problems(ROOT, TEMPLATE_REPO)
    if bad:
        print("the template is not stand-alone:", *bad, sep="\n  ")
        return 1
    bad = record_problems()
    if bad:
        print("the template commit in .kokeb/template.yml is wrong:", *bad, sep="\n  ")
        return 1
    tidy = tidy_exe()
    npx = shutil.which("npx")
    with tempfile.TemporaryDirectory(prefix="scaffold-") as tmp:
        for case in SELFTEST_CASES:
            label = f"{case[1]}-{case[3]}"
            if only and only not in label:
                continue
            if "--org" not in case:
                case = [*case, "--org", DEFAULT_ORGS[case[1]]]
            out = Path(tmp) / case[3]
            print(f"\n=== {label}", flush=True)
            run([sys.executable, str(Path(__file__).resolve()), *case, "--out", str(out)], ROOT)
            p = build_parser()
            a = resolve(p.parse_args(case), p, None)
            bad = check_output(out, a) + root_problems(out)
            if bad:
                print("the scaffolded repository is wrong:", *bad, sep="\n  ")
                return 1
            if check_drift(out, fix=False) != 0:
                print("a fresh scaffold drifts from the template")
                return 1
            if (out / "examples/esp32").exists() != ("esp32" in a.platforms) or not (out / "examples/host").is_dir():
                print("platforms: examples/esp32 missing or left over, or examples/host missing")
                return 1
            print("root:", " ".join(sorted(p.name + ("/" if p.is_dir() else "") for p in out.iterdir())))
            if npx:
                bad = markdownlint_problems(out)
                if bad:
                    print("the scaffolded Markdown fails markdownlint:", *bad, sep="\n  ")
                    return 1
                print("markdownlint: clean")
            else:
                print("npx not installed: markdownlint skipped (the line-length check above ran)")
            build = out / "build"
            run(["cmake", "-S", ".", "-B", str(build), f"-D{a.name.upper()}_BUILD_TESTS=ON",
                 "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"], out)
            run(["cmake", "--build", str(build), "--parallel"], out)
            run(["ctest", "--test-dir", str(build), "--output-on-failure"], out)
            if tidy:
                # As tools/preflight.sh: lint with a Clang compilation database when clang++ is installed.
                db = build
                clang = shutil.which("clang++-20") or shutil.which("clang++")
                if clang:
                    db = out / "build-tidy"
                    run(["cmake", "-S", ".", "-B", str(db), f"-D{a.name.upper()}_BUILD_TESTS=ON",
                         f"-DCMAKE_CXX_COMPILER={clang}", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"], out)
                bad = tidy_problems(out, db, tidy)
                if bad:
                    print("a scaffolded header fails clang-tidy on its own:", *bad, sep="\n  ")
                    return 1
                print("clang-tidy, every header alone: clean")
            else:
                print("clang-tidy 20 not installed: header-alone clang-tidy skipped")
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
    if a.check_drift:
        return check_drift(a.check_drift, a.fix)
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
