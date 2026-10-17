# Experimental HDMI audio

The remote viewer can receive stereo PCM from the C790 through the ESP32-P4.
The touchscreen does not need speakers. This implementation is opt-in and needs
separate I2S wiring; the CSI ribbon carries video only.

Software validation: firmware build with audio enabled, browser playback of a
synthetic 440 Hz mock source, network-chunk reassembly and playback queue tests,
and bounded WAV evidence capture. Live C790 capture has now produced non-silent
stereo 48 kHz PCM with the wiring below. A 60-second test with browser video remained connected and captured 59.64
seconds of audio, recovering brief overruns. Subjective audio quality,
video/audio synchronization and physical end-to-end delay remain unverified.
The sanitized [validation report](AUDIO-VALIDATION-2026-10-16.md) records exact
metrics, independent WAV checks, fresh API checks, and reproduction steps.

## Bring-up on the Function EV board (2026-10-16)

The user connected the supplied four-wire cable: white SCK -> GPIO22,
blue-gray SD -> GPIO27, yellow WFS -> GPIO53, black GND -> GND opposite GPIO48.
The audio-enabled Xbox/Bluetooth/DSI build installed into ota_1 and booted;
HDMI remained 1280x720p60, capture reported about 60 completions/sec, and LCD
preview recovered to about 20 updates/sec after its existing I2C retry path.
The receiver initialized, but the initial capture returned 503 with no samples.
After connecting the controller cable to DEVICE USB, the Xbox controller
enumerated and the USB probe reported bus=true and mounted=true.

Inspection found incorrect inherited audio register addresses and masks.
`FORCE_MUTE`, `AUTO_CMD*`, `FS_IMODE`, `ACR_*`, `SDO_MODE1`, and `DIV_MODE` now
match the TC358743 register map documented in the Linux driver, with proper
100 ms buffer-start and divider-delay field encoding. Reference:
https://github.com/torvalds/linux/blob/master/drivers/media/i2c/tc358743_regs.h
The corrected build passes and is installed. Playback was started through XBMC
and verified using fresh video stream frames; XBMC reports stereo 48 kHz audio
with passthrough disabled. The C790 reports HDMI audio sample present,
FS_CODE=2 (48 kHz), CONFCTL=0x0c17 and I2S_FMT=0x02.

The missing clock source was subsequently found in Linux's HDMI interrupt
handler: `ANA_CTL` (0x8545) must be restored to 0x31 after HDMI detection.
It had remained at 0x10, leaving the audio PLL off even with HDMI audio packets
present. The polling driver now restores it when HDMI mode is detected.
After this change, the existing wiring produced 259,200 stereo frames (5.4 s
at 48 kHz), peak 0.036438 and RMS 0.003620 in a valid WAV. No rewiring was needed
for that capture. This establishes a working HDMI-to-I2S-to-network path.

That trial then stalled with Wi-Fi SDIO allocation failures and 338 receiver
overruns. The driver calls these "mempool OOM", but this configuration allocates
those buffers from the internal DMA heap. That failure motivated coalesced
headers, close-delimited PCM (avoiding tiny HTTP chunk-framing writes), PSRAM
working buffers, and explicit DMA-heap diagnostics. A transient receiver
overrun resets the DMA queue and resumes with fresh samples. Playback
quality and end-to-end latency remain unverified.

With audio plus advanced bridge debug enabled, the serial log reports HDMI
sample presence, raw sample-rate code, audio buffer enable, and I2S format every
five seconds. A failed capture also logs observed BCLK/WS transitions over 2 ms;
this polling check establishes activity only, not a clock frequency. These
checks help distinguish missing HDMI audio from a missing I2S connection.

## Final live checkpoint (2026-10-16)

The final Xbox build is installed. It includes the PLL/register fixes, 5 ms PCM
blocks, PSRAM working buffers, capture recovery, cache-aligned SDIO PSRAM
allocation, lwIP PSRAM preference, and bounded browser reconnection.

With the web video feed active, a single HTTP capture stayed connected for
60.005 seconds and saved 2,862,720 stereo frames (59.64 seconds at 48 kHz).
Peak was 0.223938 and RMS 0.022952. The last live status sample recorded 38
DMA overruns; capture recovered automatically. The count rose to 248 while
the server detected the recording client's disconnect, so that final count
must not be interpreted as all occurring during the recording. No SDIO OOM
messages appeared in this final boot. Internal free memory was about 75 KB
with 16.9 MB PSRAM free during combined streaming.

The real browser remained active beyond 74 seconds of captured sound, with
25 recovered DMA overruns and no browser backlog resets at the last live
check. Its local queue read 43-44 ms. This queue excludes source, network and output-device latency.
Brief gaps remain possible under load; this is not a claim of lossless capture
or lip synchronization. Audio-enabled and disabled firmware builds pass;
web typecheck, 68 web tests and 3 recording-tool tests pass. HTTPS was preserved
by API choice and reviewed, but this live device was tested over HTTP.

Local evidence: `ignore/audio-2026-10-16/hdmi-audio-60s.wav` and `evidence.json`.
The viewer is the audio listener while enabled; mute it before using the agent's
recording tool. Captured PCM can establish activity and preserve evidence for
listening, but level measurements alone do not identify the sound or assess
its subjective quality.

## Header cross-check (2026-10-16)

Checked against Espressif's current P4X Function EV J1 table (the user's PCB is
marked v1.6), Geekworm's C790 connector drawing, and the supplied photos:

| Cable color inferred from connector photo | C790 J4 | ESP J1 | Row |
|---|---|---|---|
| White | 1 SCK | 12 / GPIO22 | Outer |
| Blue-gray | 2 SD | 38 / GPIO27 | Outer |
| Yellow | 3 WFS | 35 / GPIO53 | Inner |
| Empty cavity | 4 OSCK | Not connected | - |
| Black | 5 GND | 34 / GND | Outer |

Inner means toward the PCB components; outer means toward the board edge.
Counting paired positions from the C6 / PROG_C6 end: blue-gray is the second
pair outer, yellow third pair inner, black fourth pair outer. GPIO53 is between
GPIO48 and GPIO47 on the inner row; GPIO33 is three paired positions farther
toward the opposite end. Photos appear consistent with this mapping, but do not
prove continuity through the crimp contacts. Colors are inferred from this
specific cable photo, not a manufacturer guarantee for every cable.

References:
- https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4x-function-ev-board/user_guide.html#header-block
- https://geekworm.com/cdn/shop/products/C790-IMG-4490-1.jpg
- https://geekworm.com/cdn/shop/products/I2S-diagram-2000.jpg

## Wiring and configuration

Connect C790 SCK to the selected ESP BCLK input, WFS to WS, SD to DIN, and GND to
GND. OSCK/MCLK is not needed for the ESP slave receiver. Confirm connector
orientation, actual GPIO availability, and signal voltage on your board before
connecting it. The Function EV mapping above has produced live PCM; GPIO22 also conflicts with the round SPI display's default CS.

In menuconfig, enable `Features -> HDMI audio capture (I2S)`, then set the three
GPIOs in `HDMI audio (experimental)`. Pins default to -1, requiring intentional
assignment. Set `KVM_AUDIO_RATE` to the source's actual PCM rate (normally 48000
for the Xbox), and `KVM_AUDIO_SLOT_BITS` to its serial slot width (32 by default;
16 is also supported). The capture uses Philips I2S, stereo, slave mode. With
32-bit slots, the most significant 16 bits become browser PCM. Use stereo
linear PCM output on the source; Dolby/DTS passthrough is not decoded.

Receiver initialization probes pin/driver configuration, not physical signal
presence. It does not detect or track source sample-rate changes; reconnect
with matching configuration if the source format changes. The existing EDID
advertises stereo LPCM at 32, 44.1 and 48 kHz.

The opt-in [Function EV audio overlay](../boards/funcev_audio.defaults) preserves
the tested audio pins and format without enabling audio on other boards. A
fresh build with the public Xbox network and DSI preview profiles is:

```sh
# Run from the firmware repository; initialize its web submodule first.
(cd web && npm ci && npm run typecheck && npm test && npm run build)
. tools/env.sh
idf.py -B build.audio-repro \
  -D SDKCONFIG=build.audio-repro/sdkconfig \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;boards/funcev_p4.defaults;boards/funcev_xbox.defaults;boards/funcev_dsi_live.defaults;boards/funcev_audio.defaults' \
  build
```

Use a new build directory when verifying defaults: an existing `sdkconfig`
keeps prior menuconfig choices. This profile targets P4 rev 3.x, enables HTTP,
and contains no Wi-Fi credentials. Configure credentials through the device's
normal setup. The audio overlay does not choose an Xbox USB/Bluetooth controller
mode; the measured live unit had those optional features enabled separately.
It also had `CONFIG_KVM_CAPTURE_720P_MAX=y`; see the validation report for those
additional settings and the distinction between the live and fresh builds.

Build the web console before building the firmware. After installing an audio
build, turn on **Stream HDMI audio** in Settings, then click **Enable audio** in
the viewer. Volume affects local playback only; the meter measures the source.
Disabling the setting stops an existing stream without rebooting. There is one
audio listener at a time; video and controller input remain independent.

## Delay and performance

At 48 kHz, uncompressed stereo 16-bit audio uses 192,000 bytes/second (1.536
Mbit/s before HTTP/TCP/TLS overhead). There is no audio encoder load. Eight DMA
buffers hold about 40 ms total; the worker reads roughly 5 ms at a time. At 48 kHz, each 960-byte PCM block
fits one TCP packet. CPU-side working buffers live in PSRAM. A
separate asynchronous HTTP task keeps audio reads out of the HTTP server loop.
TCP_NODELAY avoids Nagle delay; sends have a 100 ms socket timeout. A DMA
overrun discards the affected block and resets capture to fresh samples. A completed slow send also resets capture; a failed send closes the connection.
The browser independently discards stale bursts. P4 SDIO transfer buffers honor
the configured PSRAM preference with cache-aligned allocations and an internal
RAM fallback. The Xbox profile also prefers PSRAM for lwIP packet allocations.

The browser starts with about 30 ms queued. After an underrun it restarts with
20 ms; a queue over 120 ms is discarded, and coalesced network reads keep only
the newest 60 ms. Muting cancels the request and stops scheduled sources. Audio
is deliberately independent of video timestamps, so it is not lip-synchronized.
An established stream that disconnects can reconnect after 250 ms, with a
three-consecutive-failure limit renewed after two seconds of received audio.
HTTP refusals (including another listener), invalid format and suspended
playback stop retries. Mute cancels the reader and any pending retry.
The tooltip reports the local playback queue and backlog reset count. These
figures exclude network, HDMI source buffering and output-device latency.

## Game-testing evidence and API contract

All routes require the ordinary session cookie when authentication is enabled.
A video viewing token does not authorize audio. Responses are not cached.

- `GET /audio.pcm`: close-delimited `application/octet-stream`, interleaved signed
  16-bit little-endian L,R samples. Headers `X-Audio-Rate` and
  `X-Audio-Format: s16le-stereo` describe the stream. HTTP 401 means sign-in is
  needed, 409 means another listener is active, and 503 means disabled,
  unconfigured, allocation failure, or no usable initial I2S samples. A running
  stream closes on clock loss, logout, or send failure/timeout. DMA overruns
  remain counted but trigger a capture reset so sound can resume.
- `GET /api/v1/audio/status`: JSON with `available` (receiver initialized, not
  proof of wiring), `enabled`, `streaming`, `rate`, `channels`, `frames`,
  `overruns`, `peak`, `rms`, `sample_age_ms`, `active_frames`, and `peak_max`.
  An audio-disabled build returns `available:false` and `reason` instead.
  Counters reset for each new listener and persist after disconnect. Frames
  count capture, not confirmed playback. Peak/RMS are normalized 0-1 for the
  latest captured block; `peak_max` preserves brief transients. Active frames
  count stereo frames with either channel exceeding 32/32768 (about -60 dBFS).
  `sample_age_ms` is -1 until samples arrive. Monitoring is passive: opening
  status does not start a capture. Stale levels are not evidence of current sound.

For a bounded recording plus machine-readable summary (mute the viewer first):

```sh
python3 tools/observe_audio.py http://esp.local --seconds 5 --output game-audio.wav
# HTTPS/authenticated devices: add --cookies cookies.txt --ca espkvm-ca.pem
```

The tool writes a new WAV without overwriting an existing file and prints JSON
with frame count, duration, peak, RMS, activity count and observation
`sound_detected`, `quiet_samples`, or `no_samples`. Capture/transport failures
return nonzero and `capture_failed`; a partial WAV may remain as evidence.
Neither clocked silence nor activity alone establishes that the game is healthy.
Record the game build/scenario alongside the WAV, and use known audible events.

For further hardware iteration: verify clocks and sample format with a known
tone; compare audio on/off video FPS, input latency and overruns over Ethernet
and Wi-Fi; then measure a synchronized visual/audio event externally. Tune DMA
and browser prebuffer only after measuring jitter and actual output delay.

## Mock and API companion notes

`npm run dev:mock` provides a labeled synthetic 440 Hz tone. It does not establish
C790 operation or live latency. `npm test` covers split PCM frames, channel order,
normalization, silence, stale bursts, underruns, and playback queue reset policy.

The API companion is maintained in the
[Xbox skills fork](https://github.com/ZacharyLeahan/espkvm-skills-xbox/tree/xbox-audio).
The documented `espkvm/mcp` repository
returned "Repository not found" during this work, so its integration could not
be updated. An MCP wrapper should expose status as a read-only tool and keep
bounded WAV capture explicit, preserving the single-listener conflict and auth.
