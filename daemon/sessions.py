#!/usr/bin/env python3
"""Open Claude Code sessions for the device's Sessions workspace.

attention-hook.sh leaves one file per session in SESSIONS_DIR holding the
project name, the git branch, the session's title, the last hook event, the
last tool and when the current task started; the file's mtime says when the
last event happened.
No third-party imports: the macOS daemon imports it, and the Linux daemon runs
it as a script that prints the message.
"""
import json
import time
import unicodedata
from pathlib import Path

SESSIONS_DIR = Path.home() / ".config" / "claude-usage-monitor" / "sessions"
ROWS = 6           # firmware's SESSION_ROWS must match
MAX_BYTES = 511    # the firmware's BLE buffer, less the terminator
MAX_AGE = 6 * 3600  # a session that crashed never fires SessionEnd
# Hook event -> state: a = needs you, w = working, d = done (waiting for a prompt).
STATE = {"alert": "a", "clear": "w", "stop": "d"}
CONFIG_FILE = SESSIONS_DIR.parent / "config"
# The corner Clawd acts out the newest working session's last tool, until that
# session goes quiet for ACTIVE_MAX (an interrupted reply never fires Stop).
ACTIVE_MAX = 600
TYPING = {"Bash", "Edit", "Write", "MultiEdit", "NotebookEdit"}
LOOKING = {"Read", "Grep", "Glob", "LS", "WebSearch", "WebFetch"}
# A task that ran at least `done_after` minutes gets a celebration when it
# finishes, announced for DONE_FRESH seconds.
DONE_AFTER = 5
DONE_FRESH = 60
# What a working row says it is doing, from its last tool. Coarse on purpose:
# the daemon resends only when the message changes.
DOING = {"Bash": "Running", "Edit": "Editing", "Write": "Editing",
         "MultiEdit": "Editing", "NotebookEdit": "Editing", "Read": "Reading",
         "Grep": "Searching", "Glob": "Searching", "LS": "Searching",
         "WebSearch": "Browsing", "WebFetch": "Browsing", "Task": "Delegating",
         "Agent": "Delegating", "TodoWrite": "Planning"}


def _ascii(text: str, limit: int) -> str:
    """The device fonts are ASCII only."""
    text = unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode()
    return " ".join(text.split())[:limit]


def _setting(name: str, path: Path) -> str:
    try:
        for line in path.read_text().splitlines():
            key, sep, val = line.split("#", 1)[0].partition("=")
            if sep and key.strip().lower() == name:
                return val.strip().lower()
    except OSError:
        pass
    return ""


def _activity(tool: str) -> str:
    """The splash animation the corner Clawd plays for this tool."""
    if tool in TYPING:
        return "laptop"
    if tool in LOOKING:
        return "magnifier"
    return "cloud"


def _doing(state: str, tool: str, agents: int = 0) -> str:
    """The row's second line: what a working session does, or that it waits for you."""
    if state == "a":
        return "Needs your input"
    if state != "w":
        return ""
    if agents:
        n = f"{agents} agent{'s' if agents > 1 else ''}"
        return n + " working" if agents < 10 else n   # the firmware keeps 16 chars
    if not tool:
        return "Thinking"
    return DOING.get(tool, "Using MCP" if tool.startswith("mcp__") else "Working")


def sessions_message(path: Path = SESSIONS_DIR, now: float | None = None,
                     config: Path = CONFIG_FILE) -> dict:
    """{"k":"ss","v":[["project | title", state, minutes, doing, branch], ...]},
    sessions that need you first, then working, then done. Within each, the
    newest alert, task start or finish comes first. Minutes count from the
    start of the task for a working session, from its finish for a done one (the
    idle prompt rewrites the file later) and from the last event otherwise.
    Doing and the branch share the row's second line; doing is "" for done, the
    branch "" outside git. A session counts as working while its subagents run,
    and the row says how many. Deletes files older than MAX_AGE. Adds "a" (what
    the newest working session is doing), "dn" (when a long task just finished,
    plus "dt" with its row's name and "b" to beep) and "n" (all open sessions,
    when some are left out)."""
    now = time.time() if now is None else now
    done_after = _setting("done_after", config)
    done_after = int(done_after) if done_after.isdigit() else DONE_AFTER
    rows = []
    active = None   # (mtime, tool) of the newest working session
    done = None     # (finish time, row name) of the newest long task that just finished
    for f in path.glob("[!.]*"):
        try:
            mtime = f.stat().st_mtime
            if now - mtime > MAX_AGE:
                f.unlink()
                continue
            d = json.loads(f.read_text())
        except (OSError, ValueError):
            continue
        state = STATE.get(d.get("e")) if isinstance(d, dict) else None
        if not state:
            continue
        # Subagents the hook saw recently. One whose SubagentStop never came
        # (a killed agent) ages out like a quiet session.
        ag = d.get("ag")
        agents = [t for t in ag.values() if isinstance(t, (int, float))
                  and now - t < ACTIVE_MAX] if isinstance(ag, dict) else []
        if agents and state == "d":
            state = "w"   # its reply ended, but background agents still run
        start, fin = d.get("st"), d.get("fin")
        since = (start if state == "w" and isinstance(start, (int, float))
                 else fin if state == "d" and isinstance(fin, (int, float)) else mtime)
        name = _ascii(str(d.get("p") or "?"), 12)
        title = _ascii(str(d.get("ti") or ""), 32)
        if title:
            name = f"{name} | {title}"
            if len(name) > 32:   # about what a row shows
                name = name[:29] + "..."
        # Rows keep their place while sessions work: a working row moves only
        # when a new prompt starts its task, a done row when it finishes.
        order = (since if state == "w" else fin if state == "d"
                 and isinstance(fin, (int, float)) else mtime)
        rows.append((state, order, name, since,
                     _doing(state, str(d.get("t") or ""), len(agents)),
                     _ascii(str(d.get("br") or ""), 24)))
        last = max([mtime, *agents])
        if state == "w" and now - last < ACTIVE_MAX and (not active or last > active[0]):
            active = (last, str(d.get("t") or ""))
        if (state == "d" and done_after and isinstance(start, (int, float))
                and isinstance(fin, (int, float)) and now - fin < DONE_FRESH
                and fin - start >= done_after * 60 and (not done or fin > done[0])):
            done = (fin, name)
    rows.sort(key=lambda r: ("awd".index(r[0]), -r[1]))
    msg = {"k": "ss", "v": [[p, s, int(max(0, now - since) // 60), doing, br]
                            for s, _, p, since, doing, br in rows[:ROWS]]}
    if active:
        msg["a"] = _activity(active[1])
    if done:
        msg["dn"] = int(done[0])
        msg["dt"] = done[1]
        if _setting("beep", config) == "on":
            msg["b"] = 1
    # Long titles can overflow the device's buffer. The last rows go first;
    # they rarely fit on the screen anyway. "n" counts every open session when
    # some are left out, for the device's "+N more".
    while True:
        if len(rows) > len(msg["v"]):
            msg["n"] = len(rows)
        if not msg["v"] or len(json.dumps(msg, separators=(",", ":"))) <= MAX_BYTES:
            return msg
        msg["v"].pop()


if __name__ == "__main__":
    print(json.dumps(sessions_message(), separators=(",", ":")))
