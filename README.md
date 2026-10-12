# ESP-KVM for the Original Xbox

This is an **original Xbox-specific fork of [espkvm/espkvm](https://github.com/espkvm/espkvm)**: a small network-connected video capture, touchscreen, recorder, and controller bridge for a real Xbox. The Xbox work is now merged into [`main`](https://github.com/ZacharyLeahan/espkvm/tree/main).

**We can watch the real Xbox, play with a Bluetooth Series controller, and send controller sequences over the network. An agent has successfully triggered kickflips and a kickflip → manual → kickflip sequence on the console, with the player confirming the results.** This is working hardware, not emulation; repeatable autonomous testing is still the goal, not a finished feature.

## What works today

- **Local video:** tested 480p and 720p inputs, with smooth, playable LCD preview. The measured 720p preview reached **28-29 updates/sec**; it is scaled to 640×360 on the 640×480 screen, not native 720p. Browser streaming is slower; see measurements below.
- **Xbox controller input:** the ESP's DEVICE USB port emulates an original Xbox controller. Browser keys, proportional touchscreen sticks, and a Bluetooth Xbox Series controller can drive it. All Bluetooth buttons were operator-confirmed in Xbox Controller Tool.
- **Touchscreen UI:** cycle between stats, controller controls, Bluetooth setup, a minimal bottom-edge input-history strip, and a clean screen. Hiding the overlay does not disconnect Bluetooth. History shows ordered presses and chords, rather than just the latest button.
- **Gameplay recording:** touchscreen Record/Stop, elapsed timer, and microSD video for later MP4/GIF export. A simultaneous 480p trial measured about **26 fps recording and 26-27 LCD updates/sec**, with zero recorder-reported drops in that trial.
- **Developer/agent access:** LAN/Tailscale viewing, a read-only evidence collector, and bounded network button sequences with automatic release. The trick demos used ad-hoc commands, not a shipped named-macro library or autonomous test runner.

Our three main hardware purchases totaled **$220.03**, or **$226.28 including the eBay Xbox controller-port adapter** (other cables/accessories excluded). [Parts and historical prices](#our-hardware) · [Controller setup](docs/XBOX-REMOTE-CONTROLLER.md) · [Recording](docs/SD-RECORDING.md) · [Testing tools](docs/XBOX-TESTING.md).

## Why this fork

The goal is **repeatable homebrew testing on a real original Xbox**, by humans
and eventually coding agents: build, deploy, launch, observe fresh video, send
controller inputs, and save pass/fail evidence. This complements emulator-based
development with checks on the actual console; it does not replace an emulator
or debugger.

- **Works today:** HDMI capture, local touchscreen preview, network viewing,
  a [read-only test-evidence collector](docs/XBOX-TESTING.md), and experimental
  original-Xbox USB controller emulation from browser keys, touchscreen controls,
  or a Bluetooth Series controller. The operator confirmed all Bluetooth buttons
  working in Xbox Controller Tool on 2026-10-11.
- **Next milestone:** repeatable automated menu tests and long-session input
  validation. Automated deployment/launch and reliable reset recovery are not
  implemented. A controller reset combination cannot recover every hard hang.
- **Tooling direction:** a small CLI first, agent workflow instructions next,
  and an optional MCP wrapper over the same operations. Tests should use bounded
  button presses, automatic release, fresh-frame checks, timeouts, and recorded
  build IDs. A unified controller CLI/MCP interface is still a roadmap item;
  the current input path uses the existing WebSocket transport.

Video and input can remain independent: a future test rig could use direct USB
HDMI capture for higher-rate observation while keeping the ESP for controller
input. That alternative has not been validated here. The first end-to-end proof
will be a repeatable real-hardware menu test, not a claim of autonomous debugging.

## Local LCD

- Three framebuffers and packed CPU scaling improve the measured 720p preview from about 20 to 28-29 updates/sec. Hardware PPA scaling stays disabled because it produced visible tearing.
- The extra buffer uses 900 KiB of PSRAM, leaving about 0.5 MB free in our test. This is a measured tradeoff for this setup, not a universal maximum-performance claim.
- With no remote viewer, resources favor the LCD. Connecting a KVM viewer prioritizes network video and reduces the LCD to **at most 1 fps**; disconnecting restores local performance automatically.
- A local-only developer strip shows capture freshness, HDMI mode/Hz, LCD updates/sec, and stream-consumer count/priority. Signal loss or stale capture shows a black status slate. Disable `KVM_DSI_STATUS_STRIP` for a clean LCD; browser video stays overlay-free.
- The **Waveshare 3.5-inch DSI LCD (E)** is not listed as supported/tested by Waveshare's ESP32 driver. We got live HDMI working in this fork and shared our findings on [Waveshare issue #184](https://github.com/waveshareteam/Waveshare-ESP32-components/issues/184#issuecomment-5973339430). This is a community implementation, not official vendor support or a confirmed upstream fix.
- In the Xbox build, tap unused black-bar space to cycle hidden, stats, stats + controller, controller, Bluetooth setup, and bottom-edge inputs only (last two require Bluetooth enabled in the build). From setup, one tap leaves just the input strip; another hides everything. Bluetooth stays connected. Setup includes full history, optional timing, and Disconnect. Touchscreen sticks are proportional. Recording keeps Stop accessible.
- **Hardware lesson:** the operator resolved a repeated/rolling LCD image by reseating a loose ribbon. Power off before handling ribbons. This does not establish that every display failure has the same cause; extended soak and hot-plug testing remain to be done.

[LCD setup, timings, and test history](docs/DSI-PREVIEW.md).

## Video

- Tested **1280×720 at 60 Hz (dashboard and 720p game)** and **720×480 at 60 Hz (480p game)**, plus capture recovery after an Xbox reboot without resetting the ESP.
- Approximately **60 FPS capture**. A ten-minute local Wi-Fi test averaged **4.74 FPS MJPEG delivery with zero reconnects**, using a 6-FPS cap, JPEG quality 75, and two viewers. This does not mean zero frame gaps or 60-FPS browser video.
- Tuned for our hardware's capture bandwidth and Wi-Fi buffering: **RGB888, two CSI lanes at 972 Mb/s per lane**, and paced network writes.

Smaller per-connection TCP buffers improved Wi-Fi stability in our test. Longer runs, repeated mode changes, and remote Tailscale performance still need testing. This is not a promise of every resolution/FPS or a proven maximum-performance preset. [Measurements and limitations](docs/XBOX-CAPTURE-DEBUG.md).

**Experimental Xbox controller input works on physical hardware.** See [setup, mappings, validation, and limitations](docs/XBOX-REMOTE-CONTROLLER.md). Same-boot Bluetooth reconnection/retry logic exists but still needs reliability testing; bonds do not survive an ESP reboot. Rumble is not forwarded, and end-to-end latency has not been measured.

For homebrew testing, use the [read-only observation helper and regression checklist](docs/XBOX-TESTING.md) to record HDMI mode changes, timestamps and optional screenshots against your build. That helper remains observation-only; controller testing is separate.

[microSD recording and GIF workflow](docs/SD-RECORDING.md): the touchscreen
Record/Stop control keeps the LCD enabled using a 720p-max memory profile.
Simultaneous 480p recording/preview has been tested; simultaneous 720p performance
and long-session recording still need validation. GIF conversion runs on a computer.

## Xbox compatibility / recovery

Observed on our hardware with build `70b054e9f`, local Wi-Fi, a 6-FPS stream cap, and JPEG quality 75:

| Game / scenario | Detected HDMI input | Test duration | Streaming / recovery result |
| --- | --- | --- | --- |
| 480p game | 720×480p60 | 10 minutes | Zero stream reconnects or ESP resets; 4.74 delivered FPS average with two viewers. |
| Xbox reboot → dashboard | 1280×720p60 after boot | One reboot; recovery time not measured | Capture resumed without an ESP reset; browser recovery without reloading not independently verified. |
| 720p game | 1280×720p60 | Short check; duration not recorded | Capture about 60 FPS; device reported 5.83 published FPS. Long-run and transition recovery not yet verified. |

These are observations, not blanket compatibility guarantees. Zero reconnects does not mean zero frame gaps. [Detailed test notes](docs/XBOX-CAPTURE-DEBUG.md#sustained-wi-fi-investigation-2026-09-30).

## Our hardware

| Part | What it does | Bought from | Our order total (USD) |
| --- | --- | --- | ---: |
| Espressif ESP32-P4 Function EV board (ordered as P4X-Function-EV; P4 rev 3.2, C6 Wi-Fi) | Runs capture, LCD rendering, and the web KVM | AliExpress | $95.54 |
| Geekworm C790 HDMI-to-CSI-2 bridge (Toshiba TC358743) | Takes Xbox HDMI into the ESP's MIPI CAMERA connector | AliExpress | $85.32 |
| **Browser-capture hardware total** | | | **$180.86** |
| Waveshare 3.5-inch DSI LCD (E), 640×480 capacitive touch | Optional local HDMI preview via MIPI DISPLAY; tap-to-toggle stats | Amazon | $39.17 |
| **All three purchases** | | | **$220.03** |
| [Female USB to Xbox controller port adapter cable, 1st gen original Xbox, 70 cm](https://www.ebay.com/itm/147474591575) | Connects the original Xbox controller port to USB-A female for our emulated controller | eBay, seller `nextgendeals07` | $6.25 |
| **Including Xbox controller-port adapter** | | | **$226.28** |

Prices are historical order totals verified against the September 18 (AliExpress), September 30 (Amazon), and October 2 (eBay) 2026 confirmation emails, not current quotes or availability guarantees. The LCD's item price was $36.95 before the $39.17 order total. The eBay cable was $5.89 + $0.36 sales tax, with free shipping. Totals exclude the Xbox HDMI adapter, USB-A-to-USB-C data cable, microSD card, controller, power supply, other cables, and case.

Connect Xbox HDMI → C790 → ESP **MIPI CAMERA**, and ESP **MIPI DISPLAY** → LCD. Both ribbon connections must use the correct pinout/contact orientation; disconnect power before handling them. USB-Serial-JTAG is used for power/flashing. The LCD needs no HDMI splitter and is optional for browser capture.

For controller input: **Xbox controller port → eBay adapter's USB-A female → USB-A male to USB-C male data cable → ESP DEVICE port**. The eBay adapter is not a direct USB-C cable and does not itself translate controller protocols; our Xbox-mode firmware provides XID emulation. Do not connect this path to USB-Serial-JTAG. See [controller setup](docs/XBOX-REMOTE-CONTROLLER.md).

## Build and use

Follow the [Xbox build instructions](docs/XBOX-FORK.md). Optionally copy [`.env.example`](.env.example) to an ignored `.env` and fill in your Wi-Fi credentials; never publish firmware containing your password.

For this exact screen, use the [recommended LCD build](docs/DSI-PREVIEW.md#recommended-build) with `boards/funcev_dsi_live.defaults` (three buffers, CPU scaling). The browser-only build keeps the LCD disabled. Allow roughly 30 seconds for the initial checkerboard to give way to HDMI video.

**Security:** this experimental profile uses HTTP with no application login. Tailscale secures tailnet traffic, but the local-network interface is still unauthenticated. Use a trusted, restricted LAN and never port-forward it.

## Credits

This fork builds on substantial existing work. Included software and protocol
references are different things; these are the projects that helped us get here:

| Repository | Role in this project |
| --- | --- |
| [espkvm/espkvm](https://github.com/espkvm/espkvm) | The upstream firmware foundation: capture, networking, web KVM, recording, and USB infrastructure. |
| [jrowny/p4kvm](https://github.com/jrowny/p4kvm) | Inherited through ESP-KVM: TC358743 bring-up and CSI bridge programming that made capture possible. |
| [espkvm/console](https://github.com/espkvm/console) and [espkvm/microlink](https://github.com/espkvm/microlink) | Web console and native Tailscale foundations; this tree pins its console and [our microlink fork](https://github.com/ZacharyLeahan/microlink) as submodules. |
| [Ryzee119/ogx360](https://github.com/Ryzee119/ogx360) and [ogx360_t4](https://github.com/Ryzee119/ogx360_t4) | Original Xbox XID protocol/design references for our independent USB implementation; their implementation code was not incorporated. |
| [rear16/ESP32-XboxBLE](https://github.com/rear16/ESP32-XboxBLE) and [asukiaaa/esp32-client-for-xbox-controller-with-nim-ble](https://github.com/asukiaaa/esp32-client-for-xbox-controller-with-nim-ble) | Series controller report-mapping and NimBLE client/identity references. |
| [waveshareteam/Waveshare-ESP32-components](https://github.com/waveshareteam/Waveshare-ESP32-components) | Display-driver comparison and compatibility investigation; we reported our working 3.5-inch E findings in [issue #184](https://github.com/waveshareteam/Waveshare-ESP32-components/issues/184#issuecomment-5973339430). Not a vendor-supported configuration. |
| [torvalds/linux](https://github.com/torvalds/linux) and [raspberrypi/linux](https://github.com/raspberrypi/linux) | ICN6211 bridge registers, DSI timing, and panel/host behavior references during LCD bring-up. |
| [espressif/esp-idf](https://github.com/espressif/esp-idf), [esp-hosted-mcu](https://github.com/espressif/esp-hosted-mcu), and [esp-bsp](https://github.com/espressif/esp-bsp) | Platform, C6 connectivity, and GT911 touch reference. [esp-hosted-mcu issue #241](https://github.com/espressif/esp-hosted-mcu/issues/241) informed the BLE pairing crypto fix. |

The fork-specific work ties those foundations together for Xbox: adaptive
capture/LCD tuning, this panel's bring-up, touchscreen recording and overlays,
XID output, Bluetooth-to-XID translation, and input history. Detailed provenance
and testing are in the [controller notes](docs/XBOX-REMOTE-CONTROLLER.md) and
[LCD investigation](docs/DSI-PREVIEW.md).

We selectively review upstream instead of automatically syncing this specialized fork.
See the [upstream review ledger](docs/UPSTREAM-REVIEW.md) for decisions, source commits, and links to our adaptations.

Based on [ESP-KVM](https://github.com/espkvm/espkvm), itself built on [jrowny/p4kvm](https://github.com/jrowny/p4kvm). Original attribution is preserved in [NOTICE](NOTICE); licensed under [Apache-2.0](LICENSE).
