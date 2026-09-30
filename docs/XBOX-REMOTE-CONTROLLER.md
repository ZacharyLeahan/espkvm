# Original Xbox remote-controller experiment

This document records the hardware, software, and code provenance for a local
ESP-KVM build that is being evaluated as a remotely operated original Xbox
controller. The Xbox controller feature is a proposal, not an implemented or
tested feature yet.

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
- the board's USB FS device port for target input
- HDMI from the original Xbox to the C790
- Wi-Fi with native Tailscale for remote access

The original Xbox controller ports carry USB power and data but use Microsoft's
proprietary XID device protocol. Connecting the USB FS port to the console will
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

## Proposed implementation boundary

The likely firmware work is a TinyUSB vendor-class XID interface with the
original Xbox descriptors, control requests, input report, and rumble output
report. The console work is a gamepad panel that sends complete controller state
over the existing control WebSocket. A boot-time setting would select either
the current keyboard/mouse composite device or the Xbox XID device, followed by
USB re-enumeration.

The first milestone is intentionally small: one emulated Controller S in port
1, digital buttons, sticks, and triggers. Rumble, physical browser Gamepad API
input, multiple controllers, and polished touch controls can follow after the
console recognizes and reads the virtual controller reliably.
