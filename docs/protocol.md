# BLE protocol

The display talks to the Mac app and the daemons over BLE. It exposes a data service next to the standard HID keyboard:

| | UUID |
| --- | --- |
| Data service | `4c41555a-4465-7669-6365-000000000001` |
| RX, write | `4c41555a-4465-7669-6365-000000000002` |
| TX, notify | `4c41555a-4465-7669-6365-000000000003` |
| HID service | `00001812-0000-1000-8000-00805f9b34fb` |

Each write to RX is one JSON message of at most 511 bytes. A message without `k` is a usage reading and replaces the last one:

```json
{ "s": 45, "sr": 120, "w": 28, "wr": 7200, "st": "allowed", "ok": true }
```

`s` and `w` are the session and weekly percentages, `sr` and `wr` the minutes until each resets, `st` the status and `ok` whether the request worked. A reading can also carry an alert: `m` is the message (ASCII), `b` beeps once and `x` ends the alert.

Messages with `k` feed the other workspaces:

```json
{ "k": "ss", "v": [["Clawdmeter | Fix the touch axis", "a", 2, "Needs your input", "main"]] }
{ "k": "td", "h": "000000000356874565431000", "v": [["Prompts", "48"], ["Tokens", "184.2m"]] }
{ "k": "hm", "n": 180, "h": "AAAFHFK...", "mo": [["May", 0], ["Jun", 4]] }
{ "k": "st", "v": [["Sessions", "412"], ["Total tokens", "21.7b"]] }
```

| Kind | Fields |
| --- | --- |
| `ss` | Sessions. Each row is `[project \| title, state, minutes, doing, branch]`, with state `a` (needs you), `w` (working) or `d` (done). `n` counts every open session when some rows were left out to fit the 511 bytes. `a` names the corner Clawd's animation (`laptop`, `magnifier` or `cloud`). `dn` is the time a long task finished, which the device celebrates once, with a beep when `b` is set. `dt` is that task's row name, shown under Clawd while he dances |
| `td` | Today. `h` is messages per hour as 24 digits from 0 to 8, relative to the busiest hour, and `v` six label and value pairs |
| `hm` | The heatmap. Week columns from the oldest Sunday, `n` valid days, `h` levels 0 to 4 packed two days per character (`'A' + 5 * first + second`), `mo` month labels by column |
| `st` | Stats. Up to six label and value pairs, already formatted |

The firmware announces `"stats":1` in its TX value, and the daemons skip the `k` messages when it's missing.
