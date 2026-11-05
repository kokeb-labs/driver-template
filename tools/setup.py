#!/usr/bin/env python3
"""Set up a repository created from driver-template (Python 3.9+, standard library only).

    python3 tools/setup.py                       # in a terminal: asks for what it cannot infer
    python3 tools/setup.py --non-interactive --bus i2c --description "C++20 driver for the TMP117 ..."
    python3 tools/setup.py --datasheet https://vendor.example/tmp117.pdf   # links it, never downloads it

It infers the kind and name from the repository name (`git remote get-url origin`, else the folder name):
`driver-tmp117` is the driver `tmp117`, any other name a library (`ring-buffer` is `ring_buffer`). It asks only
for the bus and a one-sentence description (and the namespace for a library), runs tools/scaffold.py in place
with the organisation, repository, author and licence options, then configures, builds and runs the host tests
and prints what is left to do. Every scaffold option can be given as a flag; with --non-interactive (or without
a terminal) nothing is asked and a missing value takes the scaffold's default.

The scaffold removes this script with the rest of the template tooling.
"""

from __future__ import annotations

import argparse
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import scaffold  # noqa: E402  (tools/scaffold.py, next to this file)

BUS_HELP = {
    "spi": "SPI",
    "i2c": "I2C",
    "uart": "UART",
    "adc": "analog output read by an ADC",
    "none": "no data bus: GPIO pins only",
}


def infer(repo: str) -> tuple[str, str]:
    """(kind, name) from a repository name: driver-tmp117 -> (driver, tmp117), ring-buffer -> (library, ring_buffer)."""
    slug = repo.lower()
    if slug.startswith("driver-") and len(slug) > len("driver-"):
        return "driver", slug[len("driver-"):].replace("-", "_").replace(".", "_")
    return "library", slug.replace("-", "_").replace(".", "_")


def ask(prompt: str, default: str | None = None, choices: tuple[str, ...] | None = None) -> str:
    hint = f" [{default}]" if default else ""
    while True:
        try:
            answer = input(f"{prompt}{hint}: ").strip()
        except EOFError:
            answer = ""
        answer = answer or (default or "")
        if not answer:
            print("  a value is needed")
        elif choices and answer not in choices:
            print(f"  one of: {', '.join(choices)}")
        else:
            return answer


def parse(argv: list[str]) -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--kind", choices=("driver", "library"), help="default: from the repository name")
    p.add_argument("--name", help="default: from the repository name (driver-<name> or <name>)")
    p.add_argument("--bus", choices=scaffold.BUSES, help="driver bus; asked in a terminal (default spi)")
    p.add_argument("--description", help="one sentence; asked in a terminal")
    p.add_argument("--namespace", help="C++ namespace; asked for a library in a terminal (default: the name)")
    p.add_argument("--include-root", help="library only: headers in inc/<root>/ (default: the name)")
    p.add_argument("--platforms", help="example platforms (default host,esp32 for a driver, host for a library)")
    p.add_argument("--org", help=f"GitHub organisation (default: from the remote, else {scaffold.DEFAULT_ORG})")
    p.add_argument("--repo", help="repository name (default: from the remote, else the folder name)")
    p.add_argument("--author", help="copyright holder (default: 'The <Org> Authors')")
    p.add_argument("--license", choices=tuple(scaffold.LICENSES), help=f"default {scaffold.DEFAULT_LICENSE}")
    p.add_argument("--datasheet", help="driver only: datasheet URL, linked on the Hardware page (never downloaded)")
    p.add_argument("--non-interactive", action="store_true", help="ask nothing, even in a terminal")
    p.add_argument("--no-build", action="store_true", help="scaffold only: skip configure, build and host tests")
    return p.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    a = parse(sys.argv[1:] if argv is None else argv)
    if not (ROOT / "tools/scaffold.py").is_file() or not (ROOT / "inc/xmpl100.hpp").is_file():
        sys.exit("setup: this repository is set up already (tools/scaffold.py or inc/xmpl100.hpp is gone)")
    interactive = sys.stdin.isatty() and not a.non_interactive

    remote = scaffold.git_remote(ROOT)
    org, repo = remote if remote else (None, ROOT.name)
    a.org = a.org or org
    a.repo = a.repo or repo
    if a.repo == scaffold.TEMPLATE_REPO and not (a.kind and a.name):
        if not interactive:
            sys.exit("setup: this is the template itself (driver-template); pass --kind and --name, or create a"
                     " repository from it first (docs/template.md)")
        a.repo = None
    if a.repo:
        kind, name = infer(a.repo)
        a.kind = a.kind or kind
        a.name = a.name or name
    if interactive:
        print(f"Setting up {a.org or scaffold.DEFAULT_ORG}/{a.repo or '<repository>'}.")
        if not a.kind:
            a.kind = ask("Kind", "driver", ("driver", "library"))
        if not a.name:
            a.name = ask("Name (lower case, digits, underscores)")
        print(f"  {a.kind} {a.name}")
        if a.kind == "driver" and not a.bus:
            for bus, what in BUS_HELP.items():
                print(f"  {bus:5} {what}")
            a.bus = ask("Bus", "spi", scaffold.BUSES)
        if a.kind == "library" and not a.namespace:
            a.namespace = ask("C++ namespace", a.name)
        if not a.description:
            a.description = ask("One sentence: what it is and does")
    if not a.kind or not a.name:
        sys.exit("setup: cannot infer the kind and name; pass --kind and --name")

    cmd = [sys.executable, str(ROOT / "tools/scaffold.py"), "--kind", a.kind, "--name", a.name]
    for flag in ("bus", "description", "namespace", "include_root", "platforms", "org", "repo", "author", "license",
                 "datasheet"):
        value = getattr(a, flag)
        if value:
            cmd += [f"--{flag.replace('_', '-')}", value]
    print("+ python3", shlex.join(cmd[1:]), flush=True)
    if subprocess.run(cmd, cwd=ROOT).returncode:
        return 1

    if not a.no_build:
        if not shutil.which("cmake"):
            print("\ncmake not found: install CMake 3.16+ and a C++20 compiler, then build as below.")
        else:
            build = ROOT / "build"
            upper = a.name.upper()
            for step in (["cmake", "-S", ".", "-B", "build", f"-D{upper}_BUILD_TESTS=ON"],
                         ["cmake", "--build", "build", "--parallel"],
                         ["ctest", "--test-dir", "build", "--output-on-failure"]):
                print("+", shlex.join(step), flush=True)
                if subprocess.run(step, cwd=ROOT).returncode:
                    print(f"\nsetup: `{' '.join(step)}` failed in {build}; fix that before anything else.")
                    return 1
            print("\nHost tests pass (build/).")

    steps = [
        "git add -A && git commit -s -m \"Scaffold from driver-template\" (-s: the DCO check needs Signed-off-by)",
        "Work through the checklist above, one subsystem at a time (the datasheet-conformance skill).",
        "Push the branch and open a pull request; CI ends in `CI gate / gate`.",
        "Ask an org owner to run docs/repo-setup.md (metadata, merge settings, rulesets, labels).",
    ]
    print("\nDone. Next steps:")
    for step in steps:
        print(f"  [ ] {step}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
