# ESP-KVM for the Original Xbox

This is an **original Xbox-specific fork of [espkvm/espkvm](https://github.com/espkvm/espkvm)**, streaming the console's HDMI video to a browser over the local network or Tailscale. Development lives on `xbox-experiments`.

## Video

- Tested **1280×720 at 60 Hz (dashboard and 720p game)** and **720×480 at 60 Hz (480p game)**, plus capture recovery after an Xbox reboot without resetting the ESP.
- Approximately **60 FPS capture**. A ten-minute local Wi-Fi test averaged **4.74 FPS MJPEG delivery with zero reconnects**, using a 6-FPS cap, JPEG quality 75, and two viewers. This does not mean zero frame gaps or 60-FPS browser video.
- Tuned for our hardware's capture bandwidth and Wi-Fi buffering: **RGB888, two CSI lanes at 972 Mb/s per lane**, and paced network writes.

Smaller per-connection TCP buffers improved Wi-Fi stability in our test. Longer runs, repeated mode changes, and remote Tailscale performance still need testing. This is not a promise of every resolution/FPS or a proven maximum-performance preset. [Measurements and limitations](docs/XBOX-CAPTURE-DEBUG.md).

**Xbox gamepad emulation is not implemented.** See the [controller proposal](docs/XBOX-REMOTE-CONTROLLER.md).

## Xbox compatibility / recovery

Observed on our hardware with build `70b054e9f`, local Wi-Fi, a 6-FPS stream cap, and JPEG quality 75:

| Game / scenario | Detected HDMI input | Test duration | Streaming / recovery result |
| --- | --- | --- | --- |
| 480p game | 720×480p60 | 10 minutes | Zero stream reconnects or ESP resets; 4.74 delivered FPS average with two viewers. |
| Xbox reboot → dashboard | 1280×720p60 after boot | One reboot; recovery time not measured | Capture resumed without an ESP reset; browser recovery without reloading not independently verified. |
| 720p game | 1280×720p60 | Short check; duration not recorded | Capture about 60 FPS; device reported 5.83 published FPS. Long-run and transition recovery not yet verified. |

These are observations, not blanket compatibility guarantees. Zero reconnects does not mean zero frame gaps. [Detailed test notes](docs/XBOX-CAPTURE-DEBUG.md#sustained-wi-fi-investigation-2026-09-30).

## Our hardware

| Part | Bought from | Our order total (USD) |
| --- | --- | ---: |
| Espressif ESP32-P4 Function EV board (ordered as P4X-Function-EV; P4 rev 3.2, C6 Wi-Fi) | AliExpress | $95.54 |
| Geekworm C790 HDMI-to-CSI-2 bridge (Toshiba TC358743) | AliExpress | $85.32 |
| **Working browser-capture hardware total** | | **$180.86** |
| Waveshare 3.5-inch DSI LCD (E), 640×480 touch (optional; local video preview **not yet working**) | Amazon | $39.17 |
| **All three purchases** | | **$220.03** |

Prices are historical order totals from the September 18 (AliExpress) and September 30 (Amazon) 2026 confirmation emails, not current quotes or bare component prices. The LCD currently shows color bars, not the Xbox picture; it is **not** required for browser capture. [LCD diagnostic notes](docs/DSI-PREVIEW.md). This list does not include the Xbox's HDMI adapter, cables, or a case.

## Build and use

Follow the [Xbox build instructions](docs/XBOX-FORK.md). Optionally copy [`.env.example`](.env.example) to an ignored `.env` and fill in your Wi-Fi credentials; never publish firmware containing your password.

**Security:** this experimental profile uses HTTP with no application login. Tailscale secures tailnet traffic, but the local-network interface is still unauthenticated. Use a trusted, restricted LAN and never port-forward it.

## Credits

Based on [ESP-KVM](https://github.com/espkvm/espkvm), itself built on [jrowny/p4kvm](https://github.com/jrowny/p4kvm). Original attribution is preserved in [NOTICE](NOTICE); licensed under [Apache-2.0](LICENSE).
