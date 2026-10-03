# Waveshare 3.5-inch DSI LCD (E): live Xbox preview verified

This is an **opt-in experiment**, not a supported display feature. The user
confirmed both the standalone yellow/black checkerboard and then the live
Xbox image on the physical LCD. Keep `CONFIG_KVM_DSI_PREVIEW`
disabled in normal builds. No touchscreen controls are implemented.

The verified checkpoint uses `boards/funcev_dsi_pll.defaults`: one lane at
576 Mb/s, continuous clock, 24 MHz RGB888, non-burst sync events, and no HS
EoTP. It also includes ICN6211 PLL writes and a bounded command-mode readback
pause after ten seconds. Preserve this complete sequence when reproducing
the test; the necessary subset has not yet been isolated. The bridge's PLL
registers reverted by the later readback, so successful output does not prove
those writes alone fixed it. This standalone profile disables CSI capture.

For the live HDMI integration trial, replace the final defaults file with
`boards/funcev_dsi_live.defaults`. Use a fresh build directory/configuration
so an older standalone setting does not override these defaults:

```sh
. tools/env.sh
idf.py -B build.dsi-live -D SDKCONFIG=build.dsi-live/sdkconfig \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;boards/funcev_p4.defaults;boards/funcev_xbox.defaults;boards/funcev_dsi_live.defaults' build
```

The first live trial retains the ten-second diagnostic pause and limits LCD
updates to 5 fps. These are bring-up settings, not performance claims.

The subsequent viewer-aware policy targets up to 30 LCD updates/s with no
encoded-frame consumers, bounded to 80% measured copy duty. When a remote
viewer (or another consumer counted by the frame store) connects, encoding
runs first and the LCD drops to at most 1 update/s and 5% copy duty. Thermal
limits can reduce these further. Disconnecting restores the local budget
automatically. These are ceilings, not guaranteed rates; Wi-Fi and control
tasks retain time in LCD-only mode. HDMI capture and LCD scanout clocks stay
unchanged. Scaler source columns are cached instead of dividing per pixel.

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
