# ESP-KVM for the Original Xbox

[espkvm-xbox](https://github.com/ZacharyLeahan/espkvm-xbox) is an original Xbox fork of [ESP-KVM](https://github.com/espkvm/espkvm): HDMI capture, browser viewing and audio, a local touchscreen, microSD video/audio recording, and controller input on a real console. The goal is repeatable homebrew testing by people and coding agents.

The maintained Xbox branch is [`main`](https://github.com/ZacharyLeahan/espkvm-xbox/tree/main). Feature branches hold work under review; the `upstream` remote tracks the original ESP-KVM project, including `upstream/main`. We review upstream changes selectively in the [adaptation ledger](docs/UPSTREAM-REVIEW.md).

## What works

- **Video:** 480p and 720p Xbox HDMI capture, LAN/Tailscale viewing, and optional 640×480 touchscreen preview. The 720p picture is scaled to 640×360 on the LCD.
- **Controller input:** the ESP's DEVICE USB port emulates an original Xbox controller, driven by browser keys, proportional touchscreen sticks, a Bluetooth Xbox Series controller, or bounded network input sequences with automatic release.
- **HDMI audio:** opt-in stereo PCM capture from the C790's separate I2S connector, browser playback with volume/meter and bounded reconnect, plus a tool that saves WAV evidence on a computer. The camera ribbon carries video only.
- **Touchscreen and recording:** stats, controller controls, Bluetooth setup, ordered input history, clean-screen mode, and Record/Stop to microSD with H.264 video and optional AAC stereo audio. MP4 export preserves sound; GIFs are silent.
- **Testing tools:** a read-only video evidence collector and bounded audio recording. An agent triggered kickflips and a kickflip → manual → kickflip sequence on the real Xbox; the player confirmed the results. Those demos used ad-hoc commands.

## Evidence and limits

These are observations on our hardware, under different test configurations:

| Check | Result |
| --- | --- |
| Local 720p preview | 28–29 LCD updates/sec with three buffers and CPU scaling. |
| Wi-Fi MJPEG video | Ten minutes, two viewers: 4.74 delivered FPS average, zero reconnects, 6-FPS cap and JPEG quality 75. Capture was about 60 FPS. |
| Simultaneous 480p recording/preview | About 26 recording FPS and 26–27 LCD updates/sec, zero recorder-reported drops in that trial. |
| Live HDMI audio with web video | A 60-second HTTP capture stayed connected and saved 59.64 seconds of non-silent 48 kHz stereo PCM; brief DMA overruns recovered. Real browser audio also ran beyond 74 seconds. |
| MicroSD video + audio | One-minute 720p clip decoded with stereo 48 kHz AAC; zero recorder audio/video queue drops, about 14.4 video FPS with simultaneous network audio. [Test details](docs/SD-RECORDING.md#hardware-verification-2026-10-16). |
| Bluetooth controller | All buttons operator-confirmed in Xbox Controller Tool on 2026-10-11. |

Audio is working, but brief gaps remain possible. Subjective sound quality, audio/video synchronization, and physical end-to-end latency are unverified; the browser's 43–44 ms queue measurement is only local buffering. There is **one network audio listener**: mute the viewer before using the WAV tool. The microSD recorder has its own audio subscription and can run alongside either. Stereo LPCM is required; Dolby/DTS passthrough is not decoded.

Connecting a remote video viewer reduces the LCD to at most 1 FPS; disconnecting restores local preview. Long sessions, repeated HDMI mode changes, remote Tailscale performance, and simultaneous 720p recording/preview need more validation. Bluetooth bonds do not survive an ESP reboot, reconnection needs reliability testing, and rumble is not forwarded.

Automated deployment/launch, reliable recovery from console hangs, named input macros, and an autonomous test runner remain future work. The current tools complement emulator/debugger workflows with real-console evidence.

[Audio measurements and configuration](docs/HDMI-AUDIO.md) · [Video investigation](docs/XBOX-CAPTURE-DEBUG.md) · [Historical tests and hardware costs](docs/FORK-HISTORY.md)

## Hardware and connections

Our setup uses an **ESP32-P4 Function EV board** (P4 rev 3.2, C6 Wi-Fi), **Geekworm C790** HDMI-to-CSI bridge, and optional **Waveshare 3.5-inch DSI LCD (E)**. The three purchases totaled $220.03, or $226.28 with the Xbox controller-port adapter; [historical order totals and exclusions](docs/FORK-HISTORY.md#hardware-purchases) are documented separately.

- **Video:** Xbox HDMI → C790 → ESP **MIPI CAMERA**. Optional ESP **MIPI DISPLAY** → LCD. Power off before handling ribbons and check contact orientation.
- **Audio:** C790 I2S → four ESP header connections. Use the [verified Function EV wiring and audio settings](docs/HDMI-AUDIO.md#header-cross-check-2026-10-16).
- **Controller:** Xbox controller port → female USB adapter → USB-A-to-USB-C data cable → ESP **DEVICE USB**. The firmware provides XID emulation; the adapter is just a cable.
- **Power/flashing:** use **USB-Serial-JTAG**. This is separate from the controller connection.

The Waveshare screen works in this fork but is not a vendor-supported configuration. LCD setup and investigation are in [DSI-PREVIEW.md](docs/DSI-PREVIEW.md).

## Build and use

Clone with submodules, then follow the [Xbox build instructions](docs/XBOX-FORK.md). ESP-IDF 6.1 is required. The [recommended LCD profile](docs/DSI-PREVIEW.md#recommended-build) uses `boards/funcev_dsi_live.defaults`; browser-only builds can leave the LCD disabled. Initial LCD startup can take roughly 30 seconds.

Copy [`.env.example`](.env.example) to an ignored `.env` for local Wi-Fi configuration. Keep credentials and credential-containing firmware private. After changing `web/`, run its typecheck, tests, and build before rebuilding firmware so the embedded console is updated.

For audio, follow [HDMI-AUDIO.md](docs/HDMI-AUDIO.md) and enable **HDMI audio** in Settings. Normal Record/Stop then includes sound on microSD; click **Enable audio** to listen in the viewer. The final audio-enabled and disabled firmware builds passed; web typecheck, 68 web tests, and three recording-tool tests passed. Live testing used HTTP; HTTPS audio was reviewed but not hardware-validated.

**Security:** this experimental profile uses HTTP without an application login. Tailscale protects tailnet traffic; the LAN interface remains unauthenticated. Use a trusted, restricted LAN and do not port-forward it.

| Guide | Contents |
| --- | --- |
| [Controller setup](docs/XBOX-REMOTE-CONTROLLER.md) | USB wiring, Bluetooth pairing, mappings, and limitations. |
| [Recording](docs/SD-RECORDING.md) | microSD recording, MP4 export, and GIF workflow. |
| [Testing](docs/XBOX-TESTING.md) | Observation helper, fresh-frame evidence, and regression checklist. |
| [Fork history](docs/FORK-HISTORY.md) | Hardware receipts, earlier performance/recovery tests, and development direction. |

## Credits

Included software and protocol references are distinguished below. Original attribution remains in [NOTICE](NOTICE); the project is licensed under [Apache-2.0](LICENSE).

| Repository | Contribution or reference |
| --- | --- |
| [espkvm/espkvm](https://github.com/espkvm/espkvm) | Upstream capture, networking, web KVM, recording, and USB infrastructure. |
| [jrowny/p4kvm](https://github.com/jrowny/p4kvm) | Inherited TC358743 bring-up and CSI bridge programming. |
| [espkvm/console](https://github.com/espkvm/console) and [espkvm/microlink](https://github.com/espkvm/microlink) | Web console and native Tailscale foundations; this tree uses console and [our microlink fork](https://github.com/ZacharyLeahan/microlink) as submodules. |
| [Ryzee119/ogx360](https://github.com/Ryzee119/ogx360) and [ogx360_t4](https://github.com/Ryzee119/ogx360_t4) | XID protocol/design references for our independent USB implementation; their implementation code was not incorporated. |
| [rear16/ESP32-XboxBLE](https://github.com/rear16/ESP32-XboxBLE) and [asukiaaa/esp32-client-for-xbox-controller-with-nim-ble](https://github.com/asukiaaa/esp32-client-for-xbox-controller-with-nim-ble) | Series controller report mapping and NimBLE client/identity references. |
| [waveshareteam/Waveshare-ESP32-components](https://github.com/waveshareteam/Waveshare-ESP32-components) | Display-driver comparison and compatibility investigation; our 3.5-inch E findings are in [issue #184](https://github.com/waveshareteam/Waveshare-ESP32-components/issues/184#issuecomment-5973339430). |
| [torvalds/linux](https://github.com/torvalds/linux) and [raspberrypi/linux](https://github.com/raspberrypi/linux) | ICN6211/DSI panel and host references; Linux's TC358743 register map and HDMI-mode audio PLL handling informed the audio fix. |
| [espressif/esp-adf-libs](https://github.com/espressif/esp-adf-libs/tree/master/esp_audio_codec) | Optional `esp_audio_codec` AAC-LC encoder for microSD audio; its Espressif-only license and VisualOn notices remain in the managed dependency. |
| [espressif/esp-idf](https://github.com/espressif/esp-idf), [esp-hosted-mcu](https://github.com/espressif/esp-hosted-mcu), and [esp-bsp](https://github.com/espressif/esp-bsp) | Platform, C6 connectivity, and GT911 touch references; [esp-hosted-mcu issue #241](https://github.com/espressif/esp-hosted-mcu/issues/241) informed the BLE pairing crypto fix. |

Fork work combines these foundations for Xbox: capture/LCD tuning, panel bring-up, touchscreen recording and overlays, XID output, Bluetooth translation, input history, and remote HDMI audio. Detailed provenance and tests are in the [controller notes](docs/XBOX-REMOTE-CONTROLLER.md), [LCD investigation](docs/DSI-PREVIEW.md), [audio notes](docs/HDMI-AUDIO.md), and [upstream review ledger](docs/UPSTREAM-REVIEW.md).
