#!/bin/bash
# SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
# SPDX-License-Identifier: GPL-3.0-or-later
# Install the newest i486 RPM on the emulator, and only on the emulator, without the
# confirmation popup that `sfdk deploy --sdk` raises: copy it with scp and install it
# with pkcon as root. Never use this for the phone, which must ask.
#
#   tools/deploy-emulator.sh [RPM]
#
# The emulator must be running, with its host-only network (10.220.220.1).
set -e
repo=$(cd "$(dirname "$0")/.." && pwd)
rpm=${1:-$(ls -t "$repo"/../build-salichess-*i486*-Debug/RPMS/*.rpm | head -1)}
[ -f "$rpm" ] || { echo "No i486 RPM found; build for the -i486 target first." >&2; exit 1; }
O="-o StrictHostKeyChecking=no -i $HOME/SailfishOS/vmshare/ssh/private_keys/sdk"
host=root@10.220.220.1
# A fixed name: the version has a '+' in it, which some tools mishandle in a path.
scp $O "$rpm" $host:/root/harbour-salichess.rpm >/dev/null
ssh $O $host 'pkcon -y --allow-reinstall --allow-downgrade install-local /root/harbour-salichess.rpm 2>&1 | tail -4; rm -f /root/harbour-salichess.rpm; rpm -q harbour-salichess'
