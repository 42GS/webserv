#!/usr/bin/env python3
"""Sends the request body back (used with a small body limit)."""
import os
import sys

length = int(os.environ.get("CONTENT_LENGTH", "0") or "0")
body = sys.stdin.buffer.read(length)
sys.stdout.buffer.write(b"Content-Type: text/plain\r\n\r\n")
sys.stdout.buffer.write(b"received %d bytes: " % length + body + b"\n")
