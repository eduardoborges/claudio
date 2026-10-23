#!/usr/bin/env python3
"""Sessions workspace message. Run: python -m pytest daemon/tests/test_sessions.py -q"""
import json
import os

from daemon.sessions import ACTIVE_MAX, DONE_FRESH, MAX_AGE, MAX_BYTES, ROWS, sessions_message


def write(d, sid, project, event, mtime):
    f = d / sid
    f.write_text(json.dumps({"p": project, "e": event}))
    os.utime(f, (mtime, mtime))
    return f


def test_states_order_and_age(tmp_path):
    now = 1_000_000.0
    write(tmp_path, "a", "done-old", "stop", now - 3600)
    write(tmp_path, "b", "Clawdmeter", "clear", now - 120)
    write(tmp_path, "c", "projeto ação", "alert", now - 30)
    write(tmp_path, "d", "done-new", "stop", now - 60)
    (tmp_path / ".d.123").write_text("{}")                       # hook's temp file
    write(tmp_path, "e", "x", "bogus", now)                      # unknown event
    assert sessions_message(tmp_path, now)["v"] == [
        ["projeto acao", "a", 0, "Needs your input", ""],
        ["Clawdmeter", "w", 2, "Thinking", ""],
        ["done-new", "d", 1, "", ""],
        ["done-old", "d", 60, "", ""],
    ]


def test_working_row_says_what_and_since_when(tmp_path):
    now = 1_000_000.0
    write_full(tmp_path, "a", now - 5, e="clear", t="Edit", st=now - 900)
    write_full(tmp_path, "b", now - 60, e="clear", t="mcp__figma__get_node")
    write_full(tmp_path, "c", now - 120, e="clear", t="Skill")
    write_full(tmp_path, "d", now - 180, e="stop", t="Bash", st=now - 999)
    rows = sessions_message(tmp_path, now, tmp_path / "config")["v"]
    assert [r[2:4] for r in rows] == [[1, "Using MCP"], [2, "Working"],
                                      [15, "Editing"], [3, ""]]


def test_prunes_stale_caps_rows_and_names(tmp_path):
    now = 1_000_000.0
    stale = write(tmp_path, "old", "old", "clear", now - MAX_AGE - 1)
    for i in range(ROWS + 2):
        write(tmp_path, f"s{i}", "a-very-long-project-name", "clear", now - i)
    rows = sessions_message(tmp_path, now)["v"]
    assert not stale.exists()
    assert len(rows) == ROWS
    assert rows[0][0] == "a-very-long-"


def test_empty(tmp_path):
    assert sessions_message(tmp_path / "missing", 0) == {"k": "ss", "v": []}


def write_full(d, sid, mtime, **fields):
    f = d / sid
    f.write_text(json.dumps({"p": "p", **fields}))
    os.utime(f, (mtime, mtime))


def test_activity_follows_newest_working_session(tmp_path):
    now = 1_000_000.0
    cfg = tmp_path / "config"
    sessions = tmp_path / "s"
    sessions.mkdir()
    write_full(sessions, "a", now - 30, e="clear", t="Grep")
    write_full(sessions, "b", now - 5, e="clear", t="Edit")
    write_full(sessions, "c", now - 1, e="stop", t="Bash")
    assert sessions_message(sessions, now, cfg)["a"] == "laptop"
    write_full(sessions, "b", now - ACTIVE_MAX, e="clear", t="Edit")
    assert sessions_message(sessions, now, cfg)["a"] == "magnifier"
    write_full(sessions, "a", now - 2, e="clear", t="")
    assert sessions_message(sessions, now, cfg)["a"] == "cloud"


def test_quiet_working_session_stops_animating(tmp_path):
    now = 1_000_000.0
    write_full(tmp_path, "a", now - ACTIVE_MAX, e="clear", t="Bash")
    assert "a" not in sessions_message(tmp_path, now, tmp_path / "config")


def test_long_task_finishing_is_announced_once(tmp_path):
    now = 1_000_000.0
    cfg = tmp_path / "config"
    sessions = tmp_path / "s"
    sessions.mkdir()
    write_full(sessions, "long", now, e="stop", ti="Fix the touch axis", st=now - 400, fin=now - 10)
    write_full(sessions, "short", now, e="stop", st=now - 100, fin=now - 5)
    msg = sessions_message(sessions, now, cfg)
    assert msg["dn"] == int(now - 10) and "b" not in msg
    assert msg["dt"] == "p | Fix the touch axis"
    cfg.write_text("beep = on\n")
    assert sessions_message(sessions, now, cfg)["b"] == 1
    assert "dn" not in sessions_message(sessions, now + DONE_FRESH, cfg)
    cfg.write_text("done_after = 10\n")
    assert "dn" not in sessions_message(sessions, now, cfg)
    cfg.write_text("done_after = 0\n")
    assert "dn" not in sessions_message(sessions, now, cfg)


def test_subagents_keep_the_row_working(tmp_path):
    now = 1_000_000.0
    write_full(tmp_path, "a", now - 300, e="stop", t="Agent", st=now - 600,
               ag={"x": now, "y": now - 20})
    write_full(tmp_path, "b", now - 5, e="clear", t="Read", ag={"z": now - 1})
    write_full(tmp_path, "c", now - 5, e="alert", ag={"z": now - 1})
    msg = sessions_message(tmp_path, now, tmp_path / "config")
    assert [r[1:4] for r in msg["v"]] == [["a", 0, "Needs your input"],
                                         ["w", 0, "1 agent working"],
                                         ["w", 10, "2 agents working"]]
    assert msg["a"] == "cloud"   # a parent waiting on agents is still active


def test_stale_subagents_are_ignored(tmp_path):
    now = 1_000_000.0
    write_full(tmp_path, "a", now - 300, e="stop", ag={"x": now - ACTIVE_MAX - 1})
    write_full(tmp_path, "b", now - 5, e="clear", t="Read", ag={})
    assert [r[1:4] for r in sessions_message(tmp_path, now, tmp_path / "config")["v"]] == [
        ["w", 0, "Reading"], ["d", 5, ""]]


def test_branch_rides_along(tmp_path):
    now = 1_000_000.0
    write_full(tmp_path, "a", now - 5, e="clear", br="main")
    write_full(tmp_path, "b", now - 60, e="stop", br="feat/a-branch-name-that-goes-on")
    assert [r[4] for r in sessions_message(tmp_path, now, tmp_path / "config")["v"]] == [
        "main", "feat/a-branch-name-that-"]


def test_title_follows_the_project(tmp_path):
    now = 1_000_000.0
    write_full(tmp_path, "a", now - 5, e="clear", ti="Sessões com subagents")
    write_full(tmp_path, "b", now - 60, e="stop", ti="x" * 60)
    assert [r[0] for r in sessions_message(tmp_path, now, tmp_path / "config")["v"]] == [
        "p | Sessoes com subagents", "p | " + "x" * 25 + "..."]


def test_message_fits_the_device_buffer(tmp_path):
    now = 1_000_000.0
    for i in range(ROWS):
        write_full(tmp_path, f"s{i}", now - i, p="a-very-long-project", e="alert",
                   ti="t" * 60, br="b" * 60)
    msg = sessions_message(tmp_path, now, tmp_path / "config")
    assert 0 < len(msg["v"]) < ROWS and msg["n"] == ROWS
    assert len(json.dumps(msg, separators=(",", ":"))) <= MAX_BYTES


def test_rows_keep_their_place_while_working(tmp_path):
    now = 1_000_000.0
    write_full(tmp_path, "old", now - 1, p="old", e="clear", st=now - 600)
    write_full(tmp_path, "new", now - 30, p="new", e="clear", st=now - 60)
    write_full(tmp_path, "d1", now - 5, p="d1", e="stop", fin=now - 300)
    write_full(tmp_path, "d2", now - 200, p="d2", e="stop", fin=now - 200)
    msg = sessions_message(tmp_path, now, tmp_path / "config")
    assert [r[0] for r in msg["v"]] == ["new", "old", "d2", "d1"] and "n" not in msg
    assert [r[2] for r in msg["v"]] == [1, 10, 3, 5]
    for i in range(ROWS):
        write_full(tmp_path, f"s{i}", now, e="stop")
    assert sessions_message(tmp_path, now, tmp_path / "config")["n"] == ROWS + 4
