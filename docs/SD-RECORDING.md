<!-- SPDX-License-Identifier: Apache-2.0 -->

# microSD recording: first hardware trial

Experimental. Keep a private
known-good firmware backup and restore it after testing. Never publish local
sdkconfig files or firmware containing network credentials.

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

## Existing API workflow

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
stats. The overlay is LCD-only and is not burned into recordings.

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
