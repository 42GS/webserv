#!/usr/bin/env python3
"""GET: greets the "name" of the query string."""
import html
import os
from urllib.parse import parse_qs

query = parse_qs(os.environ.get("QUERY_STRING", ""))
name = query.get("name", ["world"])[0]
print("Content-Type: text/html; charset=utf-8")
print()
print("<!DOCTYPE html><html><body>")
print("<h1>Hello, %s!</h1>" % html.escape(name))
print("<p>Method: %s</p>" % html.escape(os.environ.get("REQUEST_METHOD", "")))
print('<p><a href="/">Home</a></p></body></html>')
