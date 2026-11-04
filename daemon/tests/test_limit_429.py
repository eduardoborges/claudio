#!/usr/bin/env python3
"""At the usage limit the API answers 429 with the quota headers; the daemons
must still build a payload from them. Run: python -m pytest daemon/tests/test_limit_429.py -q"""
import asyncio
import time

import httpx
import pytest

import daemon.claude_usage_daemon as mac
import daemon.claude_usage_daemon_windows as win

REAL_CLIENT = httpx.AsyncClient


def run_poll(mod, monkeypatch, status, headers):
    def handler(request):
        return httpx.Response(status, headers=headers, json={"type": "error"})

    monkeypatch.setattr(mod.httpx, "AsyncClient",
                        lambda **kw: REAL_CLIENT(transport=httpx.MockTransport(handler), **kw))
    monkeypatch.setattr(mod, "add_chime_field", lambda p: None)
    monkeypatch.setattr(mod, "add_clock_fields", lambda p: None)
    return asyncio.run(mod.poll_api("token"))


@pytest.mark.parametrize("mod", [mac, win])
def test_429_with_quota_headers_is_data(mod, monkeypatch):
    now = time.time()
    payload = run_poll(mod, monkeypatch, 429, {
        "anthropic-ratelimit-unified-5h-utilization": "1.0",
        "anthropic-ratelimit-unified-5h-status": "rejected",
        "anthropic-ratelimit-unified-5h-reset": str(int(now + 3600)),
        "anthropic-ratelimit-unified-7d-utilization": "0.25",
        "anthropic-ratelimit-unified-7d-reset": str(int(now + 86400)),
    })
    assert payload["s"] == 100 and payload["w"] == 25 and payload["st"] == "rejected"
    assert 59 <= payload["sr"] <= 60 and payload["ok"] is True


@pytest.mark.parametrize("mod", [mac, win])
def test_429_without_quota_headers_is_still_a_miss(mod, monkeypatch):
    assert run_poll(mod, monkeypatch, 429, {}) is None


@pytest.mark.parametrize("status", [200, 429])
@pytest.mark.parametrize("mod", [mac, win])
def test_weekly_only_account_has_no_session(mod, monkeypatch, status):
    # A verified Ultra account: no 5h headers, only 7d and the unified status.
    now = time.time()
    payload = run_poll(mod, monkeypatch, status, {
        "anthropic-ratelimit-unified-status": "allowed",
        "anthropic-ratelimit-unified-7d-utilization": "0.06",
        "anthropic-ratelimit-unified-7d-reset": str(int(now + 86400)),
    })
    assert payload["hs"] is False and payload["acct"] == "pro"
    assert (payload["s"], payload["sr"], payload["w"]) == (0, -1, 6)
    assert 1439 <= payload["wr"] <= 1440 and payload["st"] == "allowed"
