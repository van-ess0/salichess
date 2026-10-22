#!/bin/bash
# SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
# SPDX-License-Identifier: GPL-3.0-or-later
# Install the newest i486 RPM on the emulator, start the app and save a screenshot.
#
#   tools/emulator-shot.sh [--logged-in] [--tab N] [--page NAME [--mock-friends]] [--patch FILE SED]... OUT.png
#
# --logged-in  mocks the logged-in interface: every `session.loggedIn` in the
#              installed main-page QML is replaced with `true`, so the tiles and
#              empty states for a logged-in user show. The session itself stays
#              logged out (no account data, no challenges or games); the
#              emulator's app never touches a real token.
# --tab N      opens the main page on tab N (0 Play, 1 Explore, 2 Me).
# --page NAME  starts on qml/pages/NAME.qml instead of the main page (e.g. FriendsPage).
# --patch FILE SED  runs sed -e SED on the installed qml/FILE (e.g. pages/PuzzlesPage.qml), to
#              set up a state that needs a tap. SED must not contain single quotes.
# --mock-friends  fills the friends page with four made-up friends.
# The emulator must be running, with its host-only network (see the Sailfish notes).
set -e
mock=0; tab=0; page=; friends=0; patches=
while [ $# -gt 1 ]; do
    case $1 in
        --logged-in) mock=1; shift ;;
        --tab) tab=$2; shift 2 ;;
        --page) page=$2; shift 2 ;;
        --mock-friends) friends=1; shift ;;
        --patch) patches="$patches sed -i '$3' \$Q/$2;"; shift 3 ;;
        *) break ;;
    esac
done
out=$1
[ -n "$out" ] || { sed -n 4,16p "$0"; exit 1; }
rpm=$(ls -t "$(dirname "$0")"/../../build-salichess-*i486*-Debug/RPMS/*.rpm | head -1)
O="-o StrictHostKeyChecking=no -i $HOME/SailfishOS/vmshare/ssh/private_keys/sdk"
scp $O "$rpm" root@10.220.220.1:/root/t.rpm >/dev/null
mockdir=$(mktemp -d)
trap 'rm -rf "$mockdir"' EXIT
if [ $friends = 1 ]; then
    cat > "$mockdir/model" <<'MOCK'
model: ListModel {
    ListElement { userId: "magnus"; username: "Magnus"; title: "GM"; online: true; playing: true
                  blitzRating: 3100; rapidRating: 2900; classicalRating: 2850; gamesCount: 41210; lastGameAt: 1792800000000; seenAt: 1792800000000 }
    ListElement { userId: "alice"; username: "Alice"; title: ""; online: true; playing: false
                  blitzRating: 1620; rapidRating: 1710; classicalRating: 0; gamesCount: 532; lastGameAt: 1790000000000; seenAt: 1792700000000 }
    ListElement { userId: "bob"; username: "Bob"; title: ""; online: false; playing: false
                  blitzRating: 1300; rapidRating: 0; classicalRating: 0; gamesCount: 7; lastGameAt: 1700000000000; seenAt: 1750000000000 }
    ListElement { userId: "newbie"; username: "Newbie"; title: ""; online: false; playing: false
                  blitzRating: 0; rapidRating: 0; classicalRating: 0; gamesCount: 0; lastGameAt: 0; seenAt: 0 }
}
MOCK
    sed -e '/model: friends/{r '"$mockdir/model"'
d}' -e 's/friends\.count/4/g; s/friends\.loading/false/g' "$(dirname "$0")/../qml/pages/FriendsPage.qml" > "$mockdir/FriendsPage.qml"
    scp $O "$mockdir/FriendsPage.qml" root@10.220.220.1:/root/FriendsPage.qml >/dev/null
fi
ssh $O root@10.220.220.1 "
rpm -U --force /root/t.rpm 2>&1 | tail -3
Q=/usr/share/harbour-salichess/qml
[ $mock = 1 ] && sed -i 's/session\.loggedIn/true/g' \$Q/pages/Main*.qml \$Q/components/LoginPrompt.qml
sed -i 's/id: tabs\$/id: tabs\n        currentIndex: $tab/' \$Q/pages/MainPage.qml
[ $friends = 1 ] && cp /root/FriendsPage.qml \$Q/pages/FriendsPage.qml
$patches
[ -n '$page' ] && sed -i 's/MainPage { }/$page { }/' \$Q/harbour-salichess.qml
ps -eo pid,comm | awk '\$2 ~ /^harbour-saliche/ {print \$1}' | xargs -r kill
sleep 1
rm -f /home/defaultuser/x.png
su -s /bin/sh defaultuser -c 'XDG_RUNTIME_DIR=/run/user/100000 WAYLAND_DISPLAY=../../display/wayland-0 QT_QPA_PLATFORM=wayland QT_LOGGING_TO_CONSOLE=1 nohup /usr/bin/harbour-salichess >/home/defaultuser/log.txt 2>&1 &'
sleep ${DELAY:-8}
su -s /bin/sh -g privileged defaultuser -c 'XDG_RUNTIME_DIR=/run/user/100000 DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/100000/dbus/user_bus_socket dbus-send --print-reply --session --dest=org.nemomobile.lipstick /org/nemomobile/lipstick/screenshot org.nemomobile.lipstick.saveScreenshot string:/home/defaultuser/x.png' >/dev/null
grep -i 'qml\|error\|warn' /home/defaultuser/log.txt | head -20"
scp $O root@10.220.220.1:/home/defaultuser/x.png "$out" >/dev/null
