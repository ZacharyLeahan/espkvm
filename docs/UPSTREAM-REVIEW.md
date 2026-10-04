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
- Coverage: 13 commits after the shared base. All remain **Pending**; this is
  initial triage from commit/file inventories and selected networking diffs,
  not a completed code/security audit. None has been integrated by this review.
- The inventory tip marks what we listed, not what we adopted or fully reviewed.

## Review queue

| Upstream commit / scope | Decision | Initial recommendation / questions | Our implementation / verification |
| --- | --- | --- | --- |
| [5d18b5b](https://github.com/espkvm/espkvm/commit/5d18b5b08037c970d83e7947fce9dc9be349bc3f): FireBeetle Wi-Fi pins | Pending | Likely ignore board-specific changes; check shared pin reservations before deciding. | None |
| [c5761dc](https://github.com/espkvm/espkvm/commit/c5761dc7db6e798e54bbcea1bbcde468f23c3319): Wi-Fi memory, power saving, co-processor update support | Pending | Priority adaptation candidate: Function EV network fixes. Measure added PSRAM pressure with three LCD buffers. Do not bundle a C6 firmware update into the fix. | None |
| [09e7920](https://github.com/espkvm/espkvm/commit/09e792014de0446bef92269ef681ed549f396aa6): archive builds, FireBeetle validation, README | Pending | Consider source-archive build improvements separately; board/marketing changes are probably irrelevant. | None |
| [cc3daee](https://github.com/espkvm/espkvm/commit/cc3daee460ecea6a6fa35ac637a3abea39be7dd4): board schematic corrections | Pending | Check Function EV and shared pin changes; defer other boards. | None |
| [338c8ce](https://github.com/espkvm/espkvm/commit/338c8ce17f1f89e65f0264e9dc0ca043c0376091): notifications, 2FA, RTC, update URLs | Pending | Split by feature. No blanket import into our no-login profile; review shared settings and update behavior separately. | None |
| [42a0d1c](https://github.com/espkvm/espkvm/commit/42a0d1c836e5cd693275c6c938675acbdd986a7a): release and dependency pins | Pending | Priority review of Microlink fixes, not just the release title. Inspect each newly pinned dependency commit and preserve our logging changes where needed. | None |
| [5afe5a5](https://github.com/espkvm/espkvm/commit/5afe5a5888db1fea216a9d81babb2af5cc6d1c8c): H.264 Wi-Fi playback | Pending | Review if enabling H.264; current HTTP/MJPEG setup may not benefit. Includes console submodule changes. | None |
| [ab5e0e7](https://github.com/espkvm/espkvm/commit/ab5e0e720bf9bf87a9de4225e6d9a1ba4610ed0d): upstream flasher redirect | Pending | Likely ignore upstream hosting deployment; not our device firmware. | None |
| [7918023](https://github.com/espkvm/espkvm/commit/79180238746747056354a79e8338d07ae0e56d57): upstream firmware publishing | Pending | Likely ignore upstream hosting workflow; revisit only for our own release system. | None |
| [6f96380](https://github.com/espkvm/espkvm/commit/6f9638042615f01906c3569105d9b7022212798d): update checks and missing-C6 boot recovery | Pending | Priority adaptation candidate for boot recovery; review update/TLS changes separately and preserve the Xbox profile. | None |
| [4565409](https://github.com/espkvm/espkvm/commit/45654092ff35b3055eef3df2861607bbe4ff3762): initial setup hotspot | Pending | Review interaction with our saved Wi-Fi, failover and no-login behavior before considering. | None |
| [e5c109e](https://github.com/espkvm/espkvm/commit/e5c109e58d5d6d4eb91b219d4f0c88e32371793b): password/network onboarding | Pending | Likely defer password-driven UX; inspect shared server changes for relevant fixes. | None |
| [ce519cc](https://github.com/espkvm/espkvm/commit/ce519ccb63cca1fb9f0ee8de6a670278838d5bb4): CEC and pointer lock | Pending | CEC is not Xbox USB gamepad emulation. Assess any actual use separately; preserve capture-bridge changes and review console/HID dependencies. | None |

## Completed decisions

None yet. Move or summarize decided rows here as the review progresses. For an
ignored change, record why and any revisit condition; for an adapted change,
link our commit, specify the subset, and record validation. Do not use this
ledger's own commit as the implementation link.
