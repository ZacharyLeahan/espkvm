# Original Xbox remote-controller experiment

This document records the hardware, software, and code provenance for a local
ESP-KVM build that is being evaluated as a remotely operated original Xbox
controller. An opt-in XID firmware implementation is now available for initial
hardware testing. On 2026-10-11 the real Xbox enumerated it and a 200 ms D-pad
Down press moved XBMC from Games to Movies. Other controls/gameplay remain
unverified.

## Goal

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
