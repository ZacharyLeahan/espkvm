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
| [5d18b5b](https://github.com/espkvm/espkvm/commit/5d18b5b08037c970d83e7947fce9dc9be349bc3f): FireBeetle Wi-Fi pins | Ignored | FireBeetle-only pin/default changes do not affect our Function EV. Revisit if supporting that board. | Not applicable |
| [c5761dc](https://github.com/espkvm/espkvm/commit/c5761dc7db6e798e54bbcea1bbcde468f23c3319): Wi-Fi memory, power saving, co-processor update support | Adapt planned (partial) | Take Wi-Fi power-saving disablement first; separately adapt aligned SDIO/lwIP PSRAM allocation with measured memory headroom. Keep our small TCP buffers. Ignore co-processor updater/UI additions and do not flash the C6. | Not implemented; test Wi-Fi latency, heap/PSRAM, remote streaming and LCD together. |
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
| `6f96380`: network initialization before C6 startup | [d2e8925](https://github.com/ZacharyLeahan/espkvm/commit/d2e8925b48280ab8bcd5715c62e64e19ec7a2ae8) — Adapted (partial) | Build and host tests pass; normal hardware boot succeeds. Missing-C6 fault injection and repeated warm restarts not tested. Updater changes omitted. |
| `cc3daee`: Function EV GPIO 6 reservation and SD power comment | [d42614a](https://github.com/ZacharyLeahan/espkvm/commit/d42614acdb3a24764e177b77c990ada318efcff0) — Adapted (partial) | Build and host tests pass; runtime SD power behavior unchanged. Other boards omitted. |
| `c5761dc`: disable station modem sleep | [9135969](https://github.com/ZacharyLeahan/espkvm/commit/91359693f01474014f34ed4c15926906708119f1) — Adapted (partial; memory work still planned) | Wi-Fi HTTP status responds after flashing; 720p60 input and about 28 LCD updates/s logged. No before/after latency claim, long soak or Tailscale validation yet. Memory/updater portions omitted. |

Hardware logs establish submission rate, not visual smoothness. On 2026-10-04,
the user confirmed smooth 720p gameplay and working tap-to-toggle after this
flash. A separate 20-second status collection completed with ten samples and
zero request failures; this is not a long soak test. No controller input work
is included. Each future implementation gets
its own commit URL and evidence; ignored/deferred entries need no implementation
commit. If testing rejects an adaptation, record that result explicitly.
