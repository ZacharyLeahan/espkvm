# Original Xbox remote-controller experiment

This document records the hardware, software, and code provenance for a local
ESP-KVM build that is being evaluated as a remotely operated original Xbox
controller. An opt-in XID firmware implementation is now available for initial
hardware testing. On 2026-10-11 the real Xbox enumerated it and a 200 ms D-pad
Down press moved XBMC from Games to Movies. All individual controls were then
checked in Xbox Controller Tool on physical controller port 1 (see below).
The initial touchscreen overlay was confirmed working by the operator.
Gameplay and the subsequent proportional-stick update still need validation.

## Physical-console validation (2026-10-11)

### Experimental Series-controller Bluetooth bridge

The fifth LCD mode is opt-in (`KVM_XBOX_BLE`), using NimBLE on the P4 and HCI
over SDIO through the existing C6. Discovery was confirmed by the operator on
2026-10-11. With firmware `f8aec1c`, the operator confirmed all buttons working
in Xbox Controller Tool on the physical console. Serial evidence also recorded
a passing DHKey self-test, encrypted 16-byte-key bonding, and 127 valid input
reports forwarded to the USB input path. Intermediate analog values, long holds,
reconnect persistence, latency, and long-session stability still need validation.
Enter the Bluetooth mode and hold the Series
controller's Pair button until its light flashes rapidly. Leaving that LCD
mode disconnects and releases inputs; retry by cycling back into it.

Reconnect recovery is experimental: transient link loss triggers up to three
rescans, two seconds apart before each scan, within the current Bluetooth mode.
Exiting the mode cancels recovery. A peer requesting repeat pairing can replace
only its own bond once per connection, with Secure Connections and a 16-byte key;
an authenticated bond cannot be replaced with an unauthenticated one. Bonds
are not globally cleared. Link timeout tolerance is being tested at six seconds
(previously 2.56 seconds); USB's stale-input release remains 500 ms.

On 2026-10-11, the operator confirmed that the recovery build `371e4b8`
worked after flashing and pairing again. Serial logs showed a passing DHKey
self-test, encrypted bonding with a 16-byte key, and 58 valid reports forwarded
to USB. LCD updates remained approximately 29-30 fps. That attempt succeeded
without automatic retry or bond replacement, so it does not establish which
change restored operation. Repeated mode-exit/reentry and controller power-cycle
tests are still needed to validate reconnect reliability.

Only the `Xbox Wireless Controller` advertisement and HID input report ID 1
with a 16-byte payload are accepted. Sticks remain proportional; 10-bit
triggers scale to 8-bit XID pressure. RB maps to Black, LB to White. Guide and
Share are ignored, and rumble is not forwarded. Touch inputs take priority,
then fresh Bluetooth input, then browser keyboard input. No reports for 500 ms
releases the USB state; verify long holds during physical testing.

The current test configuration keeps bonds in RAM, so pairing after a reboot
may require the Pair button again. Persistent reconnect and broader firmware
report layouts are not yet supported. Wi-Fi and LCD coexistence need a soak
test before enabling this by default. The installed legacy C6 advertises HCI
and initializes its controller itself: `KVM_XBOX_BLE_LEGACY_CP` skips newer
controller lifecycle RPCs. It must not be blindly enabled on a newer C6 image.

Protocol reference: [ESP32-XboxBLE's discovered Series mapping](https://github.com/rear16/ESP32-XboxBLE#discovered-mapping).
This implementation uses ESP-IDF's native NimBLE API, not the Arduino library.
The first physical pairing attempt connected but was rejected by the controller
with peer SMP DHKey-check error `0x50B`. The next test matches the reference's
bonding + MITM-request + Secure Connections flags, with NoInputNoOutput I/O.
That I/O model still means Just Works pairing, not verified MITM protection.
The auth-flag retry also failed with `0x50B`. The next diagnostic uses a
random-static own address, matching [the original Xbox BLE client reference](https://github.com/asukiaaa/esp32-client-for-xbox-controller-with-nim-ble/blob/master/src/main.cpp).
This test identity is stable within a boot, not yet persisted across reboots.

Both public and random-static address attempts failed with the same `0x50B`.
[Espressif issue 241](https://github.com/espressif/esp-hosted-mcu/issues/241)
documents a matching IDF 6.1 P4 TinyCrypt ECC regression. Our SDK has the
reported missing `soc/soc_caps.h` include in `ecc_dh.c`. The BLE option now
selects `BT_NIMBLE_CRYPTO_STACK_MBEDTLS`, retaining LE Secure Connections
rather than falling back to weaker legacy pairing. A published Bluetooth
P-256 known-answer test exercises NimBLE's selected backend at HCI sync and
blocks scanning if it fails. This workaround changes neither the shared SDK
installation nor the C6 firmware; the later combined build passed button testing.
After this switch, the operator reported `CONNECTED - TEST BUTTONS`, but the
Series light kept blinking and no Xbox test indicators moved. That status only
proved encrypted discovery/subscription completed, not usable input. The next
test reads the HID report map and subscribes to all HID input reports, as the
reference clients do. Only report ID 1 is translated; other report IDs are
not treated as gamepad buttons. LCD `CONTROLLER INPUT ACTIVE` now requires a
valid received gamepad report; periodic logs count notifications and forwards.

Separate display issue: the operator observed repeated/rolling LCD
output during this test. The network JPEG was clean, and logs showed two LCD
initialization timeouts followed by write-only fallback. The operator then
reseated the ribbon and reported that the screen was fixed. No LCD software fix
was established by this observation; Bluetooth reconnect failures continued.

### Wired and remote input baseline

Firmware: `2120786`, ESP DEVICE port connected to original Xbox controller
port 1. Observed through the C790 HDMI capture, not an emulator. Each input
was sent individually over the existing control WebSocket; held tests refreshed
state every 100 ms and captured a JPEG after about 700 ms, then explicitly
released it. No firmware changes were needed for these tests.

| Control | Observed in Xbox Controller Tool |
| --- | --- |
| A, B, X, Y, Black, White | Corresponding pressure reads 255; others zero |
| Left/right trigger | Corresponding trigger reads 255; other zero |
| Left/right stick X and Y, both directions | Correct axis reads +32767 or -32767 |
| Left/right stick click | Correct thumb indicator highlights |
| All four D-pad directions | Correct direction highlights |
| Start and Back, separately | Correct indicator highlights; no page-change chord |
| Final release | All pressure/axis values zero; no highlighted buttons |
| No refresh after an A report | Neutral at the approximately 700 ms screenshot, consistent with the 500 ms safety timeout |

A and B also opened and backed out of XBMC's Movies navigation. These tests
verify individual full-scale inputs, not intermediate pressure/axis values,
simultaneous chords, long-session reliability, rumble, or gameplay latency.
Short one-shot presses were not consistently visible during dashboard tests;
use refreshed held state for reliable testing rather than immediate REST taps.

## Goal

### Experimental LCD controls

In Xbox USB mode, tap unused space in the top or bottom letterbox bar to cycle:
**hidden -> stats -> stats + controller -> controller -> hidden**.
Startup remains stats-only. Controller/record-button hits never cycle modes,
and taps in the game image outside controls do nothing. With no letterbox,
the outermost 16-pixel top/bottom edges provide the cycle gesture instead.

The controller is drawn over the local picture only, without changing the
HDMI capture, stream or recording. It has D-pad, A/B/X/Y, Black/White, LT/RT,
Start/Back, L3/R3 and two virtual stick regions. Multiple touch contacts can
press controls together (up to six mapped button keys). Stick regions send
independent signed analog axes: an 8-pixel radial dead zone, then linear
magnitude up to full tilt at the 52-pixel rim. Diagonals retain their angle;
they do not force both axes to maximum. Moving outside the region or lifting
releases that stick. Network keyboard stick mappings remain full-deflection.
Missing touch reports release local inputs as a safety
measure; fresh local presses take priority over network keyboard input.

During recording or a start/stop transition, mode cycling is locked so Stop
cannot disappear. A recording started remotely also reveals stats/Stop.
The first digital LCD version was confirmed working by the operator; the
analog update needs hands-on verification. Host tests cover mode order,
recording lock, hitboxes, radial dead zone, monotonic travel, signed wire
encoding and framebuffer bounds. Physical USB controls were tested separately
above.

Use the existing ESP-KVM video path to view an original Xbox remotely, then use
the ESP32-P4 USB device port to present a virtual original Xbox controller. The
web console would gain a phone-friendly gamepad with the original Xbox controls:

- D-pad, Start, and Back
- A, B, X, Y, Black, and White
- two analog sticks and stick clicks
- two analog triggers
- optional rumble feedback

Computer KVM mode and Xbox-controller mode require different USB identities and
report formats. The planned design therefore treats Xbox XID as an explicit USB
mode rather than pretending it is an ordinary keyboard or HID gamepad.

## Hardware used

- Espressif ESP32-P4 Function EV Board
- Geekworm C790, based on the Toshiba TC358743 HDMI-to-CSI-2 bridge
- the board's USB-Serial/JTAG port for flashing and diagnostics
- the board's DEVICE USB port for target input (the existing KVM target port)
- HDMI from the original Xbox to the C790
- Wi-Fi with native Tailscale for remote access

The original Xbox controller ports carry USB power and data but use Microsoft's
proprietary XID device protocol. Connecting the DEVICE port to the console will
also require an original-Xbox-controller-port adapter or a purpose-built cable.
The extra Xbox controller-port conductor is not needed for ordinary controller
input emulation.

## Software actually included

This working tree starts from
[espkvm/espkvm](https://github.com/espkvm/espkvm), which is Apache-2.0 licensed.
Its own inherited p4kvm code and other acknowledgements remain documented in
the repository's [NOTICE](../NOTICE).

The web console and native Tailscale implementation remain the repository's
pinned submodules:

- [espkvm/console](https://github.com/espkvm/console)
- [ZacharyLeahan/microlink](https://github.com/ZacharyLeahan/microlink), forked
  from [espkvm/microlink](https://github.com/espkvm/microlink) with per-data-packet
  receive logging removed; pinned by the parent repository's submodule commit.

Local changes currently configure the Function EV Board/C790 combination,
provide native Tailscale access, prefer Ethernet with Wi-Fi fallback, and pace
large HTTP and WebSocket transfers for the ESP-hosted Wi-Fi transport. These
changes are not an upstream ESP-KVM release.

## Xbox protocol references

The following projects and documents are being consulted to understand the
original Xbox USB/XID protocol:

- [XboxDevWiki: Xbox Input Devices](https://xboxdevwiki.net/Xbox_Input_Devices)
- [Ryzee119/ogx360_t4](https://github.com/Ryzee119/ogx360_t4)
- [Ryzee119/ogx360](https://github.com/Ryzee119/ogx360)
- [Microsoft Xbox Hardware Design Specification 1.02](https://consolemods.org/wiki/images/a/ae/Xbox_Hardware_Design_Specification_1.02.pdf)

No source code from `ogx360`, `ogx360_t4`, or another Xbox-controller project
has been copied into this repository as of this document. They are protocol and
design references only. If implementation code is later adapted, its exact
source files, commit, license, copyright notice, and modifications must be
recorded in `NOTICE` before distribution.

## Implementation boundary

### Initial experimental firmware

Enable `CONFIG_KVM_USB_XBOX=y` under ESP-KVM / Features to replace the USB
keyboard/mouse/storage composite with one original-Xbox XID controller.
It defaults off. The SD card remains available for local recording, but is not
exported as USB storage in this mode. Do not connect the Xbox cable to JTAG.

This independent implementation lives in `components/kvm_hid/usb_xbox.c`.
The wire descriptors, requests, and 20-byte input layout were checked against
[`ogx360`'s XID reference](https://github.com/Ryzee119/ogx360/blob/master/Firmware/src/usbd/usbd_xid.cpp)
and its accompanying header. That project is GPL-3.0; its implementation was
not copied or incorporated. Rumble output is accepted but ignored.

For this first bring-up, the existing keyboard input transport is translated:

| Keyboard input | Xbox control |
| --- | --- |
| Arrow keys | D-pad |
| Enter / Backspace | Start / Back |
| Z / X / C / V | A / B / X / Y |
| B / N | Black / White |
| Q / E | Left / right trigger |
| WASD / IJKL | Left / right stick |
| R / T | Left / right stick click |

Buttons/triggers are full pressure and stick directions full deflection; this
is a bring-up mapping, not a finished gamepad UI. Modifiers and mouse reports
do nothing. Reports expire to neutral after 500 ms without fresh keyboard
state, so held keys require refresh; this prevents stale network input from
holding a control indefinitely. Explicit release and disconnect also clear
inputs. Keep the Xbox on a safe menu for the first D-pad test.

Host-only encoding test (does not prove USB enumeration):

```sh
cc -std=c11 -Wall -Wextra -Werror -I components/kvm_hid/include \
  tests/test_xbox_report.c -o /tmp/test_xbox_report
/tmp/test_xbox_report
```

Touchscreen/browser controller overlays and proportional analog input are
next steps after real-console enumeration and button tests succeed.

Bounded button tester (Node 22+, existing authenticated/local control access):

```sh
node tools/xbox-button.mjs http://esp.local down
```

Run without arguments for the button names. It sends a 200 ms press and explicit
release, without taking over another operator's control session. It does not
bypass device authentication. Successful sending is not proof the console
acted; verify the picture. The existing REST keyboard tap queues an immediate
release and was too brief to move XBMC reliably in the first test; use this
tester instead. Never use this mapping with ordinary keyboard firmware.

The firmware supplies a TinyUSB vendor-class XID interface with original Xbox
descriptors, control requests, input reports, and ignored rumble output reports.
Future console work is a gamepad panel that sends complete controller state.
USB identity currently requires rebuilding with the explicit Kconfig option;
it is not a runtime setting.

The first milestone is intentionally small: one emulated Controller S in port
1, digital buttons, sticks, and triggers. Rumble, physical browser Gamepad API
input, multiple controllers, and polished touch controls can follow after the
console recognizes and reads the virtual controller reliably.
