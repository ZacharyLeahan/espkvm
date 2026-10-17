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

## Current reconciliation (2026-10-16)

Fetched firmware `upstream/main`, console `upstream/main` and Microlink
`upstream/idf6` again. Their tips remain `4c62662`, `48c58b0` and `5129dc2`
respectively: **no new upstream commits since the inventory checkpoint**.
Reviewed local firmware through `958fa09`, console through `a258024` and
Microlink through `d3210a0`. The checkpoint above describes the original
baseline; the table below is the current implementation status. Historical
inventory decisions farther down retain the original rationale and source URLs.

| Area | Current implementation | Remaining acceptance work |
| --- | --- | --- |
| HDMI bridge recovery | Adapted in [8d94230](https://github.com/ZacharyLeahan/espkvm-xbox/commit/8d942304e8631b0ca37bf34e76d7e7e5fab61979): bounded I2C, failed reads and capped boot recovery, preserving HDMI audio PLL corrections. | Host failure tests/build passed previously; physical reconnects and oversized-to-supported mode changes with SD audio remain unverified. |
| Settings and build paths | Console [9aea143](https://github.com/ZacharyLeahan/espkvm-console-xbox/commit/9aea143beee957604e4fbf4aac7b5da28588ed07); firmware [daffe2c](https://github.com/ZacharyLeahan/espkvm-xbox/commit/daffe2c2f4872db3b61aa3be9a4163f4741d0541). Safe drafts, grouping, initial section, card wording and explicit component paths are implemented. | Console tests/typecheck and source-copy build passed previously; integrated browser checks and parent console pin remain. |
| Video delivery and recovery | SPS [1ba5d84](https://github.com/ZacharyLeahan/espkvm-xbox/commit/1ba5d84a6e3b534258109b1a3ef97baa7ca9f57b), ACK/keyframe handling [9dcc15d](https://github.com/ZacharyLeahan/espkvm-xbox/commit/9dcc15de36ce15d1243904a9046294d13a2e1efd), session transport [b63b60e](https://github.com/ZacharyLeahan/espkvm-xbox/commit/b63b60e41b92a3cafd1f4f47cb40637b93b2e55e), console [a258024](https://github.com/ZacharyLeahan/espkvm-console-xbox/commit/a258024c2ea4300ad5f0d1d3fe9da526191e1cac). | Host checks/build passed previously. HTTPS, slow viewers, simultaneous PCM/SD AAC and TS/MP4 compatibility need combined device validation. No measured latency improvement claimed. |
| Microlink reliability | Six source-attributed cherry-picks plus reconnect/path adaptation [d3210a0](https://github.com/ZacharyLeahan/microlink/commit/d3210a0523766d6917706aa643750082f61d49f8); exact sources and boundaries are recorded below. | Host checks/build passed previously; direct/relay, peer reboot, clock sync, TLS failures and parent dependency pin remain. |
| Memory and diagnostics | Upload fallback/lazy WireGuard [9a44661](https://github.com/ZacharyLeahan/espkvm-xbox/commit/9a44661e3406935de431ebf836695fe511b87d67), freshness [f723d14](https://github.com/ZacharyLeahan/espkvm-xbox/commit/f723d144f4fa5248a60695478f827b3d3242a531), heap/task endpoint [4ccb502](https://github.com/ZacharyLeahan/espkvm-xbox/commit/4ccb50280c8f92556d310efcd42825faa3463e04). | Host checks/build passed previously; measure snapshot pauses, upload behavior and memory under simultaneous video/audio/recording. API skill updated locally; upstream MCP remains an explicitly recorded access gap. |

These are local feature-branch implementations, not a completed upstream merge
or a verified release. Parent pins still reference console `60c39f0` and
Microlink `d6865e1`; dependency checkouts and the generated console bundle are
modified. Publication is not established by this reconciliation. An application
flash started during the earlier implementation work finished successfully
(hash verified in its existing log); that is installation evidence only, not
runtime acceptance. This ledger review initiated no additional flash or tests.

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
