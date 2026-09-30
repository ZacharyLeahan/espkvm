# Xbox capture investigation (2026-09-30)

Hardware: ESP32-P4 Function EV, chip rev 3.2, C790/TC358743, 594 Mb/s
CSI lanes. These results are separate from the rev 1.3 board benchmarks in
HARDWARE-NOTES.md. No stable automatic-resolution baseline is established.

## Observations

- Diagnostic build `4482f16a4` measured 300 completed capture callbacks in
  5007 ms at 1280x720 with zero viewers (about 59.9 captured frames/s).
  This measures capture, not encoded/delivered browser FPS. The next comparison
  is the game at 480p with viewers still disconnected.
- That comparison is now confirmed: 720x480, viewers=0, encodeUs=0,
  zero completed capture callbacks across repeated six-second samples. DMA
  offset stays at 688064/691200. Measured HDMI totals are 858x525 with
  active 720x480. This excludes active streaming/encoding load as the trigger.
  Raw DMA ctl0=00221b14 ctl1=000f87c0 cfg0=00000000 cfg1=22020084.

- Launcher 1280x720 capture has worked; switching back from the game restored
  capture without a board reset in the preceding test.
- Game HDMI is detected as 720x480 progressive at 60 Hz. SYS_STATUS=0x9f,
  TC CSI_ERR=0, and P4 CSI PHY/packet error registers are zero at sampled stalls.
- The game completes zero capture callbacks. This is upstream of the browser,
  JPEG encoder and Tailscale. FPS tuning cannot fix this capture stall.
- With 512-word DMA bursts, destination advances 688064 bytes into a
  691200-byte frame, then stops; CSI FIFO reaches depth 961 with threshold 960.
  The destination-register offset is diagnostic, not a guarantee that every
  preceding byte is valid image data.
- A static launcher can legitimately publish zero FPS with static-frame
  suppression enabled. Check capture callback progress, not publish FPS alone.

## Unsuccessful experiments (removed)

- DMA self-controlled completion instead of CSI-controlled completion: same
  stall. Raising FIFO threshold to 2040: still stalled, depth 1024, with an
  overrun indication. Restore the IDF threshold of 960.
- Matching both DMA burst sizes and the bridge burst size to 128 words,
  with explicit bridge block counts 674 and 675: no complete frames. In both
  cases the destination advanced 689088 bytes (2112 short). FIFO depth was
  128 for count 674, and 961 for count 675. The linked wrapper and bridge
  register readbacks confirmed the diagnostic was present.
- Earlier tests of two CSI lanes at 480p, continuous clock, a lower TC FIFO
  threshold, and post-start reconfiguration also did not fix capture.

The experimental DMA wrappers and threshold changes were removed. Existing
resolution detection and experimental bandwidth-based lane selection remain.
Do not describe the remaining lane selection as a demonstrated FIFO fix.
Next investigation should establish why the bridge/DMA stops short of frame
completion, preferably with transfer-request/packet-level evidence, before
further performance tuning or claiming a hardware throughput limit.

## RGB diagnostic breakthrough

Build `9f17e004c` enables `CONFIG_KVM_CAPTURE_DEBUG_RGB` in the private build's
sdkconfig, using the existing RGB888 profile and two capture buffers. The
option is disabled by default in source. Bandwidth-based lane selection chooses
two lanes for this RGB mode, versus one for the YUV mode (earlier two-lane YUV
testing also failed). This comparison narrows the fault, but does not prove
which individual format/packing/timing difference is responsible.

- At game input 720x480, repeated samples show 299-300 completed frames per
  5000 ms with no viewers. No capture timeout in these samples.
- A JPEG request succeeded in about 1.1 seconds. The image visibly shows the
  Guilty Gear Isuka title screen with readable text.
- Restored `vid_codec=0` (MJPEG). A bounded local `/stream` test returned HTTP
  200 and 6,346,206 bytes in 20 seconds; timeout was the intentional test limit.
  During it, status reported 8.99 published FPS, 0.99 skipped FPS, 2174 kbps,
  encodeUs=5820, while capture remained about 60 FPS. This is not a measured
  maximum or a long-term stability claim.
- An SDIO memory-pool OOM/end event occurred earlier around the single-image
  test; network robustness is still a separate outstanding issue.

The RGB diagnostic remains flashed for the next dashboard/game round trip.
Do not claim automatic transitions or a final performance baseline until both
directions and sustained browser delivery are verified on this build.

Returning to the dashboard on the 594-Mb/s RGB build was NOT successful:
1280x720p60 was detected, but `tooFast=true` and zero capture completions.
RGB active payload is 1.327 Gb/s, above two 594-Mb/s lanes' 1.188 Gb/s.
A snapshot request can still return the stored game image; that does not
demonstrate dashboard capture. Next diagnostic uses the repository's default
972-Mb/s lane rate and fixes RGB diagnostics at two lanes to avoid changing
lane count at the same time as the source resolution.

Build `c2ccf0051` (RGB, fixed two lanes, 972 Mb/s) boots at dashboard 1280x720
and completes 299-300 capture frames per 5 seconds; `tooFast=false`. A snapshot
request received headers but zero image bytes and timed out, accompanied by an
SDIO RX mempool OOM/end event. The subsequent paced `/stream` test succeeded:
HTTP 200, 585861 bytes, four complete JPEGs over a bounded 15-second test on the
static dashboard. This is not a moving-video FPS benchmark. Capture stayed at
about 60 FPS with the stream connected. Next: switch back to the game on this
same build to validate 480p at 972 Mb/s, then repeat the return transition.

Dashboard-to-game transition on `c2ccf0051` succeeded without reset:
HDMI returned at 720x480p60, followed by repeated 300 completions/5000 ms.
The local 20-second stream test received HTTP 200 and 7273207 bytes containing
142 complete JPEGs plus one partial JPEG at cutoff. Mid-test status reported
9.16 published FPS, 3850 kbps and encodeUs=6029. An SDIO TX mempool OOM began
near the end of the test; capture still completed about 60 FPS afterward.
This confirms capture recovery, NOT stable maximum network throughput. Keep
the current 10-FPS limit until the remaining transport stalls are addressed.
