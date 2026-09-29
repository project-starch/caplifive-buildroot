#!/usr/bin/env python3
"""Refuse monitor assembly in which capstone-c used a capability as its own
offset: `cincoffset(r, x, x)`. That is never meaningful. It appeared when a
long function stored into many arrays and a spilled index was not reloaded:
the array capability was loaded into the index's register and then added to
itself (2026-09-29, create_domain and context_adopt of the context-slot
monitor), and the monitor halted at its first store through the result.

Exit 1 with the sites; exit 2 when the input is not capstone-c output at all
(empty, or no function label), so a check that read nothing never passes."""
import re
import sys

path = sys.argv[1]
text = open(path).read().splitlines()
label, hits, functions = '', [], 0
for n, line in enumerate(text, 1):
    s = line.strip()
    if re.match(r'^\.global\s+\w+', s):
        functions += 1
    if re.match(r'^[A-Za-z][\w.]*:$', s):
        label = s[:-1]
    m = re.search(r'cincoffset\((\w+), (\w+), (\w+)\)', s)
    if m and m.group(2) == m.group(3):
        hits.append(f"{path}:{n}: {label}: {s}")
if not functions:
    print(f"{path}: no function in the file: the check read nothing", file=sys.stderr)
    sys.exit(2)
for h in hits:
    print(h, file=sys.stderr)
if hits:
    print(f"{path}: capstone-c used a capability as its own offset; see scripts/check-monitor-asm.py",
          file=sys.stderr)
sys.exit(1 if hits else 0)
