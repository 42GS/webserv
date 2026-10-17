#!/usr/bin/env python3
"""Lists the CGI meta-variables and the working directory."""
import os

print("Content-Type: text/plain; charset=utf-8")
print()
for key in sorted(os.environ):
    print("%s=%s" % (key, os.environ[key]))
print("cwd=" + os.getcwd())
