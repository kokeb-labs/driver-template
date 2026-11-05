"""The layers of the library: your application, the hardware-agnostic core, the port contract, your port.

Reads: From top to bottom. Your application calls one object, whose public calls are listed from the umbrella
header in inc/. Each call becomes register transfers through the port contract (the CRTP base in
inc/*_interface.hpp), which forwards them to the port class you write. A library without a port contract has only
the first two layers.
"""

import re
from pathlib import Path

from figkit import Fig

ROOT = Path(__file__).resolve().parents[3]
SETTINGS = next((ROOT / "cmake").glob("*_build_settings.cmake"))
NAME = re.match(r"(\w+)_build_settings", SETTINGS.stem).group(1)
UMBRELLA = f"inc/{NAME}.hpp"
SKIP = {"IsInitialized", "GetConfig", "GetRevision"}


def _interface():
    found = sorted((ROOT / "inc").glob(f"{NAME}_*_interface.hpp"))
    return found[0].relative_to(ROOT).as_posix() if found else None


def data():
    text = (ROOT / UMBRELLA).read_text()
    public = text.split("public:", 1)[1].split("private:", 1)[0]
    calls = []
    for name in re.findall(r"^\s+(?:\[\[nodiscard\]\] )?Result<[^>]*> (\w+)\(", public, re.M):
        if name not in SKIP and name not in calls:
            calls.append(name)
    classes = re.findall(r"^class (\w+)\s*\{", text, re.M)
    iface = _interface()
    d = {"name": NAME, "cls": classes[0] if classes else NAME, "calls": calls, "sources": [f"{UMBRELLA}:public calls"]}
    if iface:
        itext = (ROOT / iface).read_text()
        d["bus"] = re.search(rf"{NAME}_(\w+)_interface", iface).group(1).upper()
        d["base"] = re.search(r"^class (\w+) \{", itext, re.M).group(1)
        d["hooks"] = re.findall(r"\{ port\.(\w+)\(", itext)
        d["sources"].append(f"{iface}:{d['base']}")
    return d


def build(d):
    port = "hooks" in d
    f = Fig(
        "driver-layers",
        "The layers, one of them yours" if port else "The layers",
        alt=(f"Layer diagram: the application calls the hardware-agnostic {d['cls']} core, which uses the CRTP "
             f"{d.get('bus', '')} port contract, which calls the platform port you write, which drives the chip."
             if port else
             f"Layer diagram: the application calls the {d['cls']} library, which has no platform code."),
        width=1200,
        kicker="Architecture",
        subtitle=("The core and the contract are the same on every MCU. The port is a small class you write."
                  if port else "One header-only C++20 library, no platform code."),
        profile="overview",
        legend=[("brand", "this library"), ("teal", "you write"), ("slate", "hardware or caller")],
    )
    W = f.body_w
    app = f.card(0, 0, W, "Your application", ["a handler, a manager or a test"], tone="slate")
    top = app.b + 40
    chips_h = f.chips(16, f.region_inner_top(top), W - 32, [c + "()" for c in d["calls"]], tone="brand")
    reg = f.region(0, top, W, f.region_inner_top(top) - top + chips_h + 18, "Hardware-agnostic core", tone="brand",
                   sub=f"{d['cls']}, {UMBRELLA}")
    f.connect(app, reg, "bottom", "top", tone="rail")
    if not port:
        return f
    comm = f.card(0, reg.b + 40, W, "Port contract",
                  [f"{d['base']}<Derived>: CRTP base, checked at compile time, no virtual calls"],
                  tone="brand", accent=True)
    mine = f.card(0, comm.b + 40, W, "Platform port (your class)", ["  ".join(h + "()" for h in d["hooks"])],
                  tone="teal", accent=True, mono=True)
    hw = f.card(0, mine.b + 40, W, d["cls"], [f"{d['bus']} bus"], tone="slate")
    for a, b in ((reg, comm), (comm, mine), (mine, hw)):
        f.connect(a, b, "bottom", "top", tone="rail")
    return f
