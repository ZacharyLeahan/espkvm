# Upstream review ledger

This is a specialized original-Xbox fork, not a mirror. We review upstream
for useful fixes, adapt what fits, and deliberately leave other changes out.
GitHub's behind count is ancestry, not our maintenance checklist.

## Workflow

1. Fetch upstream without merging. Inventory commits since the last inventory
   tip below; also revisit pending rows. A fetched commit is not an adopted fix.
2. Inspect the diff and dependencies, including submodule commits. Split mixed
   commits into separate rows when we take only part of them.
3. Record a decision and rationale: **Pending**, **Adapt planned**, **Adapted**,
   **Adopted** (substantially unchanged), **Ignored**, or **Deferred** (with a
   reason/condition for revisiting). Recommendations are not final decisions.
4. Implement selected changes in focused commits on a test branch. Preserve
   attribution/licenses. Use `git cherry-pick -x` for appropriate whole commits;
   verify the source reference remains in the message after conflict resolution.
   For adaptations, add a full source URL as a custom commit trailer:

   ```text
   Adapted-from: https://github.com/espkvm/espkvm/commit/<full-source-sha>
   ```

   Explain what we kept, omitted, and changed. Multiple source trailers are OK.
   These are provenance links, not automatic synchronization instructions.
5. After committing, update the row with the full URL of **our implementation
   commit** and test results in a separate ledger commit. This avoids trying to
   embed a commit's own hash inside itself. Mark partial adaptations explicitly;
   never imply an entire upstream fix was adopted when only part was taken.
6. Revisit later fixes to adapted code, especially security/networking changes.
   An old adaptation does not automatically include a newer upstream fix.

For hardware-affecting changes, verify build/tests plus the relevant live
behavior (480p/720p switching, LCD/touch, browser streaming, Wi-Fi/Tailscale).
Record failures and untested areas. Do not flash, push, merge, or change a
submodule just to update this ledger; those are separate implementation steps.

## Inventory checkpoint

- Inventory date: **2026-10-16** (America/New_York).
- Fork reviewed: [`8e7e611`](https://github.com/ZacharyLeahan/espkvm-xbox/commit/8e7e611741a13e05bb2dc451a1aaa8e614878ce3), including the tested HDMI audio recorder.
- Last inventoried upstream tip: [`4c62662`](https://github.com/espkvm/espkvm/commit/4c626628b8f7fdf225edc168ca6a1d3d423c2fa5), one documentation commit after v0.65.0. Its author date is October 17 in UTC+3, October 16 in UTC and New York.
- Previous checkpoint: [`ce519cc`](https://github.com/espkvm/espkvm/commit/ce519ccb63cca1fb9f0ee8de6a670278838d5bb4) (v0.58.0), inventoried on 2026-10-03. **All 17 subsequent firmware commits are listed below**, alongside the original 13 and the relevant console/Microlink dependencies.
- Current ancestry merge base: [`5d18b5b`](https://github.com/espkvm/espkvm/commit/5d18b5b08037c970d83e7947fce9dc9be349bc3f); the original inventory started at [`77045e1`](https://github.com/espkvm/espkvm/commit/77045e1ccc336151ec7ef15049812523ec921c75). At this checkpoint there are 96 fork-only and 29 upstream-only commits. These counts include independently adapted work and are not a patch checklist.
- Upstream console: [`48c58b0`](https://github.com/espkvm/console/commit/48c58b037ac90504db677e83f03fa731b41bb664); our pin remains [`60c39f0`](https://github.com/ZacharyLeahan/espkvm-console-xbox/commit/60c39f0c7d5ae5c46ca4bbb896059a9b997a6435).
- Upstream Microlink `idf6`: [`5129dc2`](https://github.com/espkvm/microlink/commit/5129dc2735419ec54b74a95d48193ff862f29338); our pin remains [`d6865e1`](https://github.com/ZacharyLeahan/microlink/commit/d6865e1c7ebd5ad8f158029385e93dcb36d220b0).
- This reconciliation fetched history and inspected source/dependency diffs. New **Pending** recommendations are not approval to integrate. No firmware, console bundle, dependency pin, device setting or installed image was changed; no new hardware/performance validation is claimed.
- The inventory tip marks what we listed, not what we adopted or fully reviewed.

## Current implementation and acceptance status (2026-10-17)

The last upstream fetch (2026-10-16) checked firmware `upstream/main`, console `upstream/main` and Microlink
`upstream/idf6` again. Their tips remain `4c62662`, `48c58b0` and `5129dc2`
respectively: **no new upstream commits since the inventory checkpoint**.
Implementation and test evidence now cover firmware through `5ff34b2`, console through `a258024` and
Microlink through `d3210a0`. The checkpoint above describes the original
baseline; the table below is the current implementation status. Historical
inventory decisions farther down retain the original rationale and source URLs.

| Area | Current implementation | Remaining acceptance work |
| --- | --- | --- |
| HDMI bridge recovery | Adapted in [8d94230](https://github.com/ZacharyLeahan/espkvm-xbox/commit/8d942304e8631b0ca37bf34e76d7e7e5fab61979): bounded I2C, failed reads and capped boot recovery, preserving HDMI audio PLL corrections. | Host failure tests/build passed previously; physical reconnects and oversized-to-supported mode changes with SD audio remain unverified. |
| Settings and build paths | Console [9aea143](https://github.com/ZacharyLeahan/espkvm-console-xbox/commit/9aea143beee957604e4fbf4aac7b5da28588ed07); firmware [daffe2c](https://github.com/ZacharyLeahan/espkvm-xbox/commit/daffe2c2f4872db3b61aa3be9a4163f4741d0541). Safe drafts, grouping, initial section, card wording and explicit component paths are implemented. | Console tests/typecheck and source-copy build passed previously; first-open selection and grouping were checked in the browser; delayed-save interaction remains. Parent console pin is published. |
| Video delivery and recovery | SPS [1ba5d84](https://github.com/ZacharyLeahan/espkvm-xbox/commit/1ba5d84a6e3b534258109b1a3ef97baa7ca9f57b), ACK/keyframe handling [9dcc15d](https://github.com/ZacharyLeahan/espkvm-xbox/commit/9dcc15de36ce15d1243904a9046294d13a2e1efd), session transport [b63b60e](https://github.com/ZacharyLeahan/espkvm-xbox/commit/b63b60e41b92a3cafd1f4f47cb40637b93b2e55e), console [a258024](https://github.com/ZacharyLeahan/espkvm-console-xbox/commit/a258024c2ea4300ad5f0d1d3fe9da526191e1cac). | Host checks and both encode-path builds passed. Four post-cache-fix one-minute clips and a ten-minute combined video/PCM/SD run decoded cleanly. Live video remains limited and network PCM has measured transport gaps; see the live PCM diagnosis and extended tests below. Video-only recording and host AAC-preserving MP4 conversion passed. Authenticated HTTPS/WebCodecs, slow-viewer recovery, longer endurance and physical A/V delay remain. The earlier intermittent corruption is not deterministically attributed; see the cache correction below. |
| Microlink reliability | Six source-attributed cherry-picks plus reconnect/path adaptation [d3210a0](https://github.com/ZacharyLeahan/microlink/commit/d3210a0523766d6917706aa643750082f61d49f8); exact sources and boundaries are recorded below. | Host checks/build passed previously; direct discovery and tailnet HTTP passed; relay, peer reboot, clock sync and repeated TLS failures remain. Parent dependency pin is published. |
| Memory and diagnostics | Upload fallback/lazy WireGuard [9a44661](https://github.com/ZacharyLeahan/espkvm-xbox/commit/9a44661e3406935de431ebf836695fe511b87d67), freshness [f723d14](https://github.com/ZacharyLeahan/espkvm-xbox/commit/f723d144f4fa5248a60695478f827b3d3242a531), heap/task endpoint [4ccb502](https://github.com/ZacharyLeahan/espkvm-xbox/commit/4ccb50280c8f92556d310efcd42825faa3463e04). | Host checks/build passed previously; live upload checks and a ten-minute combined-load memory sample passed without progressive loss; worst-case allocation behavior remains unverified. API skill is published; upstream MCP remains an explicitly recorded access gap. |

**All five areas are implemented.** Their integration is tracked in
[PR #3](https://github.com/ZacharyLeahan/espkvm-xbox/pull/3); it merged into `main` on 2026-10-17 as `45eaf61`. This is implementation acceptance, not a claim that every
hardware scenario is verified. Parent pins reference console `a258024` and
Microlink `d3210a0`; the generated bundle is committed, and the API skill branch
is published at `60949e4`. The merged-main matrix passed all 26 test/build jobs; local Xbox, Ethernet
and revision-3 builds also passed.

### Next tests that close acceptance gaps

| Priority | Test | Evidence needed | Participation |
| --- | --- | --- | --- |
| 1 | Longer combined recording/streaming run, repeated starts/stops and reconnects | Strict H.264/AAC decode of every saved clip; audio activity, continuity, frame delivery and stable heap/task margins; retain any failures. | Agent can run while a continuous source plays. |
| 2 | Authenticated HTTPS video/audio, logout and slow viewer | Trusted TLS, WebCodecs recovery, bounded audio queues and prompt session revocation without disrupting the SD recorder. | User signs in locally; do not send passwords in chat. |
| 3 | HDMI unplug/replug and supported/unsupported mode transitions | Recovery of video and I2S clocks without reboot; valid subsequent recordings and input/LCD operation. | User handles cables/source modes while agent observes. |
| 4 | Tailscale direct/relay and reboot recovery | Confirm actual selected path, reconnect after peer/device reboot, repeated failures without heap loss. | A remote peer/network is needed to establish relay behavior. |
| 5 | Delayed settings save and memory pressure | Edits made during a delayed save survive; upload fallback succeeds with constrained allocation; no increasing memory loss under repeated load. | Agent can test controlled delays/fault injection; do not claim host tests prove worst-case hardware behavior. |

These tests validate the selected upstream adaptations already in our fork.
They do not require importing every upstream commit or eliminating GitHub's
behind count. A fresh upstream inventory is a separate read-only reconciliation.

### What is still worth adapting

Finish and validate the five implemented areas before adding another broad
batch. The remaining useful, previously inventoried candidates are:

- **Authenticated live log tail** (`5180eaa`): useful for intermittent HDMI/audio
  faults; first review bounded allocation, concurrency and private log content.
- **Console cache refresh/recovery notice** (firmware `157a797`, console
  `06e59ba`): helps avoid stale UI after an update; test offline fallback and
  keep audio/API responses out of the cache.
- **Return to sign-in on HTTP 401** (console `73ea97e`): review alongside our
  audio cancellation/logout behavior before adapting.
- **Capture-to-send timing diagnostics** (`e14b7ca`): useful after propagating
  timestamps per encoding job; frame freshness is already implemented, but
  latency instrumentation and physical A/V synchronization are not established.

The 480p FIFO/EDID subset remains conditional: our RGB path does not demonstrate
that upstream's UYVY threshold change would fix this installation. Keep adaptive
bitrate, broad stack cuts, persisted-session policy, ACME, generic gamepad
replacement and larger UDP mailbox settings deferred for the reasons below.
No newly discovered upstream change requires immediate adoption.

This reconciliation changes only the ledger and preserves unrelated roadmap,
bundle and dependency working-tree changes. Historical tests below were not
rerun for this documentation update.

## TCP queue and path correlation (2026-10-17)

Instrumentation and evidence: [1d44bd3](https://github.com/ZacharyLeahan/espkvm-xbox/commit/1d44bd3).

The default-off `CONFIG_KVM_HTTP_TX_DIAGNOSTICS` option samples stalled plain
HTTP PCM sends. It records free TCP send bytes, queued/unacknowledged/unsent
segments, advertised window, last acknowledged/next sequence, congestion window,
retransmit state, explicit core-lock acquisition time, retry count, longest
socket call and scheduler yield. PCB traversal uses the lwIP core lock and the
full IPv4 tuple, including IPv4-mapped IPv6 socket addresses. Native IPv6 and
TLS are not covered. It retains no PCB pointer across sampling calls.
No media, queue, timeout, endpoint or authentication policy changes in this step.

Measured on Function EV / C790, 48 kHz stereo PCM, Wi-Fi, without a video viewer
or SD recording during these isolated runs:

- The first 150-second diagnostic trial delivered 149.165 seconds. Its initial
  IPv4-only socket probe returned `found=0` on the dual-stack server, so its TCP
  state fields are invalid evidence. The sampler was corrected and retested.
- The corrected 150-second run delivered 149.070 seconds in 150.005 seconds;
  confirmed producer gaps were 43,920 frames (915 ms), with maximum successful
  send 170.447 ms. Representative stall snapshots showed **zero free send
  bytes, 8,192 bytes queued, 9-12 unacknowledged segments, no unsent segments,
  and a 65,535-byte peer window**. The 169.060 ms write retried 17 times,
  with maximum socket-call duration 432 us and yield 9.950 ms; explicit core
  lock acquisition took 2 us. Its next snapshot showed ACK progress and free
  space, allowing completion. This rules against a single long socket call or
  gross sender-task starvation in that captured stall, not every possible stall.
- The corresponding receiver trace showed a 171.914 ms data-arrival gap.
  The Mac emitted the ACK for the preceding packet after 67 us; the ESP's
  snapshot still considered its send buffer unacknowledged. Receiver capture
  timestamps are host-stack timestamps, not proof of over-the-air delivery.
  Two stall clusters occurred about 60 seconds apart. There were 29,856 data
  packets, no repeated/out-of-order sequence ranges in this receiver capture,
  and no kernel capture drops. This does not rule out loss on the reverse path.
- A further 150-second run added five pings/second to the gateway and ESP.
  PCM delivered 149.270 seconds in 150.005 seconds, with 33,360 confirmed gap
  frames (695 ms), maximum successful send 216.032 ms, and maximum packet
  arrival gap 259.168 ms. Across 736 timed replies per destination, gateway
  RTT peaked at 28.087 ms while ESP RTT peaked at 270.939 ms during the audio
  stall cluster. Pings did not eliminate the stalls. This points away from a
  general Mac-to-router outage and toward the AP-to-ESP/C6/SDIO receive path;
  it does not identify which part is responsible. Probe traffic can perturb
  timing, so this is not a controlled performance improvement claim.

Both corrected audio observers completed their requested durations; final send
failures occurred at client shutdown and are not counted as premature failures.
No recorder regression test or successful endurance acceptance is claimed for
these diagnostic runs. Existing TX/pacing host tests passed. Enabled and disabled
app builds were checked; full rebuilds expose existing unused-variable/function
warnings in Microlink, with no new diagnostic-code warnings.

Evidence stays local in `/tmp/espkvm-txprobe-01`, `/tmp/espkvm-txprobe-02`, and
`/tmp/espkvm-txprobe-path`. No raw media or packet summaries are published.
For reproduction, enable the diagnostic option, run `tools/observe_audio.py`
for 150 seconds with a new local WAV path, and poll `/api/v1/system/log` every
2 seconds, retaining only `tx_probe`, `audio_stream` and `http_tx` entries with
host timestamps. The ring is small; an end-only log fetch misses evidence.
Correlate sequence numbers with a local device-filtered TCP header capture.
Use trusted TLS/cookies where configured; do not disable authentication for tests.
Turn diagnostics off again for normal use.

**Decision:** keep PR #7 draft. Do not increase queue/deadline defaults based on
this evidence. The next measurement should compare C6/SDIO received-packet and
stack-delivery counters during a stall, and confirm the radio power-save state.
ESP-Hosted exposes optional host packet counters, but they were not enabled or
validated here. Its receive-loop review did not establish a matching periodic
sleep or a proven code defect. A radio/receive-path fix needs that next boundary
measurement or an alternate-network comparison before another combined
video/PCM/microSD acceptance run is meaningful.

## Nonblocking HTTP deadline experiment (2026-10-17)

Implementation and evidence: [456d090](https://github.com/ZacharyLeahan/espkvm-xbox/commit/456d090).

This follow-up supersedes the blocking-write policy in the historical section
below. The local ESP-IDF lwIP dependency at `c6f2f878e7b0f86033214b85547d579be43351e3`
checks blocked netconn write timeouts from ACK/poll callbacks
(`src/api/api_msg.c`, `lwip_netconn_do_writemore`). Its TCP poll interval is two
500 ms slow ticks. A 100 ms SO_SNDTIMEO therefore does not guarantee a 100 ms
return when the send queue stalls. This explains a possible timeout overshoot;
it does not establish the underlying cause of network backpressure.

The fork wrapper now uses MSG_DONTWAIT, retries partial writes and transient
EINTR/EAGAIN with a one-tick yield, and closes an incomplete stream at its socket
budget (capped at five seconds). PCM uses 250 ms, preserving the 40 ms browser
producer queue, TCP_NODELAY and recorder path. TLS retains its existing loop,
with the changed PCM socket budget. Lock acquisition/scheduling are not hard
real-time bounded. No REST shape, authentication or media format changed.

- At 100 ms, the 120-second combined test still lost PCM near 87 seconds, but
  the failed send took 110.360 ms instead of the previous 1.158 seconds.
  Confirmed live gaps were 420 ms. Video completed 1,111 frames/110 seconds;
  the 30,331,920-byte microSD file strictly decoded H.264 and stereo 48 kHz AAC,
  with zero recorder queue drops and 489 inserted silence frames. Overall
  acceptance failed because network audio ended early.
- At 250 ms, the 180-second combined check passed. Network audio delivered
  179.210 seconds in 180.002 seconds, with sound detected. Video completed
  1,600 frames/170 seconds (9.4 fps, 1,125 kbit/s, 96 keyframes, ACKs enabled).
  The final timed-out send (259.649 ms) occurred at client shutdown; the client
  completed its full duration. Maximum successful send was 118.966 ms.
  Capture overruns and recorder queue drops were zero. The microSD file
  `VIDEO/up-000031-3.ts` strictly decoded both tracks: 179.968 seconds of AAC,
  93 silence frames (1.94 ms), SHA-256
  `b0fb5e21b06921a472be804b6044905e798fd27029b9811efcbe430af626de50`.
  This establishes short-run connection survival, not gap-free audio or
  measured physical A/V synchronization.

- The attempted 600-second repeat failed early: PCM sent 1,229,040 frames
  (25.605 seconds), then timed out with 512/960 bytes written. Confirmed gaps
  were 17,520 frames (365 ms), maximum successful send 170.995 ms, failed send
  260.348 ms, and capture epochs/overruns zero. Thus the three-minute pass was
  not repeatable acceptance. The owned recording was stopped at about 108
  seconds and the video probe terminated; neither is a ten-minute pass.
  Recorder counters showed 1,417 video frames, zero video/audio queue drops
  and 532 inserted silence frames. The saved 108.053-second H.264/AAC file
  strictly decoded, SHA-256
  `fb60deabf0a129079644def625610a7097f1de7d1ad80ba1f3e1c127a1dfab8d`.
  Settings restored successfully. Evidence: `/tmp/espkvm-nonblocking-250-long`.

A subsequent 120-second audio-only TCP trace completed with 119.430 seconds
received, 550 ms device-reported gaps and a 220.884 ms maximum successful send.
The receiver trace contained 23,913 data packets, no repeated/out-of-order
sequence ranges and a minimum raw advertised receive window of 65,535.
At the longest pause, a 960-byte block split into 512 + 448 bytes with
212.116 ms between arrivals; the first part was acknowledged locally after
70 microseconds. Another adjacent split paused 210.112 ms. Thus receiver
window exhaustion or a locally delayed ACK was not visible at those stalls.
This receiver-side observation cannot prove when the ESP received the ACK,
or distinguish Wi-Fi loss/queuing from device-side allocation/scheduling.
The capture reported zero kernel drops; only textual packet summaries were
saved locally (`/tmp/espkvm-tcp-audio`), with no raw packet file published.
The observation client completed normally; the capture wrapper's initial stop
missed the privileged child, which was then explicitly stopped and verified.

Retain this as a draft transport experiment: nonblocking retries improve
control of stalled writes but do not resolve PCM continuity. Do not merge on
this evidence. The next investigation should correlate TCP retransmissions,
receiver window/ACK timing and device send stalls, rather than continuing to
increase deadlines. HTTPS, subjective listening and physical latency remain
unverified.

Production-function host TX and WebSocket pacing tests passed, as did the
Function EV app build without compiler warnings. Raw movie captures/logs remain
local under `/tmp/espkvm-nonblocking-01` and `/tmp/espkvm-nonblocking-250`.

## Plain HTTP complete-write handling (2026-10-17)

Fork implementation: [3585933](https://github.com/ZacharyLeahan/espkvm-xbox/commit/3585933),
revised by [f41eab0](https://github.com/ZacharyLeahan/espkvm-xbox/commit/f41eab0).
This is a focused correction to our transport wrapper, not a newly adopted
upstream commit. IDF's WebSocket sender checks only negative send results;
a short positive plain-HTTP write could otherwise advance framing too early.
Plain HTTP now completes partial writes under the per-session lock and shuts
down an incomplete connection on failure. TLS keeps its existing path.

The socket's SO_SNDTIMEO remains in effect for each blocking call, including
PCM's 100 ms setting. A separate five-second deadline bounds retries between
calls; it is not a hard wall-clock cap on a blocking send or lock acquisition.
EAGAIN/EWOULDBLOCK ends a stalled write without renewing its timeout; EINTR
yields before retrying. Incomplete-write logs retain byte counts and errno,
without payloads. No endpoint shape, authentication, queue or bitrate defaults
were changed.

**Rejected first iteration:** an overall budget equal to the short PCM socket
timeout was too restrictive for partial completion. Its combined movie attempt
lost the PCM connection after approximately 26 seconds (1,241,280 frames sent),
with 105 ms confirmed gaps and a 141.516 ms final failed send. The exact failed
return was not instrumented, so deadline expiry was not directly observed.
The owned recording was stopped after about 177 seconds; the viewer was
terminated deliberately before its requested duration. The recording helper
correctly failed `Parallel network audio capture failed`, while its saved
H.264/AAC tracks decoded. `VIDEO/up-000037.ts` had 45,845,680 bytes, zero recorder
queue drops and 980 inserted silence frames. Settings were restored. This
failed attempt remains local under `/tmp/espkvm-http-endurance-01/`.

The revision preserves per-call timeout semantics and tests a short successful
write that consumes 100 ms before the remaining bytes are sent. Host tests
cover partial success/failure, zero return, EINTR, fatal errors, retry deadline,
stale sessions and the existing TLS/session-lock behavior. Fragment pacing
tests and the revised hardware-profile build passed without new warnings.

**Revised smoke test:** a 60-second movie clip, `VIDEO/up-000044.ts`, passed
strict video/AAC decoding with zero recorder queue drops and 280 inserted
silence frames. Live PCM completed with 59.660 s received over 60.010 s wall
time, 325 ms confirmed gaps, no epoch change and a 94.693 ms longest successful
send. Its final incomplete-write log followed the observer's normal closure.
The viewer completed 55.0 s with 517 frames, 9.4 fps, 1283 kbit/s, 39 keyframes
and ACKs. The one-minute pass does not establish uninterrupted endurance or
a causal explanation for the earlier malformed frame. Evidence is local under
`/tmp/espkvm-http-smoke-02/`.

### Revised ten-minute movie trial

**Video duration and recorded-track decoding passed; uninterrupted live audio
failed.** The same 720p/60 HDMI movie source, H.264 4000 kbit/s target, 30-fps
cap, 40 ms PCM queue and 8192-byte TCP buffer were used. Sampling stayed off.
The browser viewer was paused; one measured viewer ran for 570 seconds.

- The original video connection completed all 570.0 s: 5,037 frames, 8.8 fps,
  1128 kbit/s, 341 keyframes and ACKs. No malformed WebSocket frame was observed.
  This bounded pass does not prove the old failure's cause or long-term absence.
- The original PCM connection failed around 166 s. The new log captured
  `incomplete write bytes=512/960 errno=11` (EAGAIN), with a 1.158 s failed
  send. It sent 7,902,960 complete frames, had 48,240 confirmed gap frames
  (1.005 s), no epoch reset and a 332.561 ms longest successful send. This
  establishes partial progress followed by a socket error, not its underlying
  Wi-Fi/TCP cause. A 100 ms socket timeout did not bound observed wall time.
- A replacement PCM observer completed 280.003 s, receiving 278.840 s of audible
  audio. It had 1.135 s confirmed gaps, no epoch change and a 373.416 ms longest
  successful send. Final errno 104 logs followed normal probe closures and are
  separate from the earlier errno 11 failure. The retry does not erase that
  failure. There was a period with no network audio listener, so this is not
  ten uninterrupted minutes of three consumers.
- `VIDEO/up-000139.ts` saved 121,839,604 bytes and 6,358 video frames. H.264 and
  stereo 48 kHz AAC-LC decoded strictly; audio duration was 600.021333 s, RMS
  0.010511, with 38,254,161 active decoded samples. Recorder queue drops were
  zero; 118 silence frames (2.46 ms) were inserted. Capture-overrun samples
  remained zero. AAC packet times were continuous to approximately 1 us
  rounding precision; this is not a physical A/V synchronization measurement.
- The recording helper correctly returned failure for its original parallel
  audio observer while retaining `decode_passed: true`. Original settings
  were restored and the browser resumed. The revised HTTP firmware remains
  installed for evaluation; [PR #7](https://github.com/ZacharyLeahan/espkvm-xbox/pull/7)
  remains draft/unmerged, stacked on the diagnostic PR #6.

Nineteen active-recording samples after startup had 44,423..64,363 bytes
internal free and 12,793,256..12,830,064 bytes PSRAM free. The post-recording
sample had 87,355 and 16,807,492 bytes respectively. No progressive loss was
apparent in this bounded test. Status/log/memory sampling every approximately
30 seconds is part of the load. The monitor's console summary omitted the
listener flag; the detailed saved status correctly shows the original listener
ended before the three-minute observation.

Decision: retain the tested complete-write correction for review, but do not
mark live PCM continuity resolved or infer a causal performance improvement
from different movie scenes. Further work must address send stalls/timeouts
and repeat the combined test without reconnecting either live consumer. Raw
media and logs stay local in `/tmp/espkvm-http-endurance-02/`. Saved-file SHA-256:
`a35758ef2b40dd6fcaef36f2cb9437e20f612b686cb76f80f2d67360b9f546f3`.

## Audible movie endurance follow-up (2026-10-17)

**Recording validation passed; uninterrupted live streaming failed.** The user
started a movie, and a five-second preflight had 237,410 active PCM frames out
of 238,560. The normal firmware (sampler disabled, original 8192-byte TCP buffer,
40 ms browser queue) then recorded for 600 seconds with parallel live PCM and
a measured H.264 viewer. The existing browser viewer was paused. H.264 target
was 4000 kbit/s, 30 fps, with 1280x720/60 Hz HDMI input. No firmware changes
were made during this test. Settings were restored and the browser resumed.

| Check | Measured outcome |
| --- | --- |
| Saved microSD file | `VIDEO/up-001470.ts`, 119,683,244 bytes, 5,876 video frames |
| Strict H.264 + AAC decode | Passed; stereo AAC-LC at 48 kHz, 599.978667 s decoded audio |
| Recorded audio activity | 36,932,863 active decoded samples; RMS 0.004726, peak 0.719116 |
| Recorder loss counters | Zero video/audio queue drops; 4,550 inserted silence frames (94.79 ms) |
| Capture | One observed overrun and one live-stream epoch change |
| AAC timestamps | 28,124 packets; maximum reported gap 1 microsecond (rounding scale), equal audio/video start timestamps |
| Live PCM | 596.260 s received over 600.003 s wall time; 21,823,771 active frames |
| Live confirmed gaps | 175,200 frames / 3.650 s between successful writes; longest successful send 443.409 ms |
| First video connection | Failed between the 122.8 s and 153.0 s monitor samples with `Invalid WebSocket frame: RSV2 and RSV3 must be clear`; at least 1,200 frames had been reported |
| Replacement connection | Completed 360.0 s: 3,078 frames, 8.5 fps, 1094 kbit/s, 222 keyframes, ACKs enabled |

The first viewer failure is retained as a failure, not masked by the successful
retry. Viewer-count samples show zero video clients at 153/183 seconds, one
again from 213 through 546 seconds, and zero after the replacement ended. This
was therefore not ten uninterrupted minutes of three simultaneous consumers.
The SD recording and PCM observer continued throughout. Recorder helper exit
was zero; the first video probe exited one and its replacement exited zero.

Nineteen active-recording memory samples (excluding initial startup) ranged
from 52,303 to 56,891 bytes internal free and 12,765,516 to 12,773,096 bytes
PSRAM free. The first post-recording sample had 87,123 and 16,772,732 bytes free
respectively. No progressive loss was apparent over this bounded run. The
monitor queried status, retained logs and memory approximately every 30 seconds;
these requests are part of the test load.

**Acceptance:** actual movie audio reaches the microSD recording and both
tracks decode. This does not establish lossless capture, subjective listening
quality, physical A/V synchronization, or low latency. Matching container start
timestamps and continuous AAC packet times do not erase the measured silence
padding. Live continuity remains open.

Code inspection identified a follow-up candidate for the malformed WebSocket:
plain HTTP retains IDF's single `send()` call, while its WebSocket sender only
checks for negative returns; the fork's complete-write protection currently
applies to TLS. A short positive HTTP write could corrupt framing, but this
run did not instrument partial writes and does not establish that cause. Test
complete-write handling for plain HTTP separately before claiming a fix.

Local evidence: `/tmp/espkvm-movie-endurance-01/` contains the recording report,
media, consumer/memory samples, retained PCM counters and both viewer logs.
Media and raw logs are not committed. Saved-file SHA-256:
`78529a474483d9267cf054d16d11559cc82b909d147a7da295797dd4e1282151`.

## Sender task-state sampling (2026-10-17)

[PR #5](https://github.com/ZacharyLeahan/espkvm-xbox/pull/5) merged as
`b2d672e` after all checks passed. Follow-up implementation
[ca129ef](https://github.com/ZacharyLeahan/espkvm-xbox/commit/ca129ef1e31ccffac5ac77f63ea03c24c5a5bd6a) adds an opt-in
`CONFIG_KVM_AUDIO_TASK_SAMPLING` observer samples the PCM sender inside socket
writes. It defaults off; no REST shape, capture queue or playback policy changes.
The experimental device build used 100 Hz FreeRTOS ticks, a priority-7 observer
and the existing priority-6 sender, 40 ms PCM queue and 8192-byte TCP send buffer.
Video clients and recording were stopped for these two 60-second observations.

| Trial | PCM received | Confirmed gaps | Longest successful send | Successful-write samples: blocked / runnable / other | Maximum poll interval |
| --- | --- | --- | --- | --- | --- |
| 1 | 59.570 s / 60.008 s wall | 410 ms | 153.502 ms | 57 / 1 / 0 | 11.942 ms |
| 2 | 59.645 s / 60.003 s wall | 335 ms | 190.252 ms | 70 / 3830 / 0 | 11.328 ms |

Both streams ended normally from the observer's perspective; firmware reports
`pcm_send` when a later write sees the closed client. Trial 1's final failed
send took 1.304 s, excluded from successful-send maxima and sample counts above.
Capture epochs did not reset. The source remained effectively silent: zero
active frames in trial 1 and one transient frame in trial 2, so the helper's
`sound_detected` label for trial 2 is not sustained audible-content validation.
A premature overlapping second-listener attempt returned HTTPError; it was
retried only after the first listener completed, and is not one of these trials.

Blocked states demonstrate waits inside the send path. Runnable includes ready
and running states, and these tick-aligned samples are strongly sensitive to
phase and observer preemption; the sharply different counts cannot be read as
CPU-time percentages. Plain HTTP uses blocking lwIP `send()` in this SDK, so
network-buffer/TCP-IP worker waits remain candidates, not proven Wi-Fi loss.
Sampling itself adds overhead. A precise scheduling-versus-network attribution
and an audible combined-load acceptance test remain open. Raw WAVs and logs
remain local as `/tmp/espkvm-sampling-audio-01.*` and `-02.*`.

### Temporary send-buffer comparison

Two more isolated 60-second trials doubled `CONFIG_LWIP_TCP_SND_BUF_DEFAULT`
from 8192 to 16384 bytes with the same sampler enabled. No video clients or
recording were present at either boundary of either trial. The SDK's Kconfig
help advertises `TCP_SNDBUF`, but its socket implementation does not expose
that option (`SO_SNDBUF` is marked unimplemented), so this was a temporary
firmware-wide setting, not a per-audio-socket tuning change.

| Trial | PCM received / wall time | Confirmed gaps | Longest successful send | Successful-write blocked / runnable / other |
| --- | --- | --- | --- | --- |
| 16 KB, 1 | 59.710 / 60.004 s | 270 ms | 152.723 ms | 38 / 20 / 0 |
| 16 KB, 2 | 59.695 / 60.008 s | 285 ms | 152.505 ms | 92 / 3659 / 0 |

**Rejected as a production change:** queue gaps and roughly 153 ms send stalls
remain. These two trials have somewhat smaller gaps than this session's 8 KB
trials, but overlap the variability of earlier isolated tests and do not prove
a repeatable benefit. Extra queued TCP data can also increase latency; physical
delay was not measured. Source activity was again zero/one transient frame,
not audible moving content. There were no capture epoch changes. Logs and
metrics remain local in `/tmp/espkvm-buffer-trials-01/`. The first launch before
the device finished booting timed out without starting a trial; the completed
retry supplied the table above.

Enabled and disabled builds passed, with only existing Microlink dependency
warnings. All 11 observation/recording helper tests passed. The restored build
uses the original 8192-byte buffer and disables task sampling; the normal image
contains existing delivery counters but excludes the sampler's log strings.
After app-only restoration, a 15.002 s smoke observation delivered 14.950 s
of PCM with zero capture overruns and no sampler log entries; source activity
remained zero. The browser video was resumed. Longer combined tests with
sustained audible moving content remain pending.

## Isolated audio workload comparison (2026-10-17)

Two 60-second passes isolated live PCM alone, PCM plus one H.264 viewer, and
PCM plus SD recording, in audio/video/SD/SD/video/audio order. Firmware and the
40 ms browser queue stayed unchanged; H.264 target was 4000 kbit/s with a
30-fps cap. Each video viewer ran for 55 seconds within its PCM trial.
An existing browser viewer was paused for the tests and resumed afterward.
Every trial checked consumer counts before starting and sampled them every
15 seconds: audio-only had zero video clients/consumers; video trials had one
WebSocket client while the probe ran; SD trials had zero WebSocket clients
and one recorder consumer. Capture-overrun samples remained zero throughout.
Earlier comparisons did not enforce absence of unrelated viewers, so their
frame rates are not directly comparable to this series.

| Trial | Confirmed PCM gaps | Longest successful PCM send | PCM received over approximately 60 s | Live video |
| --- | --- | --- | --- | --- |
| 1: Audio alone | 90 ms | 58.944 ms | 59.890 s | - |
| 2: Audio + video | 160 ms | 85.123 ms | 59.820 s | 20.1 fps / 117 kbit/s |
| 3: Audio + SD | 305 ms | 128.580 ms | 59.685 s | - |
| 4: Audio + SD | 545 ms | 232.527 ms | 59.445 s | - |
| 5: Audio + video | 165 ms | 124.407 ms | 59.810 s | 19.9 fps / 117 kbit/s |
| 6: Audio alone | 50 ms | 48.847 ms | 59.930 s | - |

**Decision:** neither video nor recording is necessary for PCM queue loss;
audio-only gaps reproduce it. Added work increased loss in both passes, most
strongly with SD recording in this sample. The capture counters do not point
to lost I2S clocks or capture overruns. This narrows investigation to delivery
and scheduling/backpressure, but does not distinguish Wi-Fi/TCP waiting from
task preemption: elapsed send time includes both. Do not identify the SD card
or network as a proven sole cause, or claim a firmware fix from this test.

**Source limitation:** the HDMI source was silent and low-motion. Network
observations had zero active frames in the audio-only/SD trials and only one
in each video trial. Both SD clips passed strict H.264/AAC decoding with zero
recorder video/audio drops, but the helper correctly returned failure for
`Decoded audio has no samples above the activity threshold`. These are valid
transport/container observations, not successful audible-content checks or
busy-game/movie stress tests. That failure remains in the saved reports.

The two clips were `VIDEO/up-005979.ts` (416 video frames, 2,382,336 bytes) and
`VIDEO/up-006073.ts` (336 frames, 2,400,572 bytes). Both stopped on their 60-second
limits; original settings were restored after each pass. Source media and logs
remain local under `/tmp/espkvm-load-isolation-01/`, including `summary.json`,
each trial's `load.json`, `counters.log`, and PCM observation or recording report.
The SD file SHA-256 values are:

- `29451bbb49b66a367641073fafd2fd44071c1d01c4c374f27a4890e428bbbe1b`
- `842d6e5657e465dfa9d47683db920749eedc28b03b88b1d71bb171a5ad27bbe2`

No firmware or API changes were needed. Next: profile the audio-only delivery
baseline to separate scheduling delay from socket backpressure, then repeat
under an audible moving source and controlled viewer counts. Keep live PCM
continuity and physical latency open in the acceptance ledger.

## H.264 bitrate comparison (2026-10-17)

The retained 40 ms queue and diagnostics firmware ran four 60-second SD/PCM
trials in 4000/1000/1000/4000 kbit/s order. Each included a 50-second H.264
WebSocket viewer started after recording began. All four viewers completed
with ACKs; all saved H.264/AAC tracks passed strict decoding, recorder video
and audio drop counts were zero, and settings were restored after every trial.
The movie continued through different scenes; this is a repeated workload
comparison, not a same-frame deterministic benchmark or perceptual quality test.

| Trial / target kbit/s | Live video fps / delivered kbit/s | Live PCM seconds / wall seconds | Confirmed PCM gap seconds | Longest successful PCM send | SD video frames / AAC seconds |
| --- | --- | --- | --- | --- | --- |
| 1 / 4000 | 4.3 / 595 | 58.385 / 60.003 | 1.580 | 220.779 ms | 760 / 60.032 |
| 2 / 1000 | 8.0 / 719 | 59.495 / 60.022 | 0.495 | 183.260 ms | 793 / 60.032 |
| 3 / 1000 | 8.9 / 564 | 59.620 / 60.005 | 0.360 | 212.575 ms | 784 / 60.075 |
| 4 / 4000 | 6.1 / 779 | 59.630 / 60.005 | 0.355 | 87.867 ms | 718 / 60.011 |

**Decision:** 1000 kbit/s improved delivered video frame rate in both trials,
but did not consistently reduce PCM gaps: the final 4000 trial slightly beat
both lower-bitrate trials for audio continuity. Do not present lowering bitrate
as an audio fix or change the permanent default on this evidence. The delivered
video throughput also did not simply track the configured bitrate cap. Scene
content, frame sizes and send scheduling remain confounders. Physical A/V delay,
browser playback quality and visual quality at the lower bitrate were not tested.

The [comparison helper change](https://github.com/ZacharyLeahan/espkvm-xbox/commit/41969286befc93a1a10250e3e88ddee51652ee58)
adds `--h264-kbps` (500..20000, default 4000), checks applied settings before
starting, saves only allowlisted requested/applied settings, and restores the
original bitrate. Eight host tests passed, including rejection before device
access and restoring a different original bitrate after a failed write.
No firmware or API contract changed in this follow-up.

Local evidence: `/tmp/espkvm-bitrate-comparison-01/summary.json`, each numbered
trial's `result.json`, and its adjacent `*-video.log` / `*-counters.log`.
The clips are `VIDEO/up-001639.ts`, `VIDEO/up-001722.ts`, `VIDEO/up-001802.ts`,
and `VIDEO/up-001878.ts`; raw media stays local and on the card. Recorder is
stopped and original settings restored. Saved TS SHA-256 values, in trial order:

- `e4708879b5035d675b97f69df5d99e3e0558e93288db78df7e23fc852ef98305`
- `2b6e7239a9af1cd5a3ccf7ed6e6ec09928d09f1d24e136e2fe033ab85a509182`
- `bff8c7d130e9a7b11f67dab9389faf3af8e1e9b06b1450276f45cadd55565ae2`
- `ebbfd741a2d625086d30ca392af4b78187de52b728942c45464e1334b46a9ec1`

The isolated-workload comparison above follows this test. It reproduces gaps
even with audio alone; scheduling and transport attribution remain open.
Neither this bitrate comparison nor the isolation check closes live continuity.

## Live PCM queue diagnosis (2026-10-17)

[Diagnostics implementation](https://github.com/ZacharyLeahan/espkvm-xbox/commit/b2d1424c75ea0ececfa5abcb38e339af0ff433c4)
is fork-origin work on top of merged [PR #4](https://github.com/ZacharyLeahan/espkvm-xbox/pull/4)
(`1311644`), not an additional upstream adaptation. PR #4's test/build checks
subsequently all passed (release/pages were skipped).

A 60-second combined recording/network PCM run
with a 40-second H.264 viewer isolated a live-audio delivery problem: the old
40 ms subscriber queue dropped 10,800 frames (225 ms at 48 kHz), exactly matching
read-block sample-index gaps. Startup was 5.942 ms; 36 sends exceeded 5 ms
and the longest
lasted 98.988 ms. Capture epochs did not change. The network observer received
59.750 s of PCM in 60.005 s, while the independent SD audio queue dropped zero
frames and both saved tracks decoded cleanly. Queue loss explains most of that
shortfall, rather than merely inferring loss from wall time.

An experimental browser subscriber held 24 five-millisecond blocks (120 ms),
allowing 20 ms margin above the first observed stall. It still discarded oldest
blocks on sustained backpressure and never blocked the producer or recorder.
Later successful sends exceeded that capacity, so the final change retains the
original 40 ms queue rather than spending extra PSRAM and latency on an
unproven fix. The recorder's one-second queue is unchanged.
Stream-end logs separately report complete sent/read frames, queue drops,
within-epoch gaps between completely sent blocks, epoch changes, time to first
block, maximum send time (all and successful calls separately), sends over 5 ms
and maximum block age before send. A progress line every 30 seconds distinguishes
ongoing delivery from final client-close failures. Queue drops can include
blocks discarded while a closing client's final send is failing, and successful
send means accepted by the transport, not rendered by a speaker. Lines fit
the retained log's
160-byte limit. No endpoint, authentication or error-code contract changed.

Baseline evidence remains local: `/tmp/espkvm-delivery-baseline-02/result.json`,
`/tmp/espkvm-delivery-baseline-h264.log` and
`/tmp/espkvm-delivery-baseline-device.log`. The first baseline start returned
409 shortly after boot; the later retry succeeded. A first post-flash probe
could not reach the device; it changed no settings. These startup attempts are
not passing streaming trials.

The first 120 ms queue trial saved `VIDEO/up-000034-2.ts` (11,642,652 bytes,
583 video frames), with both tracks decoding cleanly, zero recorder audio/video
drops, and 59.925 s of AAC. Network PCM was 59.770 s over 60.006 s; the 40-second
video probe completed at 6.4 fps. The original end-only diagnostic reported
10,320 read-block gap frames and 73,440 queue drops, but included a 1.439 s final
failed send after the observer closed. This did not establish a live continuity
improvement, so diagnostics were refined before the extended repeat.
Evidence: `/tmp/espkvm-delivery-fixed-02/result.json`,
`/tmp/espkvm-delivery-fixed-h264.log`, `/tmp/espkvm-delivery-fixed-device.log`.

The ten-minute 120 ms queue experiment then reported 96,240 gap frames
(2.005 s) between complete writes with no capture epoch changes. Successful
sends reached 361.929 ms. Startup was 0.934 ms; the final failed send took
1.313 s, and total queue drops rose to 153,600 frames while that connection
closed. Those total drops must not be presented as 3.2 s of audible loss.
The original 570-second video probe disconnected early after 337 frames
(code 1006); the device logged send error 11 and a failed WebSocket header.
A separate reconnect completed 450 seconds: 2,762 frames, 6.1 fps,
857 kbit/s, 313 keyframes, ACKs negotiated. Thus this is not a passing
uninterrupted ten-minute video-stream test, and its different load prevents
a clean performance comparison with the original 40 ms queue run.

The saved ten-minute file passed strict H.264 and AAC decoding:
`VIDEO/up-000031-2.ts`, 111,909,256 bytes, 5,395 video frames and 600.043 s of
48 kHz stereo AAC. Recorder video/audio drops were zero, with 959 inserted
silence/padding frames. Track starts matched and AAC packet gaps were within
1 microsecond rounding. Network PCM contained 597.945 s over 600.007 s;
26,677,705 active frames confirmed captured sound, not continuous audibility.
Thirteen active-load memory samples ranged from
48,451..56,707 free internal bytes and 12,755,404..12,818,776 free PSRAM bytes;
the post-stop sample recovered to 78,823 and 16,780,456 bytes respectively.
The recorder stopped on its limit and settings were restored.

Evidence stays local under `/tmp/espkvm-delivery-extended-01/`, with transport
and memory summaries in `/tmp/espkvm-delivery-extended-monitor.log` and video
logs `/tmp/espkvm-delivery-extended-video.log` and
`/tmp/espkvm-delivery-extended-video-retry.log`. Saved TS SHA-256:
`72fc2596259c8fea7ab7b91ef24e65146274dc748b0d8aa0620500b02f6c5790`.
The Xbox build and ACK/SPS/flat-frame host checks passed. The ten-minute
experiment used 120 ms buffering; the final implementation retains 40 ms.

After restoring the original 40 ms queue, a final 30-second combined check
passed strict H.264/AAC decode (`VIDEO/up-000046-2.ts`, 290 video frames,
29.888 s AAC, zero recorder drops). Its 20-second video probe completed at
6.2 fps with ACKs; network PCM was 29.935 s over 30.007 s. Diagnostics recorded
2,640 gap frames (55 ms), successful sends up to 41.629 ms, and a separate
1.332 s final failed send. This verifies the retained diagnostics, not gap-free
playback. The device is left on this image with recording stopped and settings
restored. Evidence: `/tmp/espkvm-delivery-final-01/result.json`,
`/tmp/espkvm-delivery-final-video.log`, `/tmp/espkvm-delivery-final-device.log`.

The bitrate comparison above follows this checkpoint and does not establish
a consistent audio improvement. Further work should isolate concurrent loads
and investigate send scheduling/packetization. Keep uninterrupted video, live PCM continuity,
browser playback and physical latency open in the acceptance ledger.

The PCM counters measure server transport, not speaker output. The browser
still limits individual chunks to the newest 60 ms and resets playback backlog
over 120 ms. Even zero server gaps would not prove lossless browser playback,
physical A/V synchronization, HTTPS behavior or recovery under longer stalls.

## Ten-minute combined streaming/recording test (2026-10-17)

The cache-corrected Xbox image completed a continuous 600-second movie recording
with network PCM active throughout and a 590-second H.264 WebSocket viewer.
No firmware changes or physical reconnects were made during this run.

| Measurement | Result |
| --- | --- |
| Saved microSD clip | `VIDEO/up-001362.ts`, 145,814,680 bytes, 7,580 video frames (about 12.6 fps). |
| Strict decode | Both 1280x720 H.264 and stereo 48 kHz AAC passed `ffmpeg -xerror`. |
| Saved audio | 28,803,072 decoded frames, 600.064 s, RMS 0.01428, peak 0.54922; 43,339,699 active samples. |
| Recorder counters | Zero video/audio drops; 199 inserted silence/padding frames. |
| Container timing | Track starts match; AAC packet gaps within 1 microsecond rounding. This is not a physical lip-sync measurement. |
| Live video | 2,504 frames in 590.0 s, 4.2 fps, 590 kbit/s, 541 keyframes; ACKs negotiated, no early disconnect. |
| Live audio | Connection lasted 600.017 s; 596.760 s of PCM, 24,044,062 active frames. The 3.257 s sample-duration shortfall includes opening/startup timing and possible delivery gaps; no lossless-delivery or physical-latency claim. |
| Memory | Ten active-load samples: internal free 44,391..52,587 bytes, PSRAM free 12,675,444..12,703,808 bytes, 44 tasks. No progressive decline observed. After stopping: 91,355 internal bytes, 16,678,764 PSRAM bytes and 40 tasks. Internal lifetime low-water mark was 43,488 bytes. |
| Audio capture midpoint | Fresh samples (7 ms age), zero reported I2S overruns. This is distinct from network delivery loss. |

The owned recorder stopped on its 600-second limit and the helper verified
restoration of the original video settings. Media remains local and on the card.
Local evidence: `/tmp/espkvm-soak-10min-01/result.json`,
`/tmp/espkvm-soak-memory-10min-01.json`, and
`/tmp/espkvm-soak-video-10min-01.log`. Saved TS SHA-256:
`87fa3d9d9ac7ca21ca2649a056ca1909d39312d350f8d087513153f8676bafbb`.

The bounded recording and PCM tools now accept 1..600 seconds (PCM also accepts
fractional positive durations), with downloads capped at 512 MiB. All ten
existing helper tests pass. The merged-main CI run
[39031261932](https://github.com/ZacharyLeahan/espkvm-xbox/actions/runs/39031261932)
completed successfully on `45eaf61`: all 26 test/build jobs passed.

This closes the ten-minute combined recording/decode and short memory-stability
checks. It does not establish hours-long endurance, lossless network audio,
higher live frame rate, authenticated HTTPS/WebCodecs, physical HDMI recovery,
Tailscale relay recovery or measured A/V delay. The original corruption has not
recurred in the four one-minute trials plus this ten-minute run; its exact
attribution remains non-deterministic. Next useful investigations are live-audio
sample continuity and slow-viewer/keyframe recovery, followed by authenticated
HTTPS and physical HDMI transitions.

## Combined-device validation checkpoint (2026-10-16)

The application-only flash completed and the device served the new frame and
memory diagnostics. No C6 update or card format was performed. These checks
exercise the combined feature-branch image, not a published release.

- A 30-second microSD test saved 442 video frames with zero recorder drops,
  1280x720 H.264 and stereo 48 kHz AAC (30.08 seconds). Both tracks decoded;
  start timestamps matched. Parallel PCM supplied 29.95 seconds. However,
  neither decoded AAC nor PCM showed meaningful signal activity: the source
  was effectively silent. The original helper reported success on nonzero
  codec noise. The helper now requires samples above its existing activity
  threshold (32/32768); the new regression and recheck of this actual clip
  correctly reject it as proof of source audio. Audible-source validation
  remains open. Matching timestamps alone do not prove physical A/V sync.
- A separate 20-second video-only test saved 429 frames with zero recorder
  drops and no audio track. It decoded successfully. Both tests restored their
  original video/audio settings and stopped only their own bounded recordings.
- Three memory snapshots measured 1,934 / 1,900 / 1,801 microseconds. The first
  two occurred during recording; frame counters advanced across each HTTP
  request and frame age stayed 3-8 ms. Internal free memory was 83,623-86,835
  bytes. This short check is not endurance or worst-case snapshot evidence.
- Updated the command-line video probe for the one-byte ACK advertisement,
  negotiated ACK replies, short-message validation and explicit HTTP origins.
  A 12-second H.264 run negotiated ACKs and yielded 15 decodable frames, all
  IDRs, at roughly 1.2 fps. This is a **performance investigation still open**,
  not a successful latency/throughput acceptance result. A static source and
  recovery/backpressure behavior need to be distinguished on a moving source.
- Seven recording-check host tests passed. Captures and raw reports stay local
  under `/tmp/espkvm-adapt-sd-validation-01`,
  `/tmp/espkvm-adapt-video-only-01`, `/tmp/espkvm-adapt-memory-samples.json` and
  `/tmp/espkvm-adapt-probe.h264`; no captured media is committed.

HTTPS browser behavior, meaningful source audio, repeated HDMI transitions,
VPN direct/relay recovery, live upload fallback and final dependency publication
remain acceptance work. No overall five-area completion is claimed.

## Streaming investigation follow-up (2026-10-16)

Implementation: [a781cb5](https://github.com/ZacharyLeahan/espkvm-xbox/commit/a781cb57a510e3998c44efa73d161b974cd94e23).
A received IDR already repairs skipped references, so the video pump no longer
requests another IDR merely because this keyframe follows a sequence gap.
The source attribution is retained for the upstream recovery adaptation.

- Build and application-only flash succeeded. ACK-window and session-TX host
  checks passed. New sanitizer checks exercise the production fragmented-send
  function's message lock, final flags, pacing boundaries and failure unlock.
  A local WebSocket fixture tests advertisement-gated ACKs, short control
  messages and early close. Both new tests are wired into CI.
- A 15-second H.264 run after the IDR correction delivered 21 IDRs (1.4 fps)
  while the encoder reported about 19 fps. This does not establish a throughput
  improvement over the earlier differently loaded run. Codec settings were
  restored after the bounded test.
- A temporary 16 KB burst trial delivered 14 IDRs in 20 seconds and the client
  departed early. No matching allocation/watchdog warnings were found in its
  bounded serial sample, but that does not make the trial successful. It was
  **rejected and reverted**, including on the installed device. The existing
  1 KB fragments / 4 KB burst pacing remains. Network burst tuning is not a
  completed adaptation or a proven fix for low frame rate.
- The old probe could return success after an early socket close if it had
  received any frames. The new probe fails early closes. After restoring the
  4 KB image, an eight-second MJPEG check stayed connected and negotiated ACKs.
  It received one static frame; this is connection evidence, not motion FPS.
- Browser inspection confirmed first-open Video selection and one each of the
  Target, Pointer and Keyboard groups on Input. H.264 correctly explained its
  HTTPS requirement on the existing HTTP page. HTTPS decoding and delayed-save
  browser interaction remain open; host draft-race tests are separate evidence.
- The device advertised a Tailscale address and six peers, but a request from
  this host to that address timed out. LAN access recovered after normal flash
  startup. Neither the advertised peer count nor LAN access proves VPN direct/
  relay operation; that acceptance item remains unresolved.

Local media/logs remain in `/tmp/espkvm-repair-idr.h264`,
`/tmp/espkvm-burst16.h264`, `/tmp/espkvm-burst16-serial.log` and
`/tmp/espkvm-restored-serial.log`. Meaningful moving/audible source validation
and the remaining combined-device gates still precede publication and final
acceptance of the five areas.

## Dependency publication and integration checks (2026-10-16)

Published `upstream-reliability-xbox` branches in the console, Microlink and API
skills forks. Firmware [b8e2ab6](https://github.com/ZacharyLeahan/espkvm-xbox/commit/b8e2ab60b7d0684a3b55e6fc038f5964dbbb4108)
pins console `a258024` and Microlink `d3210a0`, and includes the rebuilt viewer.
API documentation is published at `60949e4` on the matching skills branch.
This supersedes earlier statements that dependency publication/pins were still
pending. It does not mark the feature branch ready to merge into `main`.

- A fresh audio-disabled firmware build passed, with only the previously
  identified Microlink unused-symbol warnings. The audio-enabled build also
  passed with HTTPS compiled in for upcoming TLS transport validation.
- A uniquely named 2,097,152-byte synthetic image uploaded in about 2.65 seconds
  after the capture tests. Its card listing reported exactly 2,097,152 bytes.
  Only that test file was then deleted; absence was checked. Internal free
  memory was 93,279 bytes before and after. This proves the normal live upload
  path, not that smaller-buffer fallback was exercised or file contents were
  read back. Allocation-failure coverage remains the host fault-injection test.
- The host Tailscale client was running. A direct discovery ping to the ESP
  succeeded in 37 ms, then an HTTP video-status request through its tailnet IP
  succeeded. TSMP probes did not reply. Earlier HTTP timeouts therefore do not
  establish persistent tunnel failure, but startup recovery, relay operation
  and sustained tunnel media still need testing.
- HTTPS was disabled at compile time in the previous test profile. Merely
  enabling its saved setting could not activate the feature; the capability
  endpoint explicitly reported it absent. The original setting was restored
  before preparing an HTTPS-capable test image. HTTPS acceptance must be based
  on that image, not on prior HTTP-only tests.

### HTTPS validation boundary

The HTTPS-capable image served its public authentication endpoint with TLS
verified against the CA exported by the device. Its stored login requirement
became active once HTTPS support was compiled in; the earlier profile had
suppressed both HTTPS and authentication at compile time. No saved password
was changed or guessed. Protected TLS video/audio testing therefore remains
pending an authenticated session. A successful public handshake does not prove
media transport, logout revocation or browser WebCodecs behavior.

[ffe1eef](https://github.com/ZacharyLeahan/espkvm-xbox/commit/ffe1eef79e6a5374cba3f1e8ad4e804dc3f5b443)
adds an optional explicit CA to the video probe. Local TLS fixtures verify both
acceptance of the supplied CA and rejection of an unrelated CA; earlier
ACK/short-control/early-disconnect fixture cases still pass. Test certificates
and private build configuration stay outside Git.

The previous HTTP-only build configuration and original `sec_https=false`
setting were restored after this test. The device again reported no login
requirement, 1280x720 signal, advancing capture frames and a fresh frame age.
Stored credentials were left unchanged. HTTPS media validation remains gated
on an authenticated session rather than being claimed from the public endpoint.

### Recorded-clip MP4 compatibility

The production `ts_to_mp4.c` converter, compiled on the host with a 1280x720
fixture driver, converted the new 30-second TS capture successfully. The MP4
contained H.264 and 48 kHz stereo AAC; both decoded without errors. AAC payload
hashes matched after removing TS-only ADTS framing, and decoded float PCM hashes
matched exactly between TS and MP4. This is new-image recording/container
compatibility evidence. It is not an on-device conversion workload test, a
physical sync measurement, or proof of audible source sound (this source was
quiet). The local output is `/tmp/espkvm-adapt-remux/card-recording.mp4`.

## Direct-tailnet endurance and CI checkpoint (2026-10-16)

A 60-second WebSocket run through the ESP's Tailscale address completed without
an early close and negotiated ACKs. It received 12 JPEG frames while the Xbox
sat at the XBMC library. Capture counters advanced and unchanged-frame skipping
was about 25 fps. This demonstrates a sustained static-source connection, not
moving-video frame rate or relay behavior. A subsequent five-second PCM sample
contained no active audio frames. A bounded controller A press did not visibly
start the highlighted movie; a read-only control ping later reported the lease
free. No further blind input was sent. Moving/audible source checks remain open.

The probe now bounds the opening handshake to five seconds and validates its
requested duration (greater than zero, at most ten minutes). A localhost server
that accepts TCP but stalls the WebSocket upgrade correctly fails the test;
ACK, early-close and trusted/untrusted certificate fixtures also pass.

GitHub Actions is enabled. On pushed commit `03b98d4`, the console, host checks
and layout jobs passed. The firmware matrix was still running at this checkpoint,
including an active Function EV build step; no firmware-matrix success is claimed.
Run: https://github.com/ZacharyLeahan/espkvm-xbox/actions/runs/39000357167.
The new diagnostic timeout change is locally tested and will receive its own
published revision; this CI run does not cover that later change.

## CI-discovered Ethernet build correction (2026-10-16)

The first published matrix found missing `CONFIG_KVM_WIFI_DEFAULT_SSID` and
`CONFIG_KVM_WIFI_DEFAULT_PASSWORD` definitions in Ethernet-only profiles
(`p4-eth`, `p4-eth-rev3`, `m5-poe-p4`). Kconfig intentionally omits these symbols
when Wi-Fi is disabled, but the settings schema remains compiled. Firmware
[399ee07](https://github.com/ZacharyLeahan/espkvm-xbox/commit/399ee07952d0fb14e8fc663597bbbc559592ddfe)
supplies empty defaults only when those symbols are absent, preserving configured
Wi-Fi defaults. This is a fork integration correction, not an additional
upstream feature adoption.

A clean ESP-IDF 6.1 build using only public `sdkconfig.defaults` passed with
Wi-Fi disabled, Ethernet enabled and task tracing enabled. Only previously
recorded Microlink unused-symbol warnings appeared. Build-generated removal of
optional dependencies was discarded; the published dependency lock was retained.
The fix and bounded-handshake diagnostic commit were pushed. Their new CI run
is https://github.com/ZacharyLeahan/espkvm-xbox/actions/runs/39000741196;
it completed successfully on code commit `399ee07`: 26 test/build jobs passed,
with two release-only jobs skipped. This includes the three originally failing
Ethernet-only profiles and both Function EV revisions. Later changes in this
checkpoint are documentation only; physical and authenticated acceptance gates
remain open.

## Audible movie validation (2026-10-17)

The user started continuous movie playback on the combined adaptation image.
A five-second network PCM observation delivered 4.61 seconds at 48 kHz,
201,959 active frames, peak 0.13965 and RMS 0.00953. This supersedes the earlier
silent-source limitation; activity is measured, not a subjective listening test.

Three bounded 30-second microSD trials establish the following:

| Trial | Result |
| --- | --- |
| Parallel network PCM | `VIDEO/up-000314.ts`, 8,045,272 bytes, 396 video frames. The AAC track independently decoded to 1,442,816 stereo frames (30.0587 s), peak 0.08334, RMS 0.00674 and 2,335,986 active samples. Parallel PCM delivered 29.895 s with activity. The combined check **failed** on an H.264 macroblock decode error despite recorder counters reporting zero drops. |
| SD only | `VIDEO/up-000375.ts`, 8,530,500 bytes, 461 video frames. Both 1280x720 H.264 and stereo 48 kHz AAC decoded with errors fatal. Audio: 1,440,768 frames (30.016 s), peak 0.15478, RMS 0.00341 and 815,016 active samples. Zero reported video/audio drops; 287 silence/padding frames. Both track timestamps start at 0.7 s; AAC packet gaps are within 1 microsecond rounding. |
| Parallel network PCM repeat | `VIDEO/up-000430.ts`, 7,632,988 bytes, 386 video frames. Both tracks decoded successfully. Stereo 48 kHz AAC: 1,441,792 frames (30.0373 s), peak 0.08510, RMS 0.00625 and 1,714,259 active samples. Zero reported video/audio drops; 646 silence/padding frames. Parallel PCM delivered 29.855 s with activity. Track starts match; AAC packet gaps remain within 1 microsecond rounding. |

Both successful saved clips therefore contain real source audio, including
while network PCM is active. The error did not recur in the parallel repeat,
which does not establish its cause or resolve intermittent corruption. The first trial's
video error remains unresolved; zero drop counters alone do not establish a
clean recording. Each test stopped its own recorder and restored video settings.
Local evidence: `/tmp/espkvm-movie-sd-verify-02` and
`/tmp/espkvm-movie-sd-verify-03`, plus the successful parallel repeat in
`/tmp/espkvm-movie-sd-verify-04`. Captured media is not published.
The SD-only TS SHA-256 is
`9a8b7985a2a3ab91a02524e5388543542dfe03f39ad9c3a8b1b85bfc73b46479`.

A separate 15-second LAN MJPEG probe received 65 frames (4.3 fps), negotiated
ACKs and remained connected. This does not establish H.264 browser performance.
Perceptual A/V synchronization, physical delay, authenticated HTTPS media,
physical HDMI recovery and longer combined-load endurance remain open.

## H.264 output-cache correction (2026-10-17)

Implementation: [ac90156](https://github.com/ZacharyLeahan/espkvm-xbox/commit/ac901569fdcdc8607f5ccd26d5853604a885f724).
This is a fork integration correction to the SPS adaptation, not a new upstream
feature. The CPU SPS insertion moves the rest of an IDR in place. The installed
Espressif encoder writes back only its small slice header before the next DMA,
then invalidates the output after encoding. Dirty cache lines left by our
insertion can therefore overwrite later DMA output during eviction. Both the
asynchronous/PPA and direct-input paths now write back the owned output slot
after SPS editing; failure suppresses publication and requests a recovery IDR.
This follows Espressif's [P4 memory-synchronization contract](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/system/mm_sync.html).

The previously failed TS still reproduces its H.264 macroblock decoding error
offline. Its TS packet alignment/sync bytes are intact and its AAC independently
decodes. A new 60-second baseline recording passed before this correction, so
there is no deterministic failing before/after pair. The cache hazard is real;
attribution of the particular recorded error remains a hypothesis.

Local SPS/flat-frame host tests passed. The Xbox profile, public Ethernet profile
and a clean public revision-3 profile (`CONFIG_ESP_REV_MIN_FULL=300`) built.
Only the previously recorded Microlink unused-symbol warnings appeared in the
clean build. Application-only flashing succeeded; C6 firmware and credentials
were unchanged. Installed application SHA-256:
`24e9119c899290c7a2a52aaec52b88c31dc2f2e2a46c0b3a7c03d6c956bbcc3d`.

Four consecutive 60-second movie recordings passed strict H.264/AAC decoding
with active stereo 48 kHz audio and simultaneous network PCM. All reported zero
video/audio queue drops; reported inserted silence/padding ranged from 48 to
934 PCM frames. Track starts matched and AAC timestamp gaps were within
1 microsecond rounding. Each test stopped its own recording and restored settings.

| Trial | Card file | Video frames | Decoded AAC seconds | Network PCM seconds |
| --- | --- | --- | --- | --- |
| 1 | `VIDEO/up-000043-2.ts` | 605 | 60.0107 | 59.650 |
| 2 | `VIDEO/up-000138.ts` | 774 | 60.0533 | 59.595 |
| 3 | `VIDEO/up-000219.ts` | 795 | 60.0320 | 59.575 |
| 4 | `VIDEO/up-000301.ts` | 769 | 59.9680 | 59.590 |

During trial 2, a 30-second H.264 WebSocket probe also remained connected,
negotiated ACKs and delivered 170 frames (5.7 fps), including 24 keyframes.
This adds a combined-load check, not a controlled throughput improvement claim.
Local evidence directories are `/tmp/espkvm-cache-fixed-01` through `-04`;
raw media stays local. The bounded samples do not prove indefinite reliability,
perceptual A/V sync or authenticated HTTPS behavior. The earlier failure remains
retained as evidence; passing repeats alone cannot establish its exact cause.

CI [39030410499](https://github.com/ZacharyLeahan/espkvm-xbox/actions/runs/39030410499)
on `ac90156` had three successful host/console/layout jobs and no failures at
this checkpoint; the firmware matrix was still running. Local builds cover both
encode paths, but full CI completion is not claimed.

## Video adaptation progress (2026-10-16)

This implementation update follows the reconciliation above; it does not mark
all five areas complete or establish device performance.

- Console [a258024](https://github.com/ZacharyLeahan/espkvm-console-xbox/commit/a258024c2ea4300ad5f0d1d3fe9da526191e1cac)
  adapts `7b508e1` decoder repair and `48c58b0` advertisement-gated frame ACKs.
  It additionally rejects late callbacks after stop/decoder replacement, closes
  rejected decoder configurations, and releases JPEG output completed after stop.
  Typecheck and production build passed; all 80 console tests passed, including
  six new video lifecycle/compatibility cases. The server ACK/window protocol
  and final parent console pin are still outstanding.
- Firmware [1ba5d84](https://github.com/ZacharyLeahan/espkvm-xbox/commit/1ba5d84a6e3b534258109b1a3ef97baa7ca9f57b)
  adapts `e14b7ca` SPS no-reorder signaling into both H.264 encoder paths, without
  adaptive bitrate. Oversized parameter sets and invalid capacities are left
  unchanged. Exact-output, idempotence, tight-capacity and 10,000 bounded
  malformed/truncated-input checks passed with address/undefined sanitizers.
- The working tree now includes `5afe5a5` server keyframe repair and `09e7920`
  explicit component paths. These remain uncommitted pending their remaining
  integration checks. Microlink `5129dc2` remains an uncommitted adaptation too.
- An audio-enabled firmware build of these combined changes passed. Microlink
  emitted unused-symbol warnings at sites confirmed present in baseline
  `d6865e1`; no new warning sites were observed. This is build evidence, not
  direct/relay VPN, browser latency, HDMI reconnect or AAC/MP4 runtime evidence.
- No device flash or push was performed in this implementation step. Shared
  encoder output changed, so the existing SD recording result must be repeated
  on the new image before claiming recording compatibility.

## Memory adaptation progress (2026-10-16)

Implementation: [9a44661](https://github.com/ZacharyLeahan/espkvm-xbox/commit/9a44661e3406935de431ebf836695fe511b87d67).
This supersedes the outstanding status for upload fallback and lazy WireGuard
only; heap/stack diagnostics and frame freshness remain outstanding.

- `cae095f` upload subset: prefer four 256 KB aligned PSRAM buffers, retry with
  smaller sizes down to 32 KB when needed, and accept fewer buffers. Failed
  larger attempts are freed before retry; queue rejection frees the allocation.
  All buffers in a transfer have the same capacity. Existing recording/upload
  exclusion and file handling are unchanged. Eight injected allocation/queue
  scenarios passed under address/undefined sanitizers using the production
  allocator function, including complete exhaustion and cleanup.
- `e14b7ca` WireGuard subset: keep the existing 8 KB worker stack but allocate it
  only on first enable. Serialize worker creation against concurrent settings/IP
  events, and clean up partial semaphore initialization so retries work. Tests
  use the production init/apply/start functions and cover both semaphore failures,
  task creation failure/retry, disabled/enabled boot and 16 concurrent apply calls.
  The worker is retained after first use and sleeps when disabled; this does not
  claim reclamation on disable.
- Both tests are wired into firmware CI. The combined audio-enabled firmware
  build passed without warnings in this incremental build. No new REST fields
  were added in this subset. Live uploads after sustained capture, device memory
  measurements and real tunnel enable/reconnect remain unverified. No flash or
  push was performed; other transport/dependency adaptations remain in progress.

## Video acknowledgement progress (2026-10-16)

Implementation: [9dcc15d](https://github.com/ZacharyLeahan/espkvm-xbox/commit/9dcc15de36ce15d1243904a9046294d13a2e1efd).
Adapts the ACK/window subset of `f28e9ff` and server reference recovery from
`5afe5a5`; it does not yet include per-session transport locks or active-session
LRU protection.

- After picture subscription, the server advertises binary byte `0x85`. A
  compatible client acknowledges received frames with byte `0x09` followed by
  the frame's 32-bit little-endian sequence. This acknowledges arrival, not
  decoding, rendering, sound or physical latency. Older clients remain usable.
- The two-frame window activates only after an ACK matches a recorded send.
  Duplicate/stale ACKs do not release newer frames. Outstanding sends expire
  individually after 500 ms, and client-slot reuse resets negotiation. Malformed
  ACK lengths and oversized client messages are rejected.
- Skipped H.264 references hold back subsequent deltas until a keyframe, with
  repair requests limited to one per 300 ms. Failed fragmented sends close the
  affected session. This policy does not change recording bitrate.
- Sanitizer tests cover old clients, activation, duplicates/stale ACKs,
  out-of-order ACKs, timeout boundaries, sequence wrap and reconnect reset.
  The final audio-enabled firmware build passed without warnings in the
  incremental build. CI includes the ACK test.
- Combined browser/device tests, slow-link behavior, per-client locking, TLS
  partial-write handling and simultaneous SD AAC/PCM verification remain open.
  No flash, push or measured latency claim accompanies this commit.

## Per-session transport progress (2026-10-16)

Implementation: [b63b60e](https://github.com/ZacharyLeahan/espkvm-xbox/commit/b63b60e41b92a3cafd1f4f47cb40637b93b2e55e).
Adapts the remaining session-lock, partial-TLS-write, TCP_NODELAY and WebSocket
LRU subsets of `f28e9ff`. Live transport acceptance is still outstanding.

- Separate recursive write locks isolate sessions. Waiting writers recheck the
  slot generation after acquiring its lock; TLS context lookup happens inside
  that lock. A complete fragmented WebSocket message retains the lock to avoid
  another data message being inserted between fragments. Partial mutex startup
  allocation is cleaned up before a retry.
- TLS writes complete their buffer or fail and shut down the connection. Retry
  loops yield and use the socket's timeout as their deadline, capped at five
  seconds, retaining PCM's 100 ms configuration. This is a software retry budget,
  not a measured hard real-time wall-clock guarantee for a blocking TLS call.
- New sessions disable Nagle and refresh existing WebSocket LRU counters. IDF
  6.1 `httpd_sess.c` excludes `for_async_req` sessions from LRU selection, so
  async PCM/MJPEG/upload workers already have that protection. Refreshing a
  WebSocket is a preference over idle HTTP, not immunity when all slots are active.
- Production-function host tests pass under address/undefined sanitizers:
  another session proceeds while one is locked, recursive acquisition works,
  changed generations reject a queued writer, partial writes advance correctly,
  retry paths yield, PCM/default retry budgets terminate, and closed sessions
  cannot look up a TLS context. The final audio-enabled firmware build passed
  without warnings in the incremental build. CI includes these tests.
- Device latency, simultaneous PCM/AAC recording, slow viewers, connection-pool
  exhaustion and repeated teardown still require integrated testing. No flash,
  push or before/after performance claim was made in this step. Diagnostics and
  remaining Microlink validation are still unfinished parts of the five-area goal.

## Capture diagnostic progress (2026-10-16)

Implementation: [f723d14](https://github.com/ZacharyLeahan/espkvm-xbox/commit/f723d144f4fa5248a60695478f827b3d3242a531), adapting the frame-freshness subset of `11c11cf`
and `psramLargest` from `cae095f`. Companion API contract:
[4afef83](https://github.com/ZacharyLeahan/espkvm-skills-xbox/commit/4afef8318cd5755df78289c9720b959f58bf3348).

- `video/status.frames` counts CSI completions since boot, including discard
  buffers, independently of encoder/viewer demand. It wraps at 2^32. The counter
  and timestamp are read together under the ISR/task lock.
- `frameAgeMs` is null before the first completion, ages when capture stops,
  and saturates at 4294967294 ms. Neither field proves display, encoding,
  recording, A/V synchronization or physical latency.
- `system/info.psramLargest` complements total `psramFree` to expose
  fragmentation. Older Xbox images can omit all three new fields; companion
  documentation now explicitly separates fork support from upstream versions.
- Audio-enabled firmware build passed without warnings in this incremental
  build. Runtime stop/restart, no-viewer capture and fragmented-memory response
  verification are still outstanding. Upstream MCP access was rechecked and
  still returned HTTP 404; no MCP update is claimed.
- Task heap/stack endpoint remains outstanding. Upstream keeps task-name
  pointers after its snapshot, and its debug task-snapshot API does not acquire
  locks. Review task lifetime on this two-core IDF build before copying that
  implementation. This does not defer the requested diagnostic scope permanently.

## Microlink reconnect adaptation (2026-10-16)

Dependency implementation: [d3210a0](https://github.com/ZacharyLeahan/microlink/commit/d3210a0523766d6917706aa643750082f61d49f8), adapting `5129dc2`
on top of the six reliability cherry-picks listed above. Parent pin/publication
and integrated VPN validation remain outstanding.

- Preserve keys when moving a validated session between direct and DERP paths,
  retain in-flight initiation, request recovery after unanswered data, bound
  CallMeMaybe traffic and remove problematic voice-queue UDP marking.
- LAN interface iteration runs through `esp_netif_tcpip_exec`, as the local IDF
  `esp_netif_next_unsafe` contract recommends. It filters down/unaddressed/
  loopback interfaces, deduplicates addresses and bounds output. Changed endpoint
  update and peer-status reads also take the lwIP core lock.
- Production-function sanitizer checks passed for TCP/IP-context enumeration,
  duplicate/invalid addresses, output capacity and execution failure; path changes
  preserve keys, an in-flight handshake is not restarted, and only unanswered
  data triggers the additional recovery policy. These mocks do not exercise
  real crypto, packet queues, DNS, DERP transport or radio behavior.
- The combined audio-enabled firmware build passed without warnings in the
  incremental build. Existing logging suppression and task stack sizes remain.
  No C6 firmware changes, adaptive recording-quality changes or ACME were added.
- A read-only device check succeeded at `192.168.1.169`: HDMI locked at 1280x720,
  MJPEG selected, recording inactive. The installed image still lacks the new
  freshness fields, confirming the new build has not been flashed. This is
  reachability evidence only, not verification of this adaptation.

## Source-copy build verification (2026-10-16)

Implementation: [daffe2c](https://github.com/ZacharyLeahan/espkvm-xbox/commit/daffe2c2f4872db3b61aa3be9a4163f4741d0541), adapting the explicit component-path subset of
`09e7920`. This completes that source-path adaptation; release packaging and
unrelated board/documentation portions remain omitted.

A temporary source copy was populated from firmware, console and Microlink Git
archives, omitting symlinks. The changed top-level CMake file was copied in,
verified managed dependency packages were copied to avoid redownloading, and a
private audio-enabled profile was used only in the temporary directory. The
Microlink `components/wireguard_lwip` symlink was confirmed absent. A full
`idf.py build` succeeded from that source copy with real nested component paths.
The regular checkout audio-enabled build had also passed with those paths.
Known Microlink unused-symbol warnings appeared in the full build; no new
warning sites were observed. This validates component discovery, not a published
release archive, rebuilt console bundle or installed device image.

Task-memory snapshot work remains open. Local IDF source confirms scheduler
suspension is per-core; retaining TCB/name pointers across an unlocked interval
needs an explicit lifetime solution before adapting upstream's task table.
No device firmware, C6 or SD contents were changed during this verification.

## Heap/task diagnostic adaptation (2026-10-16)

Implementation: [4ccb502](https://github.com/ZacharyLeahan/espkvm-xbox/commit/4ccb50280c8f92556d310efcd42825faa3463e04), adapting the memory endpoint from `e14b7ca`.
Companion contract: [60949e4](https://github.com/ZacharyLeahan/espkvm-skills-xbox/commit/60949e49c6dba9dbf6f775424bb9918813e5b976).
This supersedes the outstanding source-implementation status for heap/stack
diagnostics; integrated device validation remains open.

- Authenticated `GET /api/v1/system/memory` returns internal/DMA/PSRAM heap
  totals, free/largest/minimum free bytes and block counts, plus named task
  priorities, stack location/range and minimum free stack bytes. `internalStacks`
  totals usable internal stack ranges; it does not include all task overhead.
- Unlike the upstream snapshot, a short-lived pinned worker coordinates both
  schedulers through the other core's IPC task, copies names/stack metadata into
  owned storage, then resumes before JSON or network output. No task pointers
  are dereferenced after resume. Task tracing is enabled; no runtime-stat timer
  or blanket task-stack reduction is introduced.
- Collection is bounded to 128 tasks and reports JSON 503 for unsupported tracing,
  allocation failure, IPC failure or insufficient snapshot capacity. It includes
  its temporary `kvm_mem` task; `snapshotUs` measures the local task-copy interval,
  not all IPC/HTTP time. Heap and stack samples are not simultaneous. Use it for
  occasional diagnostics, not rapid polling.
- A two-thread production-function test passed with address/undefined sanitizers:
  both schedulers are paused during metadata access, copied names outlive source
  changes, and task-capacity/IPC failures restore scheduling. CI includes the test.
  The audio-enabled tracing build passed; only previously identified Microlink
  unused-symbol warnings appeared in the full rebuild.
- Device measurements of snapshot cost, repeated collection under PCM/AAC/video,
  low-memory HTTP errors and authentication still remain. This commit is not
  evidence of live safety/performance and has not been flashed or pushed.

## Earlier decisions, reconciled on 2026-10-16

| Upstream commit / scope | Decision | Decision rationale / scope | Our implementation / verification |
| --- | --- | --- | --- |
| [5d18b5b](https://github.com/espkvm/espkvm/commit/5d18b5b08037c970d83e7947fce9dc9be349bc3f): FireBeetle Wi-Fi pins | Adopted (inherited from fork main) | Initially ignored as irrelevant to Function EV. Retained when reconciling the user-requested PR merge on 2026-10-11, because it already existed on this fork's main. Preserve both FireBeetle and Function EV GPIO 6 reservations; the added pull-up default applies only to FireBeetle. | See [fork PR #1](https://github.com/ZacharyLeahan/espkvm-xbox/pull/1) merge ancestry; no FireBeetle hardware validation. Function EV firmware rebuild passed before merge. |
| [c5761dc](https://github.com/espkvm/espkvm/commit/c5761dc7db6e798e54bbcea1bbcde468f23c3319): Wi-Fi memory, power saving, co-processor update support | Adapted (partial) | Wi-Fi power saving disabled; cache-aligned SDIO PSRAM allocation implemented for P4 SDIO builds with the existing PSRAM-preference option. The Xbox profile also prefers PSRAM for lwIP allocations. Keep our small TCP buffers. Ignore co-processor updater/UI additions and do not flash the C6. | Power-saving subset: `9135969`. SDIO/lwIP memory subsets: [59da248](https://github.com/ZacharyLeahan/espkvm-xbox/commit/59da2481b330042d9d4edabdff87ce407c852693). Combined capture passed with recovered gaps; see the 2026-10-16 record below. |
| [09e7920](https://github.com/espkvm/espkvm/commit/09e792014de0446bef92269ef681ed549f396aa6): archive builds, FireBeetle validation, README | Adapt planned (partial) | Take explicit CMake component paths to remove dependence on a preserved symlink. Ignore FireBeetle/marketing edits; defer release-archive packaging until we publish archives. | Not implemented; test recursive-clone build and source copy with submodule content but without the symlink. |
| [cc3daee](https://github.com/espkvm/espkvm/commit/cc3daee460ecea6a6fa35ac637a3abea39be7dd4): board schematic corrections | Adapted (partial) | Function EV GPIO 6 is reserved for co-processor IO2 and the SD_PWRn comment is corrected; SD power behavior is unchanged. Other board changes were omitted. | [d42614a](https://github.com/ZacharyLeahan/espkvm-xbox/commit/d42614acdb3a24764e177b77c990ada318efcff0). Build/host checks passed; see the first implementation batch below. Corrects the stale "Not implemented" row. |
| [338c8ce](https://github.com/espkvm/espkvm/commit/338c8ce17f1f89e65f0264e9dc0ca043c0376091): notifications, 2FA, RTC, update URLs | Deferred | No current need for RTC hardware, notifications or 2FA UX. Revisit when one becomes a fork requirement. Do not migrate this fork to upstream firmware manifests; our releases must preserve Xbox/LCD customizations. | No implementation planned now; this is not a declaration that authentication is unnecessary on untrusted LANs. |
| [42a0d1c](https://github.com/espkvm/espkvm/commit/42a0d1c836e5cd693275c6c938675acbdd986a7a): release and dependency pins | Adapt planned (partial) | Adapt the Microlink reliability fixes behind this dependency bump: handshake time, receive handling, endpoint validation, TLS cleanup, core locking and framed JSON parsing. Preserve quiet packet logs; do not import unrelated release/console changes. | Not implemented; review dependency commits individually, then test Tailscale direct/relay connections, reboot reconnection and repeated failures. |
| [5afe5a5](https://github.com/espkvm/espkvm/commit/5afe5a5888db1fea216a9d81babb2af5cc6d1c8c): H.264 Wi-Fi playback | Pending (reopened; recommended) | H.264 is now used for microSD recording, so the old "no H.264 planned" rationale is stale. Review browser decoder/keyframe recovery together with the newer SPS and frame-ack work; preserve HTTPS/WebCodecs requirements. | Absent from both fork server and console. Recorded H.264/AAC success does not establish browser recovery or latency. |
| [ab5e0e7](https://github.com/espkvm/espkvm/commit/ab5e0e720bf9bf87a9de4225e6d9a1ba4610ed0d): upstream flasher redirect | Ignored | Upstream website/flasher deployment is not our firmware or release destination. Design fork-specific publishing if needed later. | Not applicable |
| [7918023](https://github.com/espkvm/espkvm/commit/79180238746747056354a79e8338d07ae0e56d57): upstream firmware publishing | Ignored | Upstream firmware-host publishing workflow is not ours. Do not inherit deployment destinations or credentials assumptions. | Not applicable |
| [6f96380](https://github.com/espkvm/espkvm/commit/6f9638042615f01906c3569105d9b7022212798d): update checks and missing-C6 boot recovery | Adapted (partial) | Network-stack initialization already precedes optional C6 startup. Redirect/certificate-bundle updater work remains deferred until a fork-specific updater is designed. | [d2e8925](https://github.com/ZacharyLeahan/espkvm-xbox/commit/d2e8925b48280ab8bcd5715c62e64e19ec7a2ae8). Build/host tests and normal boot passed. Missing-C6 fault injection and repeated warm restarts remain untested. |
| [4565409](https://github.com/espkvm/espkvm/commit/45654092ff35b3055eef3df2861607bbe4ff3762): initial setup hotspot | Deferred | Automatic open setup-hotspot behavior is coupled to upstream onboarding. Keep our current saved-Wi-Fi/failover behavior; revisit as a deliberate credential-free first-run experience. | No implementation planned now; any future setup AP must restrict unauthenticated control. |
| [e5c109e](https://github.com/espkvm/espkvm/commit/e5c109e58d5d6d4eb91b219d4f0c88e32371793b): password/network onboarding | Deferred | Password-driven network selection does not fit our present profile. Revisit together with setup-hotspot/onboarding design, rather than importing auth and console changes piecemeal. | No implementation planned now. |
| [ce519cc](https://github.com/espkvm/espkvm/commit/ce519ccb63cca1fb9f0ee8de6a670278838d5bb4): CEC and pointer lock | Deferred | CEC and relative-mouse pointer lock are not the original-Xbox USB controller path. Revisit for a demonstrated HDMI-adapter/CEC use case or PC-target support; do not add extra bridge/task load now. | No implementation planned now. |

## New upstream inventory (2026-10-16)

These historical inventory rows cover every commit in `ce519cc..4c62662`.
The current reconciliation table above supersedes implementation statuses here.
**Pending (recommended)**
means worth adapting, with no implementation authorized by this reconciliation.
Mixed releases are split where their tradeoffs differ. All implementation and
hardware tests for these new subsets remain outstanding unless explicitly noted.

| Source / subset | Decision | Fork comparison and next gate |
| --- | --- | --- |
| [1af4c1c](https://github.com/espkvm/espkvm/commit/1af4c1caa460b610db3f9f84420615943d3cc24a): 480p FIFO and EDID | Pending (selective review) | Dynamic FIFO sizing is new: our fixed threshold is 374 words; upstream uses 315 for 720x480 UYVY. Our enabled RGB path has 540 words/line and would not change, so this is not evidence of a fix for our current intermittent video. The new 480p EDID passes checksum/LPCM checks below; hardware mode switching remains untested. |
| [1af4c1c](https://github.com/espkvm/espkvm/commit/1af4c1caa460b610db3f9f84420615943d3cc24a): CSI DMA diagnostics | Ignored as replacement (existing fork work) | Upstream explicitly credits our [b6534b0](https://github.com/ZacharyLeahan/espkvm-xbox/commit/b6534b048a827946b0f8953b1dda401306f61e10). Retain our diagnostics and mode-dependent lane/timing handling. |
| [b05dd03](https://github.com/espkvm/espkvm/commit/b05dd03c9c3d5d218a3a187f1239f09d2a0f5b7b): Ethernet with Wi-Fi backup | Deferred | Useful when this installation uses both links. Requires coordinated network-mode/schema/UI work and later mDNS fixes; preserve the Function EV C6/SD startup and current saved-network behavior. |
| [eabc780](https://github.com/espkvm/espkvm/commit/eabc780e280fe48a70d09e43a3323292b9e7affd): HORI/XInput gamepad, auto-network release | Deferred | Generic Switch/Windows gamepads are a separate USB architecture from our original-Xbox XID, touch and BLE path. Revisit only for an explicit additional-target requirement; do not replace our descriptors with the release wholesale. |
| [e191569](https://github.com/espkvm/espkvm/commit/e191569782761b268900f6d3100ea5d097f34eed): README gamepad clip | Ignored | Upstream promotional media; no fork behavior to adapt. |
| [5180eaa](https://github.com/espkvm/espkvm/commit/5180eaa7d2f81714fc5cae572eaaad4ec1eb280f): live ESP log tail | Pending (useful diagnostic) | Incremental authenticated log retrieval could simplify intermittent HDMI/audio investigations. Review ring concurrency, allocation/task failure cleanup and privacy before adapting; maintain the existing boot-log fallback and bound memory use. |
| [5180eaa](https://github.com/espkvm/espkvm/commit/5180eaa7d2f81714fc5cae572eaaad4ec1eb280f): serial, netconsole, URL downloads and updater transport | Deferred | New UART/network services and image-fetch workflows are not needed for the current capture task. Revisit with a target-log or image-fetch requirement; review pin conflicts, auth, cancellation and recording/storage ownership. Firmware publishing destinations remain fork-specific. |
| [341386f](https://github.com/espkvm/espkvm/commit/341386f8fcc0f64cda56677d659fd21e6672a688): read-only card explanation | Pending (recommended) | Its console change removes inappropriate reformat advice. Take the small text fix; a read-only reason must not imply that the user's card should be erased. Console source is linked below. |
| [11c11cf](https://github.com/espkvm/espkvm/commit/11c11cfee03f1ceae9c52f4651672ba2b5a5c3f2): frame freshness, UI fixes, pin probe | Pending (selected diagnostics/UI) | Frame count/age distinguishes HDMI lock from actual delivered frames. Combine with our existing capture metrics and audio fields. The initial-settings-tab fix is independent. Generic pin probing is deferred: its optional test mode drives a GPIO and must not touch our I2S, CSI or other reserved pins. |
| [11c11cf](https://github.com/espkvm/espkvm/commit/11c11cfee03f1ceae9c52f4651672ba2b5a5c3f2): button, M5 displays/RTC, card detect and bridge interrupt | Deferred | Additional peripherals and M5-specific wiring are outside the present Function EV/C790 profile. Do not import GPIO 37/38 or card-detect polarity without this board's schematic and wiring evidence. |
| [cae095f](https://github.com/espkvm/espkvm/commit/cae095f6b7e3dd9e4591f58edd09a3cd490caa0b): fragmented-PSRAM upload fallback, memory/video diagnostics | Pending (recommended subset) | Our uploader still requires four 256 KB buffers; upstream falls back to smaller/fewer allocations. `psramLargest` and the H.264 CPU-path flag improve diagnosis. Test low-memory cleanup and uploads after sustained capture, retaining the refusal to upload during recording. LT6911-only EDID visibility is not a C790 fix. |
| [e14b7ca](https://github.com/espkvm/espkvm/commit/e14b7caafd5b46d9db799dabc24f2c5c930870cd): H.264 SPS no-reorder and video timing | Pending (recommended) | Explicit no-reorder signaling can reduce decoder buffering; upstream's latency results are not measurements of our fork. Its global next-frame source timestamp needs per-job propagation on our asynchronous RGB/H.264 path, or another capture can overwrite it. Keep the recorder's tested clock origin. Validate browser recovery and TS/MP4+AAC compatibility; capture-to-send metrics do not establish display delay or A/V sync. |
| [e14b7ca](https://github.com/espkvm/espkvm/commit/e14b7caafd5b46d9db799dabc24f2c5c930870cd): adaptive bitrate/quality | Deferred pending recorder policy | Feedback changes the shared encoder. A slow browser could lower microSD recording quality too. Revisit with explicit recording priority or an opt-in policy; never silently change capture quality to accommodate a viewer. |
| [e14b7ca](https://github.com/espkvm/espkvm/commit/e14b7caafd5b46d9db799dabc24f2c5c930870cd): memory page and lazy WireGuard task | Pending (recommended subset) | Heap/stack watermarks and avoiding an unused WireGuard task can recover internal RAM. Keep the AAC writer's measured stack/priority. Upstream's stack reductions, including the clip converter, predate our AAC and cannot be copied as validated limits. |
| [e14b7ca](https://github.com/espkvm/espkvm/commit/e14b7caafd5b46d9db799dabc24f2c5c930870cd): persisted sessions, generic USB polling, M5 gating, button/network/mouse features | Deferred | Session persistence changes expiry policy: restored tokens receive a new 12-hour lifetime. Keep our logout/password-change audio revocation. Keyboard/mouse polling changes are not an XID latency fix; M5 frame gating is not our LCD pipeline. Revisit other features individually. |
| [157a797](https://github.com/espkvm/espkvm/commit/157a797f6d3ede7d1d0c3a0bb3f8808083bca257): service-worker refresh and certificate renewal UX | Pending (cache refresh); certificate UX deferred | Refreshing the saved console helps avoid stale UI after firmware updates. Pair it with the console recovery notice; test offline fallback and version changes, caching navigation only. Custom certificate renewal is useful when deployed with custom certificates. |
| [443f9d7](https://github.com/espkvm/espkvm/commit/443f9d75edfff186ed57bb2f0eb8f0e2384210d5): printed M5 case | Ignored | Documentation for different hardware. |
| [c90457c](https://github.com/espkvm/espkvm/commit/c90457ce3b6d47d6a7a84b0284ebdf74d2ed749c): Tailscale Let's Encrypt, ACME/SNI | Deferred | Potentially useful for trusted remote HTTPS, but adds certificate provisioning, DNS API support, renewal state and console controls. Revisit as a coordinated feature after tunnel reliability; do not conflate it with the transport fixes below. |
| [48c22d3](https://github.com/espkvm/espkvm/commit/48c22d3a9e655328fbfa8e098825fdb32c312b64): TC358743 I2C timeout and failed-read handling | Pending (highest-priority adaptation) | Our register transfers still use an unlimited timeout. Upstream bounds each transfer to 200 ms and distinguishes failed timing reads from absent HDMI. Adapt into our corrected audio-register/ANA_CTL path: upstream still defines FORCE_MUTE as `0x8602` instead of our corrected `0x8600` and lacks the HDMI audio PLL restoration. Verify stuck-read recovery, repeated reconnects and 480p/720p with SD audio recording. |
| [48c22d3](https://github.com/espkvm/espkvm/commit/48c22d3a9e655328fbfa8e098825fdb32c312b64): original-Xbox XID | Ignored as replacement (existing fork work) | Upstream credits this fork's protocol. Preserve our [2120786](https://github.com/ZacharyLeahan/espkvm-xbox/commit/2120786744058dcb7bcf8edaa7caaddfa97591b3) and subsequent touch/BLE/controller arbitration. Upstream's generic gamepad path uses different stick/trigger representation and runtime settings; it is not an upgrade to our integration. |
| [48c22d3](https://github.com/espkvm/espkvm/commit/48c22d3a9e655328fbfa8e098825fdb32c312b64): oversized mode at boot | Pending (recovery subset only) | Our [961ccea](https://github.com/ZacharyLeahan/espkvm-xbox/commit/961ccea46b23b8434a5c2fad4f39a2818ecbe747) already rejects unsafe sizes. Upstream additionally starts a capped receiver so a later valid mode can recover; review this startup difference. Test an oversized boot mode followed by supported 480p/720p without restart, retaining our bounds guards. |
| [48c22d3](https://github.com/espkvm/espkvm/commit/48c22d3a9e655328fbfa8e098825fdb32c312b64): shared `kvm_proto` header | Deferred | Useful for a C client, but optional for focused fixes. Our current control codes end at `0x07`; upstream's new `0x08` gamepad message is not an existing opcode collision. Reconcile controller semantics and preserve existing clients before importing its handler. |
| [3a9bacd](https://github.com/espkvm/espkvm/commit/3a9bacd8c16166e11197a01d1a19ba89de8f4269): settings drafts and repeated groups | Pending (recommended subset) | Our console can reuse inputs under repeated group keys and lose typing during saves. Adapt the fix with additional failed-save/newer-edit protection; the desktop diagram and upstream roadmap cleanup are not required. |
| [9277853](https://github.com/espkvm/espkvm/commit/92778539b002befabb234f905d69292c5e2515d4): documentation reorganization | Ignored as wholesale replacement | Our README was already shortened with Xbox setup, credits and measured audio evidence. Preserve that material; use upstream docs as a reference when adding a relevant feature. |
| [f28e9ff](https://github.com/espkvm/espkvm/commit/f28e9ffd9c59f87f6a1c9ac8d98184472c55cb5c): bounded frame delivery, per-client locks, TCP_NODELAY, active-socket protection and frame ACKs | Pending (recommended, coordinated) | Useful for slow-link playback and responsive control. Its HTTPS override retries writes for five seconds, conflicting with our PCM path's 100 ms send bound and prompt session-revocation checks. Preserve a short PCM deadline. WebSocket LRU protection alone does not protect async PCM/MJPEG streams. Test socket reuse, partial writes, slow viewers and simultaneous network audio + SD recording. |
| [f28e9ff](https://github.com/espkvm/espkvm/commit/f28e9ffd9c59f87f6a1c9ac8d98184472c55cb5c): Microlink reliability, UDP mailbox and interface advertisement | Pending (recommended) | Review exact dependency commits below. The 64-packet UDP mailbox is tuned to upstream's much larger TCP windows; measure internal-RAM pressure with our smaller windows, PCM and AAC rather than copying the number as a proven setting. |
| [4c62662](https://github.com/espkvm/espkvm/commit/4c626628b8f7fdf225edc168ca6a1d3d423c2fa5): P4-WIFI6-DB test status and board links | Ignored for this profile | Newly documented tests concern another board/C792. They do not validate our Function EV/C790 setup or audio recording. |

There is no upstream shared-I2S/AAC recorder implementation in this interval.
The only upstream `kvm_record` change is a clip-converter stack reduction in
`e14b7ca`. Our recorded audio, independent subscriptions, remux tests and console
audio controls must survive any future adaptation.

## Console dependencies and small fixes

At the original inventory, entries below were **Pending recommendations**;
the current reconciliation table supersedes implemented subsets. The explicitly deferred
certificate feature remains deferred. Upstream has 20 console commits beyond our shared base
`16f9d8141383a4c6165df280080d7ea50acaa263`; its package manifest and lockfile match
ours. This is not a reason to advance the whole submodule: upstream does not
contain our PCM playback and recording-audio UI.

| Source | Adaptation boundary / verification needed |
| --- | --- |
| [2b6907c](https://github.com/espkvm/console/commit/2b6907c376f5fe381fc10202dcb41671d0661e0e) | Fix initial settings selection to use the section's `id`, not the section object. Verify first open, tab switching and search. Other UI features are separate. |
| [e585e98](https://github.com/espkvm/console/commit/e585e98d48162208bfb2bad7103987c480ba178f) | Group repeated Target/Pointer sections once and retain text/number drafts. Strengthen upstream's implementation: do not erase a failed save or let completion of an older save erase newer typing. Test both cases and delayed saves; omit the desktop diagram. |
| [863429f](https://github.com/espkvm/console/commit/863429f997317e4c98a2c59e9d457802ae7435a1) | Remove generic reformat advice from the read-only card explanation. Verify different device-reported reasons render clearly. |
| [73ea97e](https://github.com/espkvm/console/commit/73ea97edf203caccad1e5f0ad1ef69161e1bd73a) | Return to sign-in after a 401, without importing 2FA. Verify logout, password changes, retry cancellation and network-audio shutdown. |
| [7b508e1](https://github.com/espkvm/console/commit/7b508e1e02f675a28ec2c77050b83058e66f1df6) + firmware `5afe5a5` | H.264 decoder recovery and bounded keyframe requests. Test missing deltas, decoder errors and stopped-stream messages over HTTPS. |
| [4d12807](https://github.com/espkvm/console/commit/4d128076c3d3d32c869e542071b90ba6740b9c1b) + firmware `e14b7ca` | Review only no-reorder playback/latency diagnostics alongside the SPS change. Mouse-based delay measurement and automatic wake do not establish Xbox controller or audio latency. |
| [48c58b0](https://github.com/espkvm/console/commit/48c58b037ac90504db677e83f03fa731b41bb664) + firmware `f28e9ff` | Preserve the `0x85` advertisement gate before sending frame ACKs: old firmware interprets extra messages as keyframe requests. These acknowledge arrival, not decoding/display. Test old/new combinations, reconnect reset and slow links. |
| [06e59ba](https://github.com/espkvm/console/commit/06e59bafa7154f252ecbf8e5867de2635281eef7) + firmware `157a797` | Cache/certificate recovery notice accompanies the service-worker fix. Keep audio and API responses outside its cache. Custom-certificate renewal UI can remain deferred. |
| [f68f5a9](https://github.com/espkvm/console/commit/f68f5a9f9fdd8022abdc4050ba2bd5c85aa41d29) + firmware `c90457c` | Deferred: ACME/Tailscale certificate feature and its endpoints must land together. |

## Microlink dependency reconciliation

Our pin and upstream share [`1c87e44`](https://github.com/espkvm/microlink/commit/1c87e44b2313ca8113e2e05d540c1481a0ead2f0).
The older `42a0d1c` dependency recommendation is still outstanding; our
`d6865e1` quiets logs but does not include these reliability repairs. The
latest repair can be adapted without importing the intervening certificate
feature or copying task-stack reductions.

| Exact dependency source | Decision / scope |
| --- | --- |
| [563cb91](https://github.com/espkvm/microlink/commit/563cb9136fe1814995f3a5d699b2436ea94cc8e1) | Pending overlap review: additional packet-log quieting; preserve our existing quiet logging rather than claim a new runtime fix. |
| [599aa3f](https://github.com/espkvm/microlink/commit/599aa3f38598bf0bc0b3728dae03ae9ff87185b0) | Adapt planned under `42a0d1c`: blocking receive/draining and larger internal receive queue. Separate this queue from the lwIP UDP mailbox and measure both allocations. |
| [f6ec6a4](https://github.com/espkvm/microlink/commit/f6ec6a42ab4d6bc2e359b9d0bde77fbd355e6b9c) | Adapt planned: retain DERP until a direct endpoint is validated. |
| [2707d38](https://github.com/espkvm/microlink/commit/2707d3840ce76bf0e29ce4fa0e0afd4dec30a586) | Adapt planned: wall-clock handshake timestamps for reboot recovery; test cold boot with delayed clock synchronization. |
| [d070788](https://github.com/espkvm/microlink/commit/d0707882ef52e0b31cc9a3a86d7ef3c8326b231c) | Adapt planned: free TLS allocations after failed DERP connection attempts; verify heap returns after repeated failures. |
| [9794187](https://github.com/espkvm/microlink/commit/979418717e2d89cbe46bd6334b1668eda882d70f) | Adapt planned: missing lwIP core locks. Both Tailscale and classic WireGuard share this stack; test both. |
| [de8c581](https://github.com/espkvm/microlink/commit/de8c58116c9c8cf0bbed83d96885e64daddb66d4) | Adapt planned: locate JSON using its frame length rather than the first `{` byte. An extracted host check passed six cases, including a length-prefix byte of `0x7b`; this is not integrated VPN validation. |
| [5129dc2](https://github.com/espkvm/microlink/commit/5129dc2735419ec54b74a95d48193ff862f29338) | Pending (recommended): start/recover sessions, retain in-flight handshakes during path changes, limit CallMeMaybe loops and stop marking direct UDP for the problematic Wi-Fi voice queue. Apply after endpoint/core/time fixes; test peer reboot, direct/relay changes and idle peers. Review `esp_netif_next_unsafe` synchronization before adopting all-interface advertisement. |
| [f9122f2](https://github.com/espkvm/microlink/commit/f9122f2289feac67b73e83d842b74cc2da0ac50e) | Deferred: upstream stack reductions require measurements of this fork's failure and audio/recording paths. |
| [bf2ac1e](https://github.com/espkvm/microlink/commit/bf2ac1eb0b17d9b17e6dc37d24209235cf2e6636) | Deferred: ACME/DNS support for firmware `c90457c`. Its DNS request can hold the Tailscale mutex for up to 60 seconds; assess responsiveness and TLS memory before adopting. |

## Original implementation order and acceptance gates

1. **Bridge recovery:** the `48c22d3` timeout/error-propagation subset. Preserve
   HDMI audio PLL recovery; prove reconnect and mode switching with the recorder.
2. **Small maintenance/UI fixes:** settings first tab, grouped fields and safe
   drafts, read-only card wording, then the still-planned `09e7920` real CMake
   paths. Test console save races and a recursive clone/source copy without the
   symlink. These do not require an upstream release merge.
3. **Video transport and decoding:** `5afe5a5`, `e14b7ca` SPS, and `f28e9ff`
   with matching console subsets. Establish a new latency baseline; test slow
   links, disconnects, HTTPS, logout, browser audio and AAC recording together.
   Keep recording quality independent of viewer-driven adaptation.
4. **Microlink reliability:** complete the older `42a0d1c` dependency work and
   the newer reconnect fixes. Test direct/relay connections, cold boot/time sync,
   peer restart, repeated TLS failure and sustained video/audio before changing
   UDP queues or declaring a remote-performance improvement.
5. **Memory and observability:** smaller upload allocations, heap/stack metrics,
   frame freshness and lazy WireGuard startup. Measure on this fork's workloads;
   do not reuse upstream stack sizes as AAC/LCD safety limits. Live log tailing
   can follow when its concurrency and failure handling are reviewed.

The earlier boot-order, Function EV pin and Wi-Fi allocation/power-saving
subsets are already implemented. Their remaining evidence gaps are retained
below; they are not new implementation tasks. Pending items above are a proposed
sequence, not an instruction to merge, flash or publish.

## Reconciliation checks (2026-10-16)

- Inventory coverage check: all 17 commits in `ce519cc..4c62662` have full source
  links above. Git resolved 67 distinct firmware/console/Microlink commit
  references; relative documentation links and `git diff --check` passed.
- Isolated upstream SPS test: extracted `h264_sps.c`, `h264_sps.h` and
  `test/test_h264_sps.c` from `e14b7ca:components/video_pipeline/`, compiled with
  `cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g`, and ran
  successfully (`h264 sps: all passed`). This does not test integration with
  our asynchronous encoder or AAC recordings.
- Parsed `tc358743_edid_480p` from `1af4c1c`: 256 bytes, both 128-byte block
  checksums zero, CTA LPCM descriptor `09 07 01` retained (two channels,
  16-bit, 32/44.1/48 kHz). This validates bytes, not HDMI negotiation; our
  configured I2S rate still needs to match the source.
- Extracted `coord_json_offset` from
  `de8c581:components/microlink/src/ml_coord.c` and compiled a bounded C harness
  with `-std=c11 -Wall -Wextra -Werror`. Six cases passed: `0x7b` in the length
  prefix, ordinary framed JSON, bare JSON, mismatched length, null/empty input
  and four-byte-only input. No integrated networking or fuzzing claim.
- Firmware/source and both submodule checkouts were unchanged. No build,
  hardware test, upstream merge, flash or push was performed for this review.
  Existing local roadmap edits were left intact.

## First implementation batch (2026-10-04)

This historical batch records the implemented subsets and their original test
limits. The reconciled decision table above reflects these implementations:

| Source subset | Implementation | Verification / remaining gaps |
| --- | --- | --- |
| `6f96380`: network initialization before C6 startup | [d2e8925](https://github.com/ZacharyLeahan/espkvm-xbox/commit/d2e8925b48280ab8bcd5715c62e64e19ec7a2ae8) — Adapted (partial) | Build and host tests pass; normal hardware boot succeeds. Missing-C6 fault injection and repeated warm restarts not tested. Updater changes omitted. |
| `cc3daee`: Function EV GPIO 6 reservation and SD power comment | [d42614a](https://github.com/ZacharyLeahan/espkvm-xbox/commit/d42614acdb3a24764e177b77c990ada318efcff0) — Adapted (partial) | Build and host tests pass; runtime SD power behavior unchanged. Other boards omitted. |
| `c5761dc`: disable station modem sleep | [9135969](https://github.com/ZacharyLeahan/espkvm-xbox/commit/91359693f01474014f34ed4c15926906708119f1) — Adapted (partial; memory work still planned) | Wi-Fi HTTP status responds after flashing; 720p60 input and about 28 LCD updates/s logged. No before/after latency claim, long soak or Tailscale validation yet. Memory/updater portions omitted. |

Hardware logs establish submission rate, not visual smoothness. On 2026-10-04,
the user confirmed smooth 720p gameplay and working tap-to-toggle after this
flash. A separate 20-second status collection completed with ten samples and
zero request failures; this is not a long soak test. No controller input work
is included. Each future implementation gets
its own commit URL and evidence; ignored/deferred entries need no implementation
commit. If testing rejects an adaptation, record that result explicitly.

## Wi-Fi SDIO allocation during HDMI audio bring-up (2026-10-16)

**Adapted (partial).** Implementation: [59da248](https://github.com/ZacharyLeahan/espkvm-xbox/commit/59da2481b330042d9d4edabdff87ce407c852693). Source:
[c5761dc7db6e798e54bbcea1bbcde468f23c3319](https://github.com/espkvm/espkvm/commit/c5761dc7db6e798e54bbcea1bbcde468f23c3319).
Concurrent browser video and PCM audio produced esp-hosted RX allocation
failures and audio send timeouts. The existing
`CONFIG_EH_HOST_PORT_DMA_PREFER_SPIRAM=y` only affects esp-hosted's aligned
allocator; its per-packet SDIO path calls the unaligned allocator instead.

- Added a link-time wrapper in `components/kvm_net/wifi_dma.c`, enabled only
  for Wi-Fi builds targeting P4 with SDIO and the existing PSRAM preference.
  It allocates cache-aligned DMA-capable PSRAM, rounds allocation sizes to a
  whole L2 cache line, and falls back to the original allocator on failure.
  Managed dependency files are unchanged.
- Local ESP-IDF 6.1 source checks confirmed that SDMMC accepts cache-aligned
  external buffers and performs cache synchronization. The hosted P4 setup
  already enables `SDMMC_HOST_FLAG_ALLOC_ALIGNED_BUF` for byte-mode tails.
- Build and link-map verification passed. Boot confirmed actual PSRAM allocation.
  A combined video/audio test captured 50.66 seconds before a send timeout;
  SDIO OOM logs were absent, with about 16.9 MB PSRAM available. This establishes
  allocation behavior, not complete transport reliability.
- Subsequently enabled `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP` in the Xbox
  profile to move TCP packet allocations away from internal RAM too, retaining
  internal fallback and the existing 8192-byte TCP buffer/window sizes.
  A final 60.005-second capture with browser video active stayed connected,
  preserving 59.64 seconds of non-silent PCM with brief recovered capture gaps.
  No SDIO OOM messages appeared in that boot; a combined-load sample showed
  about 75 KB internal free memory and 16.9 MB PSRAM free. The browser also
  showed live audio activity. Longer endurance, HTTPS and physical delay are
  not established. The co-processor updater remains omitted; C6 firmware has
  not been changed. Audio-enabled and disabled builds pass.

## TC358743 HDMI audio bring-up (2026-10-16)

**Adapted (partial).** Implementation: [ca27f55](https://github.com/ZacharyLeahan/espkvm-xbox/commit/ca27f5576918cc26196593782423d783509699e1). The source is Linux's original TC358743
driver addition, [d32d98642de66048f9534a05f3641558e811bbc9](https://github.com/torvalds/linux/commit/d32d98642de66048f9534a05f3641558e811bbc9).
The relevant behavior was checked against both that pinned revision and the
current driver. This is a separate register-level adaptation, not adoption of
the Linux driver or a change to the ESP-KVM inventory checkpoint above.

| Source subset | Adaptation scope | Verification / remaining gaps |
| --- | --- | --- |
| [Audio register definitions and encodings](https://github.com/torvalds/linux/blob/d32d98642de66048f9534a05f3641558e811bbc9/drivers/media/i2c/tc358743_regs.h#L506), used by [set_hdmi_audio](https://github.com/torvalds/linux/blob/d32d98642de66048f9534a05f3641558e811bbc9/drivers/media/i2c/tc358743.c#L756) | Correct the inherited audio register addresses, sample-frequency/CTS adjustment masks, and buffer/divider delay encodings. Preserve the existing ESP-IDF bridge initialization and CSI path. | Firmware builds and boots on Function EV with C790. Readbacks show HDMI audio packets and a 48 kHz frequency code, but these checks alone did not produce I2S samples. |
| [ANA_CTL definitions](https://github.com/torvalds/linux/blob/d32d98642de66048f9534a05f3641558e811bbc9/drivers/media/i2c/tc358743_regs.h#L396) and [HDMI transition handling](https://github.com/torvalds/linux/blob/d32d98642de66048f9534a05f3641558e811bbc9/drivers/media/i2c/tc358743.c#L1065) | Restore ANA_CTL (0x8545) to 0x31 when HDMI mode is detected, enabling the audio PLL and its normal charge-pump setting. Linux performs this in its HDMI interrupt handler because DVI mode resets the register. Our implementation checks it through the existing timing polling path, including recovery after mode changes; Linux interrupt/V4L2 infrastructure is omitted. | After this fix, the unchanged cable and GPIO assignment delivered a valid 5.4-second capture: stereo 48 kHz, 16-bit PCM, 259,200 frames, nonzero peak 1,194 and RMS 118.61 in signed-16-bit units. Artifact: /tmp/espkvm-pll-scheduled.wav. This establishes actual audio capture, not merely source detection. The subsequent Wi-Fi/DMA allocation failure leaves sustained transport reliability unresolved at this checkpoint. |

The capture was verified programmatically. Listening quality, measured audio
delay, audio/video synchronization, simultaneous browser audio/video endurance,
and repeated HDMI reconnect recovery are not established by this short capture.
Transport tuning is separate from the bridge register fix and needs its own
results. No wiring change or level shifter was needed to obtain these samples.
The implementation commit carries the Linux source URL in its `Adapted-from:`
trailer. Subsequent transport tuning and combined-load checks are recorded in
the [audio validation report](AUDIO-VALIDATION-2026-10-16.md). That report separates
measured sample activity from subjective sound quality and physical latency.

## Audio feature and review bundle (2026-10-16)

- Firmware/API: [d6daefe](https://github.com/ZacharyLeahan/espkvm-xbox/commit/d6daefe0492e542ebdf2f9ca5939cb788520135a).
  Opt-in slave I2S capture, authenticated single-listener PCM, freshness/activity
  diagnostics, and the bounded WAV observation tool. This is fork implementation
  work, separate from the two adaptations above.
- Session lifetime: [a98d060](https://github.com/ZacharyLeahan/espkvm-xbox/commit/a98d06062166d595988332a546c49859974196b0).
  Bind audio to its originating cookie session so logout/password changes revoke
  it. Eight host lifecycle cases passed; authenticated hardware logout is untested.
- Console: [03f0a71](https://github.com/ZacharyLeahan/espkvm-console-xbox/commit/03f0a719ae272c2fd5ed2ed60000c26db68fde50).
  PCM framing, playback queue bounds, cancellation and bounded reconnect tests.
  The firmware submodule pin and bundled viewer use this published revision.
- API skill contract: [002ce16](https://github.com/ZacharyLeahan/espkvm-skills-xbox/commit/002ce16977254490d7cc4a74232ade0738fab180).
  The upstream `espkvm/mcp` repository returned HTTP 404; no MCP server change or
  validation is claimed. The standalone observation tool remains available.
- [Wiring and usage](HDMI-AUDIO.md), [reproduction and measured evidence](AUDIO-VALIDATION-2026-10-16.md).
  Captured movie audio and raw private device logs are kept local. Published
  evidence contains metrics, source references, public configuration and limits.

`main` is this fork's maintained Xbox branch. Reviewable contributions can be
prepared as focused branches containing the relevant commits; retaining a
permanent `xbox-experiments` branch is not required for upstream review.


## Shared HDMI capture and microSD AAC (2026-10-16)

Implementation and tests: [1e83a01](https://github.com/ZacharyLeahan/espkvm-xbox/commit/1e83a01).

This fork adds an independent I2S producer with bounded browser and recorder
queues, recorder-side AAC accumulation on the video clock, AAC in MPEG-TS, and
AAC-preserving MP4 remuxing. Network disconnect and backpressure do not own the
recorder's capture lifetime. Host tests cover timeline loss/reset/splits and
container audio parsing; hardware results are recorded in [SD-RECORDING.md](SD-RECORDING.md).

The AAC codec is a managed Espressif dependency, not copied implementation:

- P4 rev >=3: `esp_audio_codec` 2.6.2, source package commit
  [f9110cbb](https://github.com/espressif/esp-adf-libs/tree/f9110cbb471b4017b0faf9bd48fcac84c61ecb6d/esp_audio_codec).
- Earlier P4: 2.5.0, source package commit
  [3bb83597](https://github.com/espressif/esp-adf-libs/tree/3bb83597d07b604e1ab5b78dd4370a28d6fa802d/esp_audio_codec).
- Dependency selection is conditional on audio being compiled. The newer
  optimized library cannot be used on older P4 silicon. The package's
  [Espressif-only license](https://components.espressif.com/components/espressif/esp_audio_codec/versions/2.6.2/license)
  and [VisualOn attribution](https://docs.espressif.com/projects/esp-adf/en/latest/COPYRIGHT.html)
  are acknowledged in NOTICE. This is a dependency integration, not an adaptation
  of upstream recorder implementation commits.

API companion updates are in [skills commit 85ea5ac](https://github.com/ZacharyLeahan/espkvm-skills-xbox/commit/85ea5ac).
The viewer reports audio recording in [console commit 60c39f0](https://github.com/ZacharyLeahan/espkvm-console-xbox/commit/60c39f0). `espkvm/mcp` was inaccessible
(HTTP 404), so its recorder/audio contract still needs a maintainer update.
