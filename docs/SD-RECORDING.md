<!-- SPDX-License-Identifier: Apache-2.0 -->

# microSD video and audio recording

Enable **HDMI audio** in Settings before starting a normal recording. With the
C790 I2S cable connected, Record/Stop saves **H.264 video and AAC-LC stereo audio**
in the same `.ts` file under `VIDEO/` on the microSD. Browser audio can run at the
same time; muting or closing the viewer does not stop SD capture. With HDMI audio
disabled, recordings remain video-only. The audio choice is kept for the duration
of each recording.

- Audio uses 16-bit source PCM at the configured capture rate, encoded at
  160 kbit/s stereo (1024 samples per AAC frame). The tested wiring uses 48 kHz.
- On the Function EV profile, the encoder/writer stack uses external RAM so
  Wi-Fi and I2S retain internal memory.
- Video and audio share a timestamp origin. A one-second independent PCM queue
  absorbs short card stalls. Lost PCM is replaced with timed silence rather than
  shortening the audio timeline; `audioDropped` and `audioSilenceFrames` expose it.
- If enabled audio cannot start, the recording fails with a clear reason. If
  capture or encoding fails after starting, the file closes and status reports
  the error. A silent soundtrack is valid; absence of I2S clocks is not.
- Timelapses intentionally omit sound. Audio pre-roll needs **microSD** dashcam
  mode; RAM pre-roll with audio enabled returns an explanatory error.
- The on-device clip MP4 converter retains AAC. Ordinary Record/Stop saves TS;
  a computer can remux it without encoding using `ffmpeg -i clip.ts -c copy clip.mp4`.
  GIFs cannot contain sound.

`GET /api/v1/record/status` and `GET /api/v1/video/status` (under `record`) include
`audio`, `audioFrames`, `audioDropped`, and `audioSilenceFrames`. Counts are PCM
frames per channel; encoded totals include final AAC-frame padding. These are
capture/queue diagnostics, not proof of perceptual synchronization or sound quality.

Keep private firmware backups and credentials out of published evidence. Do not
format the card for a test.

## H.264 cache correction (2026-10-17)

The reliability branch now flushes CPU-edited SPS/keyframe data before the
hardware encoder reuses that output buffer. This removes a cache-coherency
hazard that could corrupt later frames without increasing recorder drop counts.
It is a plausible cause of the earlier video error, not a deterministic diagnosis.
See the [cache correction and repeated hardware results](UPSTREAM-REVIEW.md#h264-output-cache-correction-2026-10-17).
Recorder drop counters measure queue/reference loss; strict decoding of the
saved file remains necessary to check bitstream integrity.

## Reliability-branch movie check (2026-10-17)

On the combined upstream-adaptation image, two 30-second movie recordings
passed strict H.264 and AAC decoding: one SD-only and one with parallel network
PCM. Both saved active stereo 48 kHz audio. The parallel repeat saved
`VIDEO/up-000430.ts`, with 30.0373 seconds of AAC and zero reported queue drops.
An earlier parallel trial saved valid audio but failed strict video decoding
with an H.264 macroblock error, despite zero reported drops. That intermittent
video issue remains open; passing repeats do not establish its cause.
See the [upstream ledger](UPSTREAM-REVIEW.md#audible-movie-validation-2026-10-17)
for all three trials and their measured limits. Settings were restored after each.

## Hardware verification (2026-10-16)

Installed on Function EV P4 rev 3.2 with C790, 48 kHz/32-bit-slot I2S,
720p60 HDMI input, H.264 target 4,000 kbit/s, 30-fps cap, Wi-Fi, and the
existing Xbox/Bluetooth/LCD profile. The capture allocations were limited to
720p. Final private firmware SHA-256:
`834e55027c9eb0342050c790269ad7d4af4489958e6bad786b8cf7f96e726593`.

| Trial | Saved result |
| --- | --- |
| SD only, 8 seconds | 166 video frames; 48 kHz stereo AAC, 386,048 decoded audio frames; zero audio queue or video drops. |
| Parallel network audio, 8 seconds | Both tracks decoded; zero audio queue or video drops after the scheduling fix. This immediate post-boot trial delivered only 40 video frames. |
| Audio disabled, 8 seconds | `VIDEO/up-000162.ts`, 174 video frames, 8.002233 s, zero audio tracks, successful decode and zero video drops; audio was restored to enabled afterward. |
| Parallel network audio, 60 seconds | `VIDEO/up-000066.ts`, 16,413,904 bytes; 863 video frames (about **14.4 fps**), 2,882,560 decoded audio frames (60.0533 s), zero audio queue or recorder video drops. One remote H.264 viewer was also connected during status sampling. |

The one-minute TS has exactly one 1280x720 H.264 track and one AAC-LC stereo
track. Both start at 0.700000 s (the TS decoder lead). AAC timestamps are
continuous to rounding precision; audio RMS is 0.005519 and peak 0.081610 on a
normalized scale. The 702 inserted-silence frames include startup alignment and
final AAC padding; no recorder queue loss occurred. The separate network
capture delivered 59.165 seconds in 60.003 seconds, with detected sound.
At 54 seconds, shared capture reported zero DMA overruns and fresh samples.

The actual saved one-minute TS was also passed through the firmware's own C
MP4 converter **on the host**. Both MP4 tracks decoded successfully; start times
were zero, video duration 60.040122 s and audio 60.053333 s. This establishes
container/export behavior, not an on-device dashcam conversion soak.

One-minute TS SHA-256:
`1ba0336efd0000065c3edcbe866534ed0b00f555c24bdd08c2db4ef80868ef47`.
Test media and raw logs remain in ignored local evidence folders. The clips
remain on the card. Video settings were restored after each trial.

The first integration exposed two issues that were fixed before the final
one-minute run: a 40 KB internal stack allocation failed, so the writer uses
PSRAM on this profile; and priority 4 let video/network work starve the AAC
writer, losing 3.89 seconds of PCM in an eight-second parallel test. Audio
recording now runs at priority 6. The final one-minute run's longest measured
card write was 127.7 ms and audio pump 158.1 ms; minimum spare writer stack was
36,808 bytes. These are observed maxima, not worst-case guarantees.

Final enabled and disabled firmware builds passed without new warnings. Six
recorder host suites, real-codec TS/MP4 round trips (including gaps and padded
split boundaries), six hardware-helper tests, three WAV-tool tests, and 68 web
tests passed. CI now includes the host checks; a hosted CI run is not claimed.

**Limits:** this is a one-minute 720p test, not hours of recording or a locked
30/60 fps result. Absolute lip-sync, AAC priming, physical latency, and sustained
SD dashcam pre-roll still need measurement. The source must retain the
configured PCM sample rate; changing it mid-recording is unsupported.

## Reproduce a bounded recording check

Use the current audio-enabled hardware profile, H.264 capture support, and a
known audible stereo PCM HDMI source. The helper restores the prior video
settings, saves only allowlisted configuration values, and checks the downloaded
file using `ffprobe` and `ffmpeg`. It refuses to take over an active recording.

```sh
python3 tools/check_sd_audio.py http://esp.local --seconds 8 --output-dir ignore/sd-check-8s
python3 tools/check_sd_audio.py http://esp.local --seconds 60 --output-dir ignore/sd-check-60s
python3 tools/check_sd_audio.py http://esp.local --seconds 8 --video-only --output-dir ignore/sd-check-video-only
# Add --no-network-audio to test SD alone or leave an existing listener alone.
# Authenticated HTTPS: --cookies cookies.txt --ca espkvm-ca.pem
```

Use a new output directory for each run. Media stays local and on the card;
these checks do not delete recordings. The result distinguishes decoded audio
activity from silence and reports container timing; it does not establish
physical lip-sync. Host checks are `sh components/kvm_record/test/run.sh`,
`python3 components/kvm_record/test/test_audio_roundtrip.py`, and
`python3 -m unittest discover -s tools -p test_check_sd_audio.py`.

## Before integration: audio track check (2026-10-16)

**The previous firmware saved video only, even when HDMI audio was enabled.**
A fresh bounded eight-second recording was saved on the microSD as
`VIDEO/up-001764.ts`, then downloaded and inspected with `ffprobe`:

- 2,862,300 bytes, 7.983733 seconds, 1280x720 H.264, 178 recorder frames.
- Exactly one video stream and **zero audio streams**; zero recorder-reported
  video drops. This says nothing about subjective smoothness.
- During recording, a separate network PCM capture received 235,200 stereo
  frames (4.9 seconds at 48 kHz) and detected activity. The source therefore
  reached the audio capture path; it was not written into the card's video file.
- Original video settings were restored and the audio listener was released.
  The test clip remains on the card. Media and raw results stay in ignored
  local evidence storage; this public note contains only test metadata.

File SHA-256: `92cb56b0990ad17421288a66b6a9c96afcc12bc238dbd411f96fe70e55385a58`.

```sh
ffprobe -v error -show_entries stream=index,codec_type,codec_name,sample_rate,channels,width,height:format=duration,size -of json clip.ts
```

At that checkpoint, `ts_mux.c` declared one H.264 stream, `kvm_record.c`
fed only video frames into it, and `ts_to_mp4.c` produced only a video track.
The four existing recorder host tests passed, but they cover video/container,
frame-buffer and subtitle behavior, not audio. The separate WAV tool saves on
the computer running it. Adding microSD audio requires a recorder audio input,
timestamps, an audio track/container format, and matching export support;
turning on browser audio cannot supply that integration.

## Tested on 2026-10-04

Function EV P4 + C790, Wi-Fi, 720x480p60 Xbox input, no browser viewers:

- A new SK32G card mounted writable at 40 MHz (30,436 MiB reported capacity).
  A screenshot saved and downloaded successfully; no formatting was needed.
- The normal three-buffer LCD build could not allocate the recording ring:
  232 KiB PSRAM free versus about 2,400 KiB required. A failed start did not
  produce a recording.
- An isolated build with `CONFIG_KVM_DSI_PREVIEW` disabled recorded H.264
  at a 4,000 kbit/s target. The LCD was unavailable during this trial.
- A 20-fps limit saved 289 frames in 15.013 seconds (19.25 frames/s).
- A subsequent 30-fps limit saved 567 frames in 19.999 seconds (28.35 frames/s).
  Both clips reported zero recorder drops and decoded successfully on the Mac.
- A 60.017-second moving-homebrew gameplay test saved 1,676 frames (27.93
  frames/s), zero recorder drops, and decoded without errors. An eight-second
  720x480 GIF was generated at 25 fps, plus a full-length MP4 remux without
  re-encoding. The original `.ts` recordings remain on the card.

These are short 480p tests, not proof of sustained recording, 720p recording,
or simultaneous LCD and SD operation. The configured bitrate is a target, not
the actual bitrate. Recorder drops do not count every possible source-frame
omission. In particular, 60-Hz HDMI input does not mean 60-fps recording.

## Earlier 480p trial workflow (2026-10-04)

Use the existing authenticated API or the trusted-LAN profile; do not expose
the latter to the public internet. Save the original `vid_codec`, `vid_fps_max`
and `h264_kbps` settings before changing them, and restore all three afterward.
Do not dump the whole settings response into public logs: it contains secrets.

1. Build separately from the working LCD build, using its board configuration
   with `CONFIG_KVM_DSI_PREVIEW` disabled. Do not erase NVS or format the card.
2. Set `vid_codec=1` (H.264), `vid_fps_max=30`, `h264_kbps=4000` using
   `PUT /api/v1/settings` with a JSON body. This is a trial preset, not a
   universal safe maximum.
3. `POST /api/v1/record/start?seconds=15` with JSON `{}` and
   `Content-Type: application/json`. The bounded duration stops automatically.
4. Poll `GET /api/v1/record/status`; check `frames`, `dropped`, and `stopped`.
5. Download the exact returned `file` through
   `GET /api/v1/captures/file?path=VIDEO/<name>.ts` after recording stops.
6. Restore settings and the known-good LCD firmware. Check HDMI and LCD again.

Files without a wall clock use an `up-...` name. The microSD retains the source
recording; downloading does not delete it. GIF creation happens on the Mac,
not on the ESP.

## Validate and make a GIF

Use unique output filenames to avoid overwriting earlier evidence:

```sh
ffprobe -v error -count_frames \
  -show_entries stream=width,height,nb_read_frames:format=duration,size \
  -of json clip.ts
ffmpeg -v error -i clip.ts -fps_mode passthrough -enc_time_base demux -f null -
ffmpeg -ss 2 -t 6 -i clip.ts -filter_complex \
  '[0:v]fps=25,split[a][b];[a]palettegen[p];[b][p]paletteuse=dither=sierra2_4a' \
  -loop 0 gameplay.gif
```

Compute delivered FPS from decoded frame count divided by duration. Changing
the FPS cap after encoder initialization left nominal 20-fps metadata in the
30-fps-target trial; timestamps and decoded count showed the actual rate.
Preserve the demuxer time base when checking decode errors. The GIF filter
explicitly resamples to 25 fps rather than claiming all HDMI frames were saved.

## Experimental LCD Record/Stop control

The status-strip build includes a persistent top-right button with a red
record circle. Tap it to select H.264, a 30-fps target, and 4,000 kbit/s, then
record to the card. It becomes a white stop square, with a separate gently
blinking red indicator and minutes:seconds timer to its left. Amber dots mean
the requested action is still in progress, not that recording has started.
Tap again to close
the file and restore the prior codec/rate/bitrate. Taps elsewhere still toggle
the entire overlay, including the record/stop button, indicator and timer.
The overlay stays visible while recording or processing a start/stop request;
taps outside Stop do not hide it. Stop recording first, then tap elsewhere to
hide everything. A remotely started recording also reveals the controls.
When idle and hidden, any tap restores the overlay without activating a
hidden button. The overlay is LCD-only and is not burned into recordings.

The recorder runs on a separate control task. Failed starts show an LCD
message and restore settings; detailed reasons remain in the system log.
Automatic stops (including the configured recording duration) also restore
settings. A power interruption cannot run that restoration or close the file:
stop from the button before disconnecting power. A deliberate web-setting
change is not overwritten if it differs from the recording preset.

For this memory-constrained hardware, append `boards/funcev_lcd_record.defaults`
to the live three-buffer LCD profile. This opts into fixed **1280x720 maximum
capture allocations**, instead of reserving for 1920x1080. 480p and 720p stay
within that bound; larger inputs are not supported by this profile. Keep the
normal profile for 1080p, and retain a known-good private rollback image.

Local recording is no longer treated as a remote viewer for the LCD's 1-fps
limit. Additional video consumers still receive remote priority. Both outputs
target smooth video, but simultaneous rates require hardware measurement;
30 fps on each is a target, not a guarantee.

First simultaneous 480p result (2026-10-04): the user verified the top-right
Record/Stop taps, timer, and smooth picture. A longer clip stopped from the
LCD contained 891 decoded frames over 33.933 seconds (26.26 fps), zero
recorder-reported drops, and no decode errors. LCD diagnostics during the
recording were about 26-27 updates/s, returning to about 30 after stopping.
The subsequent build caches button rasterization and labels local recording
separately from remote viewing. Its bounded final check saved 393 frames in
14.982 seconds (26.23 fps), with zero recorder drops and no decode errors;
the LCD reported 264 updates over a 10.044-second recording interval. Caching
the button did not establish a 30-fps result. Idle settings were restored and
recording was off at handoff. Simultaneous 720p and a long soak are pending.

GT911 coordinate parsing follows the [Espressif driver](https://github.com/espressif/esp-bsp/blob/master/components/lcd_touch/esp_lcd_touch_gt911/esp_lcd_touch_gt911.c).
The panel's existing configuration supplies the coordinate range; no touch
reset or configuration rewrite is performed. Confirm the top-right hit target
on hardware. Host tests cover drawing bounds and touch press/hold debounce.
