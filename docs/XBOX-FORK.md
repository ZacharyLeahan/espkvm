# Xbox experiment fork

Upstream: https://github.com/espkvm/espkvm (original attribution and licensing
are preserved). Xbox changes live on the `xbox-experiments` branch of
https://github.com/ZacharyLeahan/espkvm, with no upstream pull request.

## Current state

ESP32-P4 Function EV rev 3.2 + Geekworm C790. The current diagnostic uses RGB888,
two CSI lanes at 972 Mb/s, and MJPEG over HTTP. Dashboard 720p60 capture and the
automatic switch into 480p60 game capture have worked at about 60 captured
frames/sec. With reduced TCP buffers, a ten-minute local Wi-Fi test at a
6-FPS cap and JPEG quality 75 averaged 4.74 delivered FPS with zero reconnects
while the full console was also viewing. Xbox reboot recovery and a short
720p60 game capture check succeeded. This is not 60-FPS browser video or a
guarantee of zero gaps; longer-term and remote-link testing remain open. See
[measured results](XBOX-CAPTURE-DEBUG.md). Xbox controller emulation is only a
[proposal](XBOX-REMOTE-CONTROLLER.md), not an implemented feature.

## Build

Use ESP-IDF 6.1 and initialize submodules:

```sh
git clone --recursive --branch xbox-experiments https://github.com/ZacharyLeahan/espkvm.git
cd espkvm
. tools/env.sh
# Optional: copy .env.example to .env and fill in WIFI_SSID and WIFI_PASSWORD.
cp .env.example .env
python3 tools/build_xbox.py
```

Skip the copy if you already have a `.env` (do not overwrite your settings), or
if you prefer to configure Wi-Fi on the device. Edit `.env` in a text editor.
Keep the example's quoted format; values are literal, not shell commands.
The helper reads `.env` automatically and builds in ignored `build.xbox-local`.
Use `python3 tools/build_xbox.py --configure-only` to generate the configuration
without compiling. No third-party dotenv package is required.

On an existing build, defaults do not override saved `sdkconfig` choices. To
reproduce the network tuning, use `idf.py -B build.xbox-local menuconfig` and
verify `CONFIG_LWIP_TCP_WND_DEFAULT=8192`,
`CONFIG_LWIP_TCP_SND_BUF_DEFAULT=8192`, and
`CONFIG_LWIP_TCP_RECVMBOX_SIZE=8` in `build.xbox-local/sdkconfig`, then rebuild.
A fresh build picks these up from `boards/funcev_xbox.defaults`.

Changing `.env` replaces the two build defaults even on an existing local build;
removing it resets those defaults to empty. Saved device Wi-Fi settings in NVS
still take precedence: this does not replace credentials already saved on the
device. Configure those through the device settings; do not erase storage just
to change the network.

**Never publish this local firmware if you embed real credentials.** `.env`,
generated defaults, sdkconfig, and compiled binaries can contain the password.
Git ignores them, but that does not encrypt them or make a binary safe to share.
Tailscale authentication keys are not accepted by this helper; configure them
on the device. The script builds only; it does not flash or reset the hardware.

The public profile is credential-free. Configure Wi-Fi and Tailscale locally.
Private board defaults, sdkconfig, build artifacts, passwords and authentication
keys are not published. The tested runtime settings are MJPEG, JPEG quality 75,
and a 6-FPS limit (`vid_codec=0`, `jpg_quality=75`, `vid_fps_max=6`). Set them
on the device: these runtime values are not embedded in this board profile.

**Security:** this experimental profile disables HTTPS and application login.
Tailscale protects tailnet access, but it does not protect the unencrypted,
unauthenticated local LAN listener. Use only on a trusted, restricted network;
do not port-forward it or expose it publicly. Keep the upstream secure defaults
for other deployments.

## Dependency changes

The web console remains pinned to upstream `espkvm/console`. The microlink
submodule is pinned to `ZacharyLeahan/microlink`, branch `xbox-experiments`,
which preserves the existing `espkvm/microlink` idf6 base and removes per-data-
packet WireGuard logging from the receive hot path. Clone recursively to include
that change. No pull request to either upstream repository is part of this work.
