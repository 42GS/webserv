#!/usr/bin/env python3
"""A cookie session (bonus): counts the visits of one browser.

Only a random identifier goes into the cookie; the count stays on the
server. An identifier the server does not know is replaced by a new one,
so a client cannot pick its own (session fixation). "?logout" ends the
session and expires the cookie with a fixed past date.
"""
import html
import json
import os
import re
import secrets
import tempfile

STORE = os.path.join(tempfile.gettempdir(), "webserv-sessions")
COOKIE = "webserv_session"
EXPIRED = "Thu, 01 Jan 1970 00:00:00 GMT"


def cookies():
    jar = {}
    for part in os.environ.get("HTTP_COOKIE", "").split(";"):
        name, _, value = part.strip().partition("=")
        if name:
            jar[name] = value
    return jar


def session_path(session_id):
    return os.path.join(STORE, session_id + ".json")


os.makedirs(STORE, exist_ok=True)
session_id = cookies().get(COOKIE, "")
known = (re.fullmatch(r"[0-9a-f]{32}", session_id) is not None
         and os.path.exists(session_path(session_id)))
headers = ["Content-Type: text/html; charset=utf-8"]

if os.environ.get("QUERY_STRING") == "logout":
    if known:
        os.remove(session_path(session_id))
    headers.append("Set-Cookie: %s=; Path=/; Expires=%s" % (COOKIE, EXPIRED))
    message = "Session ended."
else:
    if known:
        with open(session_path(session_id)) as stored:
            state = json.load(stored)
    else:
        session_id = secrets.token_hex(16)
        state = {"visits": 0}
        headers.append("Set-Cookie: %s=%s; Path=/; HttpOnly"
                       % (COOKIE, session_id))
    state["visits"] += 1
    with open(session_path(session_id), "w") as stored:
        json.dump(state, stored)
    message = "Visit number %d in this session." % state["visits"]

print("\n".join(headers))
print()
print("<!DOCTYPE html><html><body><h1>Session</h1>")
print("<p>%s</p>" % html.escape(message))
print('<p><a href="/cgi-bin/session.py">Reload</a> | '
      '<a href="/cgi-bin/session.py?logout">Log out</a> | '
      '<a href="/">Home</a></p></body></html>')
