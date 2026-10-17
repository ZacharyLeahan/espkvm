#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -I "$here/.." "$here/test_video_ack.c" -o "$out/test_video_ack"
"$out/test_video_ack"
