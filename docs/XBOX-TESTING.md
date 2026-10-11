# Real-Xbox observation and regression tests

Controller input is deliberately out of scope until the cable and first real
input test are available. Use the physical controller for the steps below.

## Capture evidence

The existing firmware already provides read-only video status and a JPEG frame
endpoint. `tools/observe_xbox.py` wraps those APIs into timestamped JSONL for
humans and agents; it does not change the REST API, settings or controller state.

```sh
python3 tools/observe_xbox.py http://esp.local \
  --build-id YOUR_HOMEBREW_COMMIT --scenario 720p-game \
  --seconds 120 --output observations/my-build-720p
```

Use a new output directory each time. `--build-id` identifies the Xbox program,
not the KVM firmware; use an explicit unknown/reference label when unavailable.
Record the KVM firmware commit separately with your test notes. The output
directory is private and ignored by Git. Review screenshots before sharing.

Add `--screenshot` to request one JPEG at the start. That briefly counts as a
video consumer and may reduce local LCD FPS. Do not use it when measuring
uninterrupted LCD performance. The endpoint requires the existing agent API
permission or supported authentication; this helper targets our no-login local
profile and does not bypass a disabled API or accept credentials. A 403/503 or
malformed response is recorded as a failure, not an empty successful screenshot.
No microSD is needed for the direct frame endpoint.

`events.jsonl` contains UTC and monotonic elapsed times, selected video fields,
observed signal/mode/viewer transitions, screenshot hashes, and request failures.
Signal lock is **not proof of fresh capture**: the current status API lacks a
capture timestamp, so `capture_freshness` is explicitly `unknown` when locked.
Encoded FPS can be zero on a static menu. Screenshot success is evidence of a
frame endpoint response, not proof the game is running correctly. Sampling can
miss short transitions; timestamps are observation times, not exact event times.

The LCD status and existing system log provide additional CSI freshness and
LCD-submission diagnostics. A future structured capture-freshness API requires
coordinated API/client documentation changes; it is not silently invented here.

## Manual regression matrix

Record actual results and duration; do not assume pass from a responding API.

| Step | Human action | Evidence / pass condition |
| --- | --- | --- |
| Boot | Power on Xbox normally | HDMI lock and first picture return; note elapsed time |
| Dashboard | Leave dashboard displayed for 2 minutes | Static image is not reported as a game failure |
| 480p | Play 480p game for 10 minutes | Correct colors, no splits/blackouts; LCD about 30 updates/s with no viewers |
| 720p | Switch and play for 10 minutes | Automatic mode switch; smooth LCD around 28-29 updates/s |
| Touch | Tap twice, then hold | Stats hide/show; hold does not repeat; geometry unchanged |
| Reset | Use normal in-game reset | Mode/signal events match action and picture recovers without ESP reset |
| HDMI loss | Unplug only HDMI, then reconnect | NO SIGNAL slate, then live image; never disturb powered ribbons |
| Remote | Open browser stream, then close | Remote-priority LCD limit activates, then local rate returns |
| Network | Test local Wi-Fi and Tailscale separately | Record delivery gaps, reconnects and memory headroom; no assumptions from LAN success |

Existing `tools/soak_mjpeg.py` measures stream delivery but creates a viewer:
its LCD slowdown is expected. Do not compare its results to LCD-only tests.
Remote reset/input automation and a controller API are not implemented by this
workflow. Keep the last known-good firmware until all relevant hardware checks
pass; build success alone does not establish runtime stability.
