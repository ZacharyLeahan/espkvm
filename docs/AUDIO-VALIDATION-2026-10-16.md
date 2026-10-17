<!-- SPDX-License-Identifier: Apache-2.0 -->

# HDMI audio validation - 2026-10-16

Live HDMI audio reached both a browser and a bounded PCM recorder. A single
60-second HTTP recording remained connected alongside browser video and
preserved 59.64 seconds of non-silent stereo samples. Brief capture gaps remain;
this is evidence of a working path, not lossless audio or measured end-to-end
latency. No media recordings, screenshots, credentials, or raw device logs are
included in this public report.

## Hardware and software

- Espressif Function EV PCB marked v1.6, ESP32-P4 rev 3.2, 32 MB PSRAM;
  Geekworm C790/TC358743; Waveshare 3.5-inch DSI LCD (E).
- Original Xbox through its HDMI adapter; XBMC stereo LPCM at 48 kHz,
  passthrough disabled; HDMI video 1280x720p60. Tests used Wi-Fi and HTTP.
- C790 J4: white SCK to GPIO22, blue-gray SD to GPIO27, yellow WFS to GPIO53,
  black GND to J1 ground opposite GPIO48. OSCK is unconnected. Colors identify
  this particular cable; use the connector signal names on other cables.
- ESP-IDF 6.1 checkout `fff9895c82d744c7237be8847347bdd1b07c6643`,
  esp-hosted 3.0.9. The C6 co-processor firmware was not replaced.
- Audio receiver: Philips I2S slave, 32-bit stereo slots, most significant
  16 bits sent as signed little-endian stereo PCM. Eight 5 ms DMA buffers,
  5 ms application blocks, PSRAM working buffers, and a 6144-byte task stack.
- Bridge audio register corrections and HDMI `ANA_CTL=0x31` restoration;
  close-delimited HTTP PCM; transient DMA-backlog recovery; cache-aligned
  SDIO PSRAM allocation; lwIP PSRAM preference with internal-memory fallback.
  TCP send buffer and window remain 8192 bytes.

The bridge fixes follow the pinned Linux source and the allocator adaptation
follows ESP-KVM upstream `c5761dc`; source links and adaptation scope are in the
[upstream ledger](UPSTREAM-REVIEW.md). The live image was built before the source
changes were committed, so its embedded version does not contain the later
commit IDs. The network implementation is recorded in
[59da248](https://github.com/ZacharyLeahan/espkvm-xbox/commit/59da2481b330042d9d4edabdff87ce407c852693),
the bridge corrections in
[ca27f55](https://github.com/ZacharyLeahan/espkvm-xbox/commit/ca27f5576918cc26196593782423d783509699e1),
the initial audio API in
[d6daefe](https://github.com/ZacharyLeahan/espkvm-xbox/commit/d6daefe0492e542ebdf2f9ca5939cb788520135a),
and the browser behavior in console commit `03f0a719ae272c2fd5ed2ed60000c26db68fde50`.

## Recorded results

| Check | Result | What it establishes |
| --- | --- | --- |
| Initial capture after restoring the audio PLL | 259,200 frames / 5.4 s, stereo 48 kHz, nonzero levels | Existing wiring can carry HDMI audio into the ESP; HDMI sample-present registers alone were insufficient. |
| Final combined browser-video / HTTP-recorder run | 60.005 s wall time; 2,862,720 frames / 59.64 s PCM; one continuous connection | Sustained capture under this load, with brief recovered gaps. Wall-minus-PCM time includes startup and gaps; it is not a packet-loss measurement. |
| Recorded levels | Peak 0.22393798828125; RMS 0.02295185753355995; 2,498,278 active stereo frames | Non-silent PCM. An active frame has either channel strictly above 32/32768 in absolute magnitude. |
| Receiver overruns | 38 at the last live status sample; 248 after disconnect | The latter includes receiver activity while detecting the departed client and is not the recording's overrun total. Counters alone do not measure missing samples. |
| Browser checkpoint | 3,588,240 captured frames / 74.755 s; 25 recovered overruns; local playback queue 43-44 ms; zero observed browser backlog resets | Browser received and scheduled live sound. The queue excludes HDMI, network, browser output, and speaker latency. |
| Memory checkpoint | About 75 KB internal free and 16.9 MB PSRAM free; no SDIO OOM logs in the final boot | Allocation pressure improved at this checkpoint; no exhaustive leak or long-run stability claim. |

The private WAV is 11,450,924 bytes including its header. An independent
Python `wave`/integer-sample pass recomputed its format, frame count, peak, RMS,
and active-frame count on 2026-10-16; every value matched the saved summary
(floating-point comparison tolerance `1e-12`). Its SHA-256 is:

```text
1d6a48f703d3e5b4a02535689dc247c98a23500414f3d4f0377e5bde2b127805
```

The source WAV and browser screenshot remain in ignored local evidence storage.
This checksum identifies the checked artifact without distributing its content.

## Fresh verification during report preparation

With no existing audio listener, a fresh bounded capture completed in 5.005 s:
239,280 stereo frames / 4.985 s PCM, peak 0.147552490234375, RMS
0.017207997898784917, and 228,452 active frames. The observation tool returned
`sound_detected` and exit status 0. This follow-up did not independently verify
whether a video viewer was active, so it is not another combined-load trial.

A separate short API check opened one live PCM reader, then attempted another.
The second request returned HTTP 409, the first continued returning PCM, and
`/api/v1/audio/status` returned `streaming:false` after the first reader closed.
No flash, reboot, settings change, or source playback change was needed.

## Reproduction

The public defaults and build command are in [HDMI-AUDIO.md](HDMI-AUDIO.md#wiring-and-configuration).
`boards/funcev_audio.defaults` opts into the verified GPIO22/53/27 assignment and
48 kHz / 32-bit-slot format. The accompanying Xbox profile selects the tested
Wi-Fi memory policy; `funcev_dsi_live.defaults` selects three DSI preview buffers
with PPA disabled. No private defaults file is required.

The measured live unit additionally had the following settings. They are
listed explicitly because the audio-only overlay does not select controller
features or impose a video-resolution limit:

```text
CONFIG_KVM_USB_XBOX=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_ESP_HOSTED_HOST_FEAT_BT=y
CONFIG_KVM_XBOX_BLE=y
CONFIG_KVM_XBOX_BLE_LEGACY_CP=y
CONFIG_KVM_CAPTURE_720P_MAX=y
```

Start a known stereo LPCM source, enable **Stream HDMI audio** in Settings, and
leave the browser video feed active. Mute the viewer's audio before recording:
only one audio listener is allowed. Substitute the device's actual origin:

```sh
python3 tools/observe_audio.py http://esp.local --seconds 60 --output audio-check.wav
curl --fail --max-time 2 http://esp.local/api/v1/audio/status
```

Record live status during capture to distinguish active-run overruns from
disconnect cleanup. After recording, enable audio in the viewer to check its
queue, retries, and continued video. Authenticated or HTTPS deployments require
the corresponding cookie/certificate options documented by the tool. Use new
output filenames; the recorder refuses to overwrite files.

The original implementation checks included audio-enabled and audio-disabled
firmware builds, web typechecking and 68 web tests, and 3 recording-tool tests.
A fresh `build.audio-repro` using only the documented public overlays completed
successfully (exit 0), producing a 2,117,760-byte firmware image. The generated
configuration was checked for the audio pins/rate/slots, three DSI buffers,
SDIO/lwIP PSRAM preferences, 8192-byte TCP buffers, HTTP mode, and empty default
Wi-Fi credentials. The linker map contains the SDIO allocator wrapper. This
fresh profile leaves optional Xbox controller/Bluetooth and 720p-allocation-limit
features off; it was not flashed, and its memory footprint is not the measured
live profile's footprint.

The fresh build emitted 11 unused-variable/function warnings: one in DSI
preview and ten in the existing Microlink/WireGuard sources; none were in the
new audio overlay or audio/network allocator code. Its initial image SHA-256
was `0d6ac1362c8709a6f237533a35bb49b3584c5d75f2d042c5e7d98c97c452b74b`.
This initial build and the live tests preceded the authentication-lifetime
follow-up below. They do not validate live authenticated logout.

### Final session-lifetime follow-up

[Commit a98d060](https://github.com/ZacharyLeahan/espkvm-xbox/commit/a98d06062166d595988332a546c49859974196b0)
binds an audio stream to its originating cookie session, instead of the
WebSocket socket allow-list. Both final configurations built successfully
(exit 0) with this change. The generated configurations explicitly confirmed
that audio was enabled with GPIO22/53/27 in `build.audio-repro` and disabled
in `build.audio-disabled`. No new audio/auth/allocator warnings appeared.

| Final local build | Firmware bytes | SHA-256 |
| --- | ---: | --- |
| Audio enabled | 2,118,304 | `ae85852a325e49dfae5cca6e53d00bf2f17832cd0295b5bbb11498bedfacb43a` |
| Audio disabled | 2,057,120 | `e1998c87a0921ca304103d07fd2e8c10af60540143e77cd97a71a0ab2d888789` |

These build artifacts contain local Git version metadata and were not flashed.
The running unit remains on the working audio build used for the live results.

A temporary host harness compiled the production session lookup, clear,
logout, capture-reference and validation functions with C11 warnings treated
as errors. Eight lifecycle cases passed: valid session/sliding expiry, logout,
expiry, password-session clearing, slot replacement, mandatory-password-change
refusal, rejected admission, and enabling login after an unauthenticated start.
The clock, request authentication/cookie reader, locking and HTTP responses were
stubbed. This is a focused logic check, not an HTTP integration or concurrency
test; it is not part of the committed CI suite. The capture tool's three tests
were also rerun successfully and added to CI.

For hardware auth acceptance, sign in, open audio with that session, and sign
out while another authenticated session watches status. Verify capture closes
and a new request with the expired cookie is refused. Repeat after a password
change and after enabling login during a previously unauthenticated capture.
Those device-level checks remain outstanding; no login settings were changed
on the user's device during this validation.

## Publication check

The renamed `ZacharyLeahan/espkvm-xbox` repository uses `main` as its default;
`xbox-experiments` retains its earlier published tip. A fresh recursive shallow
clone of published commit `17cbd3fe7da00bc731c8f2a02c170994a6b3ffd1` succeeded
with a clean worktree and fetched both exact submodule revisions:

- Console: `03f0a719ae272c2fd5ed2ed60000c26db68fde50` from the Xbox console fork.
- Microlink: `d6865e1c7ebd5ad8f158029385e93dcb36d220b0` from the existing fork.

The API skill contract is published at
[002ce16](https://github.com/ZacharyLeahan/espkvm-skills-xbox/commit/002ce16977254490d7cc4a74232ade0738fab180).
GitHub Actions returned no runs for this push at verification time; the build
and test outcomes above are local/hardware results, not a hosted CI pass.

## Remaining limits

Subjective listening quality, correct recognition of game sounds, lip sync,
physical end-to-end delay, HTTPS streaming, Ethernet/VPN behavior, repeated
HDMI hot-plug recovery, authenticated-session revocation on hardware, and long endurance
are not established here. Activity
meters do not prove that the game played the intended sound. The source format
must match the configured sample rate; the receiver does not follow rate
changes automatically. Transient resets can make audible gaps, and send
timeouts can disconnect the stream. The browser's bounded reconnection helps
recover but does not make delivery lossless.
