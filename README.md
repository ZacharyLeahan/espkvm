# ESP-KVM for the Original Xbox

This is an **original Xbox-specific fork of [espkvm/espkvm](https://github.com/espkvm/espkvm)**, streaming the console's HDMI video to a browser over the local network or Tailscale. Development lives on `xbox-experiments`.

## Video

- Tested **1280×720 at 60 Hz (dashboard)** and **720×480 at 60 Hz (game)**, including an automatic dashboard-to-game resolution change without resetting the ESP.
- Approximately **60 FPS capture**, with **7–10 FPS MJPEG delivery** in short local Wi-Fi tests using a 10-FPS stream cap. Capture rate is not browser frame rate.
- Tuned for our hardware's capture bandwidth and Wi-Fi buffering: **RGB888, two CSI lanes at 972 Mb/s per lane**, and paced network writes.

Video works, including over Wi-Fi, but this remains experimental: Wi-Fi buffer exhaustion and mode-switch reliability need more testing. This is not a promise of every resolution/FPS or a proven maximum-performance preset. [Measurements and limitations](docs/XBOX-CAPTURE-DEBUG.md).

**Xbox gamepad emulation is not implemented.** See the [controller proposal](docs/XBOX-REMOTE-CONTROLLER.md).

## Our hardware

| Part | Bought from | Our order total (USD) |
| --- | --- | ---: |
| Espressif ESP32-P4 Function EV board (ordered as P4X-Function-EV; P4 rev 3.2, C6 Wi-Fi) | AliExpress | $95.54 |
| Geekworm C790 HDMI-to-CSI-2 bridge (Toshiba TC358743) | AliExpress | $85.32 |
| **Combined** | | **$180.86** |

Prices are from our September 18, 2026 order-confirmation emails, not current quotes or bare component prices. This list does not include the Xbox's HDMI adapter, cables, or a case.

## Build and use

Follow the [Xbox build instructions](docs/XBOX-FORK.md). Optionally copy [`.env.example`](.env.example) to an ignored `.env` and fill in your Wi-Fi credentials; never publish firmware containing your password.

**Security:** this experimental profile uses HTTP with no application login. Tailscale secures tailnet traffic, but the local-network interface is still unauthenticated. Use a trusted, restricted LAN and never port-forward it.

## Credits

Based on [ESP-KVM](https://github.com/espkvm/espkvm), itself built on [jrowny/p4kvm](https://github.com/jrowny/p4kvm). Original attribution is preserved in [NOTICE](NOTICE); licensed under [Apache-2.0](LICENSE).
