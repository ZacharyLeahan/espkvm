<!-- SPDX-License-Identifier: Apache-2.0 -->

# Xbox fork history and measured results

These notes preserve the detailed hardware costs and earlier test observations
previously in the README. They describe particular builds and test conditions,
not guarantees for every board or the current firmware's memory usage. See the
[README](../README.md) for current scope and [HDMI-AUDIO.md](HDMI-AUDIO.md) for
the 2026-10-16 audio validation.

## Hardware purchases

| Part | Purpose | Bought from | Order total (USD) |
| --- | --- | --- | ---: |
| Espressif ESP32-P4 Function EV (ordered as P4X-Function-EV; P4 rev 3.2, C6 Wi-Fi) | Capture, LCD rendering, web KVM, and controller bridge | AliExpress | $95.54 |
| Geekworm C790 HDMI-to-CSI-2 bridge (Toshiba TC358743) | Xbox HDMI into the ESP's MIPI CAMERA connector | AliExpress | $85.32 |
| **Browser-capture hardware subtotal** | | | **$180.86** |
| Waveshare 3.5-inch DSI LCD (E), 640×480 capacitive touch | Optional local preview and touch controls through MIPI DISPLAY | Amazon | $39.17 |
| **Three main purchases** | | | **$220.03** |
| [Female USB to Xbox controller port adapter cable, first-generation Xbox, 70 cm](https://www.ebay.com/itm/147474591575) | Xbox controller port to USB-A female | eBay, seller `nextgendeals07` | $6.25 |
| **Including controller-port adapter** | | | **$226.28** |

These are historical order totals verified against September 18 (AliExpress),
September 30 (Amazon), and October 2 (eBay) 2026 confirmation emails, not current
quotes or availability guarantees. The LCD item was $36.95 before its $39.17
order total. The eBay cable was $5.89 plus $0.36 tax, with free shipping. Totals
exclude the Xbox HDMI adapter, USB-A-to-USB-C data cable, microSD card,
controller, power supply, other cables, and case.

The LCD is optional for browser capture and needs no HDMI splitter. The Xbox
controller-port adapter does not translate protocols: Xbox-mode firmware
implements XID over the ESP DEVICE USB connection. USB-Serial-JTAG is the
separate power/flashing connection. See [controller wiring](XBOX-REMOTE-CONTROLLER.md).

## Local LCD investigation

Three framebuffers and packed CPU scaling improved measured 720p preview from
about 20 to 28–29 LCD updates/sec. The 1280×720 picture scales to 640×360 on the
640×480 screen. Hardware PPA scaling remained disabled because it produced
visible tearing. The extra framebuffer consumes 900 KiB PSRAM; an earlier
configuration had about 0.5 MB free. That historical free-memory figure does
not describe the final audio build.

With no remote viewer, resources favor the LCD. A connected video viewer takes
priority and reduces LCD updates to at most 1 FPS; disconnecting restores local
performance. A local developer strip reports capture freshness, HDMI mode/Hz,
LCD updates/sec, and stream-consumer priority. Signal loss or stale capture
shows a black status slate. `KVM_DSI_STATUS_STRIP` controls the strip; browser
video stays free of the LCD overlay.

On the Xbox build, tapping unused black-bar space cycles through hidden,
stats, stats plus controller, controller, Bluetooth setup, and a minimal
bottom-edge input-history strip. The latter two need Bluetooth enabled in the
build. Leaving setup can retain just the input strip, then hide everything;
Bluetooth stays connected. Setup includes full history, optional timing, and
Disconnect. Touchscreen sticks are proportional and recording keeps Stop
accessible. History shows ordered presses and chords.

The Waveshare 3.5-inch DSI LCD (E) was not listed as supported/tested by its
ESP32 driver. This fork achieved live HDMI preview and shared the findings in
[Waveshare issue #184](https://github.com/waveshareteam/Waveshare-ESP32-components/issues/184#issuecomment-5973339430).
This is a community implementation, not vendor support or a confirmed upstream
fix. The operator once resolved a repeated/rolling image by reseating a loose
ribbon. That does not establish the cause of every later display failure;
extended soak and hot-plug testing remain open. Power off before handling
ribbons. See [LCD setup, timings, and test history](DSI-PREVIEW.md).

## Video compatibility and recovery

The following observations used build `70b054e9f`, local Wi-Fi, a 6-FPS stream
cap, and JPEG quality 75:

| Scenario | Detected HDMI input | Duration | Result |
| --- | --- | --- | --- |
| 480p game | 720×480p60 | Ten minutes | Zero stream reconnects or ESP resets; 4.74 delivered FPS average with two viewers. |
| Xbox reboot to dashboard | 1280×720p60 after boot | One reboot; recovery time not measured | Capture resumed without an ESP reset; browser recovery without reloading was not independently verified. |
| 720p game | 1280×720p60 | Short check; duration not recorded | About 60 capture FPS and 5.83 device-reported published FPS; long-run and transition recovery unverified. |

Zero reconnects does not mean zero frame gaps, and 60 capture FPS does not mean
60-FPS browser video. The hardware profile used RGB888, two CSI lanes at
972 Mb/s per lane, paced network writes, and smaller per-connection TCP buffers
that improved Wi-Fi stability in this trial. Repeated mode changes, longer
runs, and remote Tailscale performance still need validation. See the
[sustained Wi-Fi investigation](XBOX-CAPTURE-DEBUG.md#sustained-wi-fi-investigation-2026-09-30).

## Recording and controller checks

A simultaneous 480p trial measured about 26 recording FPS and 26–27 LCD
updates/sec, with zero recorder-reported drops. The touchscreen Record/Stop
control keeps the LCD enabled with a 720p-max memory profile. Simultaneous
720p recording/preview and long recordings still need validation. MP4/GIF
export runs on a computer; see [the recording workflow](SD-RECORDING.md).

The operator confirmed all Bluetooth Xbox Series controller buttons in Xbox
Controller Tool on 2026-10-11. Same-boot reconnection/retry logic is implemented
but needs reliability testing; bonds do not survive an ESP reboot. Rumble is
not forwarded and physical end-to-end input latency is unmeasured. An agent
performed kickflips and a kickflip → manual → kickflip sequence on the console,
confirmed by the player. These used ad-hoc bounded network commands, not a
shipped named-macro library or autonomous test runner.

## Development direction

The goal is a repeatable homebrew loop on the actual Xbox: build, deploy,
launch, observe fresh video/audio, send bounded controller inputs, and save
pass/fail evidence. Automated deployment/launch and reliable reset recovery
are not implemented; a controller reset combination cannot recover every hard
hang. The [observation helper](XBOX-TESTING.md) remains read-only; controller
testing uses the existing WebSocket transport.

The intended tooling progression is a small CLI, agent workflow instructions,
and an optional MCP wrapper over the same operations. A unified controller
CLI/MCP interface remains a roadmap item. Tests should retain automatic input
release, fresh-frame checks, timeouts, and recorded build IDs. Repeatable menu
tests and long-session input validation are the next milestones.

Video and input can remain independent: a future rig could combine direct
USB HDMI capture with ESP controller input for higher-rate observation. That
alternative has not been validated here. The real-console workflow complements
emulators and debuggers; it does not replace them.
