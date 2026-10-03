# Waveshare 3.5-inch DSI LCD (E) preview: not working yet

This is an **opt-in experiment**, not a supported display feature. The Xbox
picture works in the browser, but this LCD currently shows its color-bar
pattern instead of the preview framebuffer. Keep `CONFIG_KVM_DSI_PREVIEW`
disabled in normal builds. No touchscreen controls are implemented.

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
letterboxes captured frames into a 640×480 RGB888 framebuffer. Neither the
checkerboard nor Xbox pixels have appeared on the LCD.

| DSI trial | LCD observation |
| --- | --- |
| 2 lanes at 500 Mb/s, including continuous clock | Color bars |
| 1 lane at 800 Mb/s, default burst mode | Color bars |
| 1 lane at 800 and 600 Mb/s, non-burst sync-pulse mode | Color bars |
| 1 lane at 800 and 600 Mb/s, non-burst sync-event mode | Color bars; arrangement changed |
| 1 lane at 600 Mb/s, non-burst sync events, HS EoTP disabled | Different colors/arrangement of bars; **still no preview image** |
| Xbox HDMI unplugged, then ESP reset | Bars still appeared; they are not the Xbox feed |

The DSI host reports its PHY active and no persistent error bits after startup.
The preview task updates its framebuffer, but the panel still does not show
those pixels. The changed bars only show that the panel reacts to link-setting
changes; they are not evidence of successful video decoding.

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

## Next diagnostic

Before more timing guesses, identify the display board's DSI bridge/controller
and its required reset, power, or initialization sequence, then compare that
with the ESP32-P4 Function EV DSI connector and Waveshare's panel schematic.
Do not flip or reseat either ribbon while powered. A Raspberry Pi known-good
test of this exact LCD would also separate panel hardware failure from an ESP
driver incompatibility.
