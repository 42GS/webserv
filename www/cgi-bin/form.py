#!/usr/bin/env python3
"""POST: reads an urlencoded body from standard input."""
import html
import os
import sys
from urllib.parse import parse_qs

length = int(os.environ.get("CONTENT_LENGTH", "0") or "0")
body = sys.stdin.read(length)
fields = parse_qs(body)
print("Content-Type: text/html; charset=utf-8")
print()
print("<!DOCTYPE html><html><body><h1>Form received</h1><ul>")
for key in sorted(fields):
    print("<li>%s = %s</li>" % (html.escape(key), html.escape(fields[key][0])))
print("</ul><p>%d bytes of body.</p>" % length)
print('<p><a href="/form.html">Again</a> | <a href="/">Home</a></p></body></html>')
