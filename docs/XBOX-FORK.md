# Xbox experiment fork

Upstream: https://github.com/espkvm/espkvm (original attribution and licensing
are preserved). Xbox changes live on the `xbox-experiments` branch of
https://github.com/ZacharyLeahan/espkvm, with no upstream pull request.

## Current state

ESP32-P4 Function EV rev 3.2 + Geekworm C790. The current diagnostic uses RGB888,
two CSI lanes at 972 Mb/s, and MJPEG over HTTP. Dashboard 720p60 capture and the
automatic switch into 480p60 game capture have worked at about 60 captured
frames/sec. This is NOT browser throughput: the current stream tests are
limited to 10 published FPS. Wi-Fi buffer exhaustion remains unresolved; the
return transition on this exact build still needs validation. See
[measured results](XBOX-CAPTURE-DEBUG.md). Xbox controller emulation is only a
[proposal](XBOX-REMOTE-CONTROLLER.md), not an implemented feature.

## Build

Use ESP-IDF 6.1 and initialize submodules:

```sh
git clone --recursive --branch xbox-experiments https://github.com/ZacharyLeahan/espkvm.git
cd espkvm
. tools/env.sh
idf.py -B build.xbox -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;boards/funcev_p4.defaults;boards/funcev_xbox.defaults" build
```

The public profile is credential-free. Configure Wi-Fi and Tailscale locally.
Private board defaults, sdkconfig, build artifacts, passwords and authentication
keys are not published. Saved device settings such as JPEG quality and the
10-FPS limit are not embedded in this profile; set them on the device.

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
