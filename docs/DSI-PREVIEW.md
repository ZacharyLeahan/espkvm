# Waveshare 3.5-inch DSI LCD (E): live Xbox preview verified

## Recommended local LCD configuration (2026-10-03)

`CONFIG_KVM_DSI_PREVIEW_BUFFERS=3` lets the CPU prepare a spare frame while
the previous display handoff completes. Each retired buffer retains its own
DMA-completion deadline; the renderer never reuses the selected display
buffer. The live profile now selects three buffers and disables PPA. Generic
Kconfig builds retain two buffers; LCD preview itself remains opt-in.

1280x720 -> 640x360 measurements: approximately 280-293 LCD updates per ten
seconds (28-29 fps), roughly 29 ms per packed CPU copy, and about 0.5 MB free
PSRAM. The user confirmed the 720p game looked "super smooth" with no remote
viewer. This is our recommended daily-use LCD configuration for this hardware,
not a guarantee of 30 fps, measured latency, or long-term stability.

The third buffer costs 921600 bytes. A short simultaneous MJPEG test reported
88 frames in 30 seconds, zero reconnects, and a 0.68-second longest gap at that
sample. It was stopped manually, not completed as a long soak. LCD updates
dropped below 1 fps as designed; after disconnecting, logs returned to 28-29
fps and the user confirmed smooth gameplay. Extended combined load, microSD,
Tailscale, and repeated mode-change testing of this memory tradeoff remain open.

This is **community support in this fork**, not official Waveshare support. The user
confirmed both the standalone yellow/black checkerboard and then the live
Xbox image on the physical LCD. Keep `CONFIG_KVM_DSI_PREVIEW`
disabled on builds without this LCD. Touch currently only toggles the stats;
Xbox controller buttons are not implemented.

We shared the working startup settings on
[Waveshare issue #184](https://github.com/waveshareteam/Waveshare-ESP32-components/issues/184#issuecomment-5973339430).
That report does not establish that Waveshare merged a fix or that the same
settings work on the issue author's different board.

The verified checkpoint uses `boards/funcev_dsi_pll.defaults`: one lane at
576 Mb/s, continuous clock, 24 MHz RGB888, non-burst sync events, and no HS
EoTP. It also includes ICN6211 PLL writes and a bounded command-mode readback
pause after ten seconds. Preserve this complete sequence when reproducing
the test; the necessary subset has not yet been isolated. The bridge's PLL
registers reverted by the later readback, so successful output does not prove
those writes alone fixed it. This standalone profile disables CSI capture.

## Recommended build

### Developer status strip

`CONFIG_KVM_DSI_STATUS_STRIP` defaults on when LCD preview is enabled. The
20-pixel top strip reads, for example, `LIVE 720p60Hz LCD 28.5 V:0 LOCAL`.
It sits in the 720p letterbox; full-height content loses the top 20 pixels to
the opaque strip. Disable the option in menuconfig for a clean local picture.
Browser video, recordings, and screenshots are untouched.

Tap anywhere to toggle the strip; hold/drag does not repeat. GT911 product ID
is checked before polling at 0x5d (or 0x14). Only ready contact reports are
acknowledged; no-data packets are not releases. The panel's existing reset and
configuration are preserved. A low-priority task polls independently of video;
the capture task alone changes framebuffer contents. Stats default on after
reboot, and hiding them does not stretch/crop video. No-signal/stale warnings
remain visible for safety. With a remote viewer, the visual change can wait
for the next low-rate LCD update. Repeated I2C failures stop touch polling
without stopping capture. Hardware tap validation is pending.

Protocol reference: [Espressif GT911 driver](https://github.com/espressif/esp-bsp/blob/master/components/lcd_touch/esp_lcd_touch_gt911/esp_lcd_touch_gt911.c).

- `LIVE` means the capture task observed advancing CSI completion counters
  within two seconds, not that the game is animating or responding to input.
- `STALE` means no recent completion (or none observed yet); `NO SIGNAL`
  means the HDMI bridge reports no lock. Unsupported input takes precedence
  over stale capture when the bridge reports a bandwidth limit.
- `LCD` measures successful live-frame submissions, not panel refresh, game
  FPS, or unique presented frames. HDMI Hz is the separate source rate.
- `V` is the existing frame-store consumer count: streams and other consumers
  can contribute. It is not a count of authenticated users or distinct people.
- `REMOTE` identifies remote-priority scheduling, not a person's identity.

Text/rate sampling runs approximately once per second; composition uses the
retired LCD buffer. During signal loss or stalled capture, the capture task
submits a black status slate instead of leaving an unlabelled frozen game.
Its normal two-second frame wait and recovery work can delay status updates;
this is not an independent watchdog for a hung capture task. State, mode, and
consumer changes appear in the existing timestamped system log, accessible
through the existing browser/API log facilities. No REST schema is changed.
A dedicated structured event-history API and client names are future work.

### Build commands

Use `boards/funcev_dsi_live.defaults`. Use a fresh build directory/configuration
so an older standalone setting does not override these defaults:

```sh
. tools/env.sh
idf.py -B build.dsi-live -D SDKCONFIG=build.dsi-live/sdkconfig \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;boards/funcev_p4.defaults;boards/funcev_xbox.defaults;boards/funcev_dsi_live.defaults' build
```

Configure Wi-Fi on the device; this command does not import `.env`. For an
existing configuration, explicitly select `CONFIG_KVM_DSI_PREVIEW_BUFFERS=3`
and disable `CONFIG_KVM_DSI_PREVIEW_PPA` in menuconfig: defaults do not override
saved sdkconfig choices. Flash using `idf.py -B build.dsi-live -p YOUR_PORT flash`.
Keep a known-good two-buffer build for rollback. Never publish local firmware
with embedded credentials. Startup still includes the verified diagnostic
pause; allow about 30 seconds for live video.

## Automatic CPU scaling paths

RGB888 1280-to-640 uses the existing packed 2:1 scaler. RGB888 720-to-640
(480p) now copies eight adjacent pixels out of every nine, preserving the
general nearest-neighbor scaler's exact output. Selection follows each frame's
dimensions, not a persistent 480p/720p setting. Vertical sampling, aspect ratio,
buffer handoff, and remote priority are unchanged. BGR/YUV and other ratios
retain the general path. Both specialized paths use the existing bounded 90%
local copy budget; this is not a guaranteed frame rate.

Initial hardware sample on 2026-10-03: 720x480p60 input measured 297 LCD
updates in 10014 ms (29.7 updates/s), with about 19.9 ms frame preparation,
versus roughly 20 updates/s and 30 ms on the general path. This includes
the developer strip, three buffers, no remote viewers, and PPA disabled.
Later samples reached 300 updates per 10009 ms. The user confirmed smooth
480p gameplay and then smooth 720p after switching back, validating automatic
path selection on this setup. The 2:1 scaler's reference test also passes.

The 9:8 path is tested against the reference for output lengths 0-640,
randomized pixels, tails, and unaligned buffers with ASan/UBSan:

```sh
cc -O2 -Wall -Wextra -fsanitize=address,undefined tools/test_lcd_rgb_9to8.c -o /tmp/test-lcd-rgb-9to8
/tmp/test-lcd-rgb-9to8
```

## Earlier performance trials (historical)

The following measurements describe earlier builds, not the recommended
three-buffer profile above.

The first live trial retains the ten-second diagnostic pause and limits LCD
updates to 5 fps. These are bring-up settings, not performance claims.

The subsequent viewer-aware policy targets up to 60 LCD updates/s with no
encoded-frame consumers, bounded to 80% measured general CPU-copy duty or 90%
for the packed RGB half-scaler and experimental hardware scaler. When a remote
viewer (or another consumer counted by the frame store) connects, encoding
runs first and the LCD drops to at most 1 update/s and 5% copy duty. Thermal
limits can reduce these further. Disconnecting restores the local budget
automatically. These are ceilings, not guaranteed rates; Wi-Fi and control
tasks retain time in LCD-only mode. HDMI capture and LCD scanout clocks stay
unchanged. Scaler source columns are cached instead of dividing per pixel.

With `CONFIG_KVM_DSI_PREVIEW_PPA=y`, exact RGB scale ratios representable in sixteenths can use the ESP PPA;
other ratios and YUV retain CPU scaling. See Espressif's
[PPA documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/peripherals/ppa.html).
The driver invalidates the DMA destination. CPU-written letterboxing is
flushed on geometry changes. Two DSI buffers separate rendering from scanout;
after submission the previous buffer is not reused until a DMA completion
has selected the new buffer. If the callback runs on another core, a second
completion is required to cover an ISR already in flight at submission.
At 1280x720 -> 640x360, measurements were about 27.7 ms per update and
29.4 updates/s with the 90% budget. At 80% the same hardware path delivered
about 20 updates/s because it missed the next available input frame.
The 60 fps ceiling is not a measured achievement. The user reported horizontal
splits at 29.4 fps with one buffer, and still saw splits at about 20 fps with
two buffers. The CPU-only two-buffer control measured about 15 fps and was
visually confirmed smooth. Shortening the same-core handoff wait recovered
about 20 fps at 720p, and a subsequent 480p test also measured about 20 fps.
PPA remains disabled by default; the higher-rate PPA trial is not a clean
video success.

The CPU now has an exact 2:1 RGB888 packed-copy path for 720p. Its byte output
was checked against nearest-neighbor reference output for widths 1-640,
unaligned pointers, and randomized pixels under AddressSanitizer and UBSan:

```sh
cc -O2 -Wall -Wextra -fsanitize=address,undefined tools/test_lcd_rgb_half.c -o /tmp/test-lcd-rgb-half
/tmp/test-lcd-rgb-half
```

After returning to 720p, the packed CPU path measured about 29-30 ms per copy
and 20 LCD updates/s. Raising its local work budget from 80% to 90% did not
raise measured output: double-buffer handoff still limits presentation.
The user confirmed the 480p game looked good; visual confirmation of this
latest 720p test remains separate from the counters. Do not advertise 30 or
60 fps for this CPU configuration.

At 720p the first policy test measured 199 LCD updates per roughly ten seconds
(about 20 fps) with no viewer. A 45.7-second MJPEG connection switched priority
to remote, kept the LCD below 1 fps, and completed without reconnects. That
scene delivered only nine JPEGs with skipped-frame counters active, so it
does not establish moving-video throughput. Disconnecting restored LCD
priority automatically. Local and remote limits are separate from HDMI's
60 Hz input and the panel's scanout rate.

The live test detected 1280x720p60 HDMI and fitted it into 640x360 on the
640x480 panel. Serial counters measured about 4.6 LCD updates/s without a
browser viewer and about 3 updates/s while MJPEG was also streaming. The
60 Hz input rate is not the LCD preview update rate. Initial combined testing
exposed a camera-task watchdog timeout during the diagnostic wait; servicing
the watchdog during that bounded wait fixed the observed reboot loop.
Touch, other input resolutions, sustained stability, and faster preview still
need verification. Earlier failed trials below are retained for reference.

A 60-second simultaneous local MJPEG request completed without reconnects
or a new watchdog reset, but delivered only 14 JPEGs (longest delivery gap
5.52 seconds). This is a connectivity check, not a smooth-video pass;
moving-scene testing is required before making performance claims.

## Hardware and known-good capture path

- ESP32-P4 Function EV board, Geekworm C790 HDMI-to-CSI bridge, and Waveshare
  **3.5-inch DSI LCD (E)**, 640×480. The purchase sources and historical totals
  are in the [README](../README.md#our-hardware).
- The ESP reports 1280×720p60 Xbox capture, and browser video works. This
  establishes that HDMI input and CSI capture are separate from the LCD issue.
- The LCD ribbon powers the panel; its touch controller acknowledges at I²C
  address `0x5d`. This does **not** prove that high-speed DSI video is aligned
  or decoded.

## What was tried

The test firmware writes a yellow/black checkerboard on startup and then
letterboxes captured frames into a 640×480 RGB888 framebuffer. The trials
below preceded the successful standalone checkerboard checkpoint above.

| DSI trial | LCD observation |
| --- | --- |
| 2 lanes at 500 Mb/s, including continuous clock | Color bars |
| 1 lane at 800 Mb/s, default burst mode | Color bars |
| 1 lane at 800 and 600 Mb/s, non-burst sync-pulse mode | Color bars |
| 1 lane at 800 and 600 Mb/s, non-burst sync-event mode | Color bars; arrangement changed |
| 1 lane at 600 Mb/s, non-burst sync events, HS EoTP disabled | Different colors/arrangement of bars; **still no preview image** |
| Xbox HDMI unplugged, then ESP reset | Bars still appeared; they are not the Xbox feed |
| Waveshare generic ESP driver startup order, then standalone checkerboard | Horizontal rainbow bars, never checkerboard; normal capture firmware restored afterward |

The ESP host's vertical-bar generator visibly changed the bars from horizontal
to vertical. That is evidence of a working DSI physical link, but not of the
framebuffer path. With host bars disabled, the LCD was blank at 800 Mb/s and at
600 Mb/s with a 20 MHz pixel clock. Starting with host bars for 500 ms and
then switching to the framebuffer also left it blank.

The DSI bridge's DMA-complete callback counted about 60 transfers per second,
and its FIFO held data. The DSI host nevertheless reported a payload underflow
at startup with Xbox capture active. A standalone framebuffer-only test (no
CSI capture) removed that underflow, but the LCD still went from rainbow bars
to blank without ever showing the checkerboard. Capture bandwidth is therefore
not sufficient to explain the display failure. Forcing continuous high-speed
DSI (no low-power blanking), still without CSI capture, made the rainbow bars
blink rather than displaying the checkerboard; it was reverted.

Increasing the ESP's L2 cache from 128 to 256 KiB caused the Tailscale network
task to fail allocation, so that local configuration was reverted. The device
was returned to its normal capture build after the standalone test.

## Basis for the settings

Waveshare's [product instructions](https://www.waveshare.com/wiki/3.5inch_DSI_LCD_%28E%29)
identify the panel and its Raspberry Pi setup. We inspected the manufacturer's
`Waveshare_35DSI.dtbo`: its default panel node specifies **one data lane**,
RGB888, `MODE_VIDEO`, 24 MHz, 640×480, horizontal porches 48/32/80, and vertical
porches 3/4/13. The current ESP trial matches those timing values. The
[Raspberry Pi VC4 DSI driver](https://github.com/raspberrypi/linux/blob/rpi-6.12.y/drivers/gpu/drm/vc4/vc4_dsi.c)
disables HS EoTP, so the last trial matches that detail too. A
[Raspberry Pi report](https://github.com/raspberrypi/linux/issues/7376)
documents that this model can display test bars when its DSI panel driver
fails to start, which is why bars cannot be treated as successful output.

## Diagnostic switches

`CONFIG_KVM_DSI_PREVIEW_HOST_PATTERN` enables the ESP's **vertical** DSI test
bars instead of the framebuffer. This distinguished an ESP-generated picture
from the LCD's existing **horizontal** bars. It is disabled by default.

`CONFIG_KVM_DSI_PREVIEW_STANDALONE` drives only the yellow/black checkerboard
framebuffer and intentionally skips HDMI/CSI capture. It is also disabled by
default; never leave it enabled in a normal KVM build. Bridge FIFO depth and
framebuffer DMA completion counts are logged for both modes.

The [Espressif board guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/user_guide.html)
calls for a reverse-contact LCD ribbon. The photos appear consistent with one,
and the [board schematic](https://dl.espressif.com/dl/schematics/esp32-p4-function-ev-board-schematics_v1.52.pdf)
matches Waveshare's published DSI connector pinout. This still does not
establish that high-speed DSI pairs are making reliable contact. Do not flip
or reseat either ribbon while powered.

The board in our photos carries an ICN6211 DSI-to-RGB bridge and a Nuvoton
controller; another developer has
[reported this model not working on ESP32-P4](https://github.com/waveshareteam/Waveshare-ESP32-components/issues/184).

## Source-level compatibility findings

Waveshare's [ESP32 display support list](https://github.com/waveshareteam/Waveshare-ESP32-components/blob/master/README.md)
does **not** list the 3.5-inch DSI LCD (E) among tested Raspberry-adapter
panels. The [open issue for this exact model](https://github.com/waveshareteam/Waveshare-ESP32-components/issues/184)
describes the same failure on a different ESP32-P4 board and requests driver
support. This does not prove that the panel cannot work on a P4, but it means
we do not have a vendor-validated configuration to port.

The manufacturer's Raspberry Pi instructions select
`dtoverlay=waveshare_35DSI,35E,dsi1`. We decoded that overlay: its 35E panel
defaults are one data lane, RGB888, only `MODE_VIDEO` (not burst or sync
pulse), 24 MHz, 640×480, and 48/32/80 and 3/4/13 porches. The current ESP
trial already matches these values. Raspberry Pi's
[generic `panel-dsi` driver](https://github.com/raspberrypi/linux/blob/rpi-6.12.y/drivers/gpu/drm/panel/panel-simple.c)
reads those properties and attaches the DSI host; it does not send a
panel-specific initialization command sequence. Thus we have not found a
missing Waveshare overlay parameter to copy.

The overlay describes a **generic DSI panel**, not an ICN6211 bridge device.
On a Pi, that path supplies timing and mode flags without the explicit ICN6211
register sequence found in
[Linux's standalone ICN6211 bridge driver](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/bridge/chipone-icn6211.c).
The LCD's onboard Nuvoton controller may initialize the bridge instead, but
its firmware and exact startup contract are not documented by this overlay.
Blindly copying the standalone bridge driver's register writes could conflict
with the Waveshare board's own controller. Also, a
[Raspberry Pi report for this exact LCD](https://github.com/raspberrypi/linux/issues/7376)
shows that rainbow bars can persist when the generic panel driver fails,
despite the hardware being otherwise functional.

Before another flash, a known-good Raspberry Pi test would establish whether
this LCD, bridge, and ribbon work with the manufacturer's supported setup.
If that succeeds, compare the Pi's actual DSI signal/clock behavior with the
ESP32-P4 host rather than guessing more nominal timing values. A validated
ESP32-P4 example for this exact model would provide the same missing evidence.

## Waveshare ESP-driver comparison

The [Waveshare `esp_lcd_dsi` component](https://github.com/waveshareteam/Waveshare-ESP32-components/tree/master/display/lcd/esp_lcd_dsi)
targets other Raspberry-adapter LCD models, **not** this 3.5-inch E. Its
startup code writes bridge-control registers `c0/c2/ac/ab/aa/ad` at I²C
address `0x45`, waits one second, and sends DCS `MADCTL`, `SLPOUT`, and
`DISPON` commands before enabling DPI video. The earlier ESP trial only wrote
`c0/c2/ac/ad`, skipped the wait and DCS commands, and performed `ad` after
panel initialization. The current opt-in trial mirrors Waveshare's full
order. The LCD still showed horizontal rainbow bars in the October 3 hardware
test, never the checkerboard. The `0x45` bridge address did not acknowledge on
this LCD, so the generic driver sequence is **not** a fix for the 3.5-inch E.
The normal Xbox capture build was reflashed and its local web page returned
HTTP 200 afterward. The preview remains disabled by default.

## ICN6211 register access and targeted BIST test

The ICN6211 Linux driver also supports DSI generic register reads. We added
an opt-in, standalone probe that reads registers `0x00`-`0x03` (vendor ID,
device ID, and version) with a 100 ms timeout. Unlike ESP-IDF's read helper,
it cannot wait forever if this panel does not answer. The probe returned
`c1 62 11 ff`, matching Linux's ICN6211 ID check. This confirms low-power
DSI register communication with the bridge. It does **not** confirm that the
bridge receives valid high-speed video packets. The read-only profile is
[`boards/funcev_dsi_probe.defaults`](../boards/funcev_dsi_probe.defaults).

The read-only dump showed the expected 640x480 geometry and 48/32/80,
3/4/13 porches already programmed by the LCD's own controller. Its BIST
register (`0x2a`) was `0x49` during the dump, but `0x01` on the next boot
before video startup. Bit 3 is the ICN6211's built-in test generator, so
[`boards/funcev_dsi_bist.defaults`](../boards/funcev_dsi_bist.defaults) tests
clearing **only** that volatile bit after checking the chip ID, then starts
the standalone checkerboard. This avoids overwriting the board controller's
other settings. The regular Xbox profile leaves both diagnostics off.

In hardware, the targeted test read `0x01` before video (the BIST enable bit
was already clear), wrote the same value, and verified it. The LCD still did
not show the checkerboard. A DSI register read after starting continuous DPI
video timed out and set a host error flag, so the diagnostic no longer tries
to read registers while video is running. Delaying the pre-video write by
500 ms also read `0x01`, but its verify read timed out during the controller's
startup transition. Neither test supports a BIST-bit-only fix. The ICN6211's
documented I2C addresses `0x2c` and `0x2d` did not acknowledge on the ESP's
shared bus; the `0x5d` touch controller did. The bridge may be on a private
bus behind the display's MCU.

The critical missing evidence is whether the ICN6211 accepts the ESP's
high-speed video packets after its MCU has finished setup. The read-only
register probe verifies only low-power commands, and DMA completion verifies
only that the ESP supplied frames to its own DSI host. Neither is a screen
image. A supported Raspberry Pi test or a verified ESP32-P4 example for this
exact panel would distinguish host protocol mismatch from LCD-side startup
behavior. After these diagnostics, the normal Xbox firmware was reflashed;
`GET http://192.168.1.169/` returned HTTP 200.
