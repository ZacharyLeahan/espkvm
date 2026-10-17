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

- Inventory date: 2026-10-03.
- Shared base: [`77045e1`](https://github.com/espkvm/espkvm/commit/77045e1ccc336151ec7ef15049812523ec921c75).
- Last inventoried upstream tip: [`ce519cc`](https://github.com/espkvm/espkvm/commit/ce519ccb63cca1fb9f0ee8de6a670278838d5bb4) (v0.58.0).
- Coverage: decisions recorded for all 13 commits on 2026-10-03: **5 partial
  adaptations planned, 3 ignored, 5 deferred**. Selected relevant diffs were
  inspected; this is scope/prioritization, not a completed security audit or
  implementation sign-off. On 2026-10-04, three of those upstream commits had
  selected subsets implemented below; memory and dependency work remains planned.
- The inventory tip marks what we listed, not what we adopted or fully reviewed.

## Decisions

| Upstream commit / scope | Decision | Decision rationale / scope | Our implementation / verification |
| --- | --- | --- | --- |
| [5d18b5b](https://github.com/espkvm/espkvm/commit/5d18b5b08037c970d83e7947fce9dc9be349bc3f): FireBeetle Wi-Fi pins | Adopted (inherited from fork main) | Initially ignored as irrelevant to Function EV. Retained when reconciling the user-requested PR merge on 2026-10-11, because it already existed on this fork's main. Preserve both FireBeetle and Function EV GPIO 6 reservations; the added pull-up default applies only to FireBeetle. | See [fork PR #1](https://github.com/ZacharyLeahan/espkvm-xbox/pull/1) merge ancestry; no FireBeetle hardware validation. Function EV firmware rebuild passed before merge. |
| [c5761dc](https://github.com/espkvm/espkvm/commit/c5761dc7db6e798e54bbcea1bbcde468f23c3319): Wi-Fi memory, power saving, co-processor update support | Adapted (partial) | Wi-Fi power saving disabled; cache-aligned SDIO PSRAM allocation implemented for P4 SDIO builds with the existing PSRAM-preference option. The Xbox profile also prefers PSRAM for lwIP allocations. Keep our small TCP buffers. Ignore co-processor updater/UI additions and do not flash the C6. | Power-saving subset: `9135969`. SDIO/lwIP memory subsets: [59da248](https://github.com/ZacharyLeahan/espkvm-xbox/commit/59da2481b330042d9d4edabdff87ce407c852693). Combined capture passed with recovered gaps; see the 2026-10-16 record below. |
| [09e7920](https://github.com/espkvm/espkvm/commit/09e792014de0446bef92269ef681ed549f396aa6): archive builds, FireBeetle validation, README | Adapt planned (partial) | Take explicit CMake component paths to remove dependence on a preserved symlink. Ignore FireBeetle/marketing edits; defer release-archive packaging until we publish archives. | Not implemented; test recursive-clone build and source copy with submodule content but without the symlink. |
| [cc3daee](https://github.com/espkvm/espkvm/commit/cc3daee460ecea6a6fa35ac637a3abea39be7dd4): board schematic corrections | Adapt planned (partial) | Reserve Function EV GPIO 6 for co-processor IO2 and correct the SD_PWRn comment without changing SD power behavior. Ignore unrelated board changes. | Not implemented; check pin-conflict tests and unchanged Function EV runtime pins. |
| [338c8ce](https://github.com/espkvm/espkvm/commit/338c8ce17f1f89e65f0264e9dc0ca043c0376091): notifications, 2FA, RTC, update URLs | Deferred | No current need for RTC hardware, notifications or 2FA UX. Revisit when one becomes a fork requirement. Do not migrate this fork to upstream firmware manifests; our releases must preserve Xbox/LCD customizations. | No implementation planned now; this is not a declaration that authentication is unnecessary on untrusted LANs. |
| [42a0d1c](https://github.com/espkvm/espkvm/commit/42a0d1c836e5cd693275c6c938675acbdd986a7a): release and dependency pins | Adapt planned (partial) | Adapt the Microlink reliability fixes behind this dependency bump: handshake time, receive handling, endpoint validation, TLS cleanup, core locking and framed JSON parsing. Preserve quiet packet logs; do not import unrelated release/console changes. | Not implemented; review dependency commits individually, then test Tailscale direct/relay connections, reboot reconnection and repeated failures. |
| [5afe5a5](https://github.com/espkvm/espkvm/commit/5afe5a5888db1fea216a9d81babb2af5cc6d1c8c): H.264 Wi-Fi playback | Deferred | H.264 recovery is useful but our tested deployment is HTTP/MJPEG. Revisit before offering H.264, with both server and matching console changes plus secure-context requirements. | No implementation planned now. |
| [ab5e0e7](https://github.com/espkvm/espkvm/commit/ab5e0e720bf9bf87a9de4225e6d9a1ba4610ed0d): upstream flasher redirect | Ignored | Upstream website/flasher deployment is not our firmware or release destination. Design fork-specific publishing if needed later. | Not applicable |
| [7918023](https://github.com/espkvm/espkvm/commit/79180238746747056354a79e8338d07ae0e56d57): upstream firmware publishing | Ignored | Upstream firmware-host publishing workflow is not ours. Do not inherit deployment destinations or credentials assumptions. | Not applicable |
| [6f96380](https://github.com/espkvm/espkvm/commit/6f9638042615f01906c3569105d9b7022212798d): update checks and missing-C6 boot recovery | Adapt planned (partial) | Take network-stack initialization before optional C6 startup to avoid a boot assertion when the co-processor is unavailable. Defer redirect/certificate-bundle update changes until a fork-specific updater is designed. | Not implemented; test normal boot, warm restarts and simulated C6-start failure without changing hardware wiring. |
| [4565409](https://github.com/espkvm/espkvm/commit/45654092ff35b3055eef3df2861607bbe4ff3762): initial setup hotspot | Deferred | Automatic open setup-hotspot behavior is coupled to upstream onboarding. Keep our current saved-Wi-Fi/failover behavior; revisit as a deliberate credential-free first-run experience. | No implementation planned now; any future setup AP must restrict unauthenticated control. |
| [e5c109e](https://github.com/espkvm/espkvm/commit/e5c109e58d5d6d4eb91b219d4f0c88e32371793b): password/network onboarding | Deferred | Password-driven network selection does not fit our present profile. Revisit together with setup-hotspot/onboarding design, rather than importing auth and console changes piecemeal. | No implementation planned now. |
| [ce519cc](https://github.com/espkvm/espkvm/commit/ce519ccb63cca1fb9f0ee8de6a670278838d5bb4): CEC and pointer lock | Deferred | CEC and relative-mouse pointer lock are not the original-Xbox USB controller path. Revisit for a demonstrated HDMI-adapter/CEC use case or PC-target support; do not add extra bridge/task load now. | No implementation planned now. |

## Implementation order and acceptance gates

1. **Boot recovery** (`6f96380` subset): small, targeted resilience fix.
2. **Function EV pin reservation** (`cc3daee` subset): prevent GPIO misuse.
3. **Wi-Fi latency** (`c5761dc` power-saving subset): measure before/after;
   keep memory changes separate so regressions can be isolated.
4. **Microlink reliability** (`42a0d1c` dependency subset): inspect and record
   actual dependency source hashes when adapting; test clock synchronization,
   reconnect after reboot, direct/relay paths and long-running streams.
5. **Wi-Fi memory allocation** (`c5761dc` subset): adopt only if simultaneous
   LCD/network testing preserves headroom and avoids allocation failures.
   Three-buffer LCD mode previously left about 0.5 MB free PSRAM; moving more
   buffers there is a tradeoff, not a free improvement.
6. **Build portability** (`09e7920` subset): independent low-risk build cleanup.

## First implementation batch (2026-10-04)

The decision table above records the original scope; these implementation
records supersede its "Not implemented" status only for the named subsets:

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
- Console: [03f0a71](https://github.com/ZacharyLeahan/espkvm-console-xbox/commit/03f0a719ae272c2fd5ed2ed60000c26db68fde50).
  PCM framing, playback queue bounds, cancellation and bounded reconnect tests.
  The firmware submodule pin and bundled viewer use this published revision.
- API skill contract: [994c64a](https://github.com/ZacharyLeahan/espkvm-skills-xbox/commit/994c64a).
  The upstream `espkvm/mcp` repository returned HTTP 404; no MCP server change or
  validation is claimed. The standalone observation tool remains available.
- [Wiring and usage](HDMI-AUDIO.md), [reproduction and measured evidence](AUDIO-VALIDATION-2026-10-16.md).
  Captured movie audio and raw private device logs are kept local. Published
  evidence contains metrics, source references, public configuration and limits.

`main` is this fork's maintained Xbox branch. Reviewable contributions can be
prepared as focused branches containing the relevant commits; retaining a
permanent `xbox-experiments` branch is not required for upstream review.
