#!/usr/bin/env python3
"""Opens data.txt by a relative path: works only if run in this directory."""
with open("data.txt") as data:
    text = data.read()
print("Content-Type: text/plain; charset=utf-8")
print()
print(text, end="")
