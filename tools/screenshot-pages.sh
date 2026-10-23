#!/bin/bash
# SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
# SPDX-License-Identifier: GPL-3.0-or-later
# Screenshot the app's pages on the emulator, logged out and with the logged-in
# interface mocked. The rules for when to run it are in docs/README.md.
#
#   tools/screenshot-pages.sh [options] [PAGE...]
#
#   PAGE            a page label (see --list), or the part before its "-"
#                   (MainPage shoots all three tabs); none means every page
#   --changed[=REF] the pages whose QML changed since REF (default HEAD), counting
#                   uncommitted and untracked files, and the components a page
#                   uses, and theirs in turn
#   --new           the pages that have no screenshot yet
#   --state S       only the shots of that state (logged-in, logged-out, any-login)
#   -o DIR          where the shots go (default: docs/screenshots/)
#   --no-install    do not install the newest i486 RPM first
#   --list          list the pages and their states, and exit
#
# PAGE, --changed and --new together shoot the union. A shot is named PAGE_STATE.png
# (PuzzlesPage_logged-out.png) and replaces the old one; git keeps the older ones.
#
# "Logged in" is a mock: `session.loggedIn` becomes `true` in every installed QML
# file and the session stays empty, so no account data shows. Pages that need a
# running game or a controller handed to them (GamePage, ChatPage, ComputerGamePage
# in play, AnalysisSummaryPage, ChallengeWaitingPage, SeekWaitingPage) are not here.
# The profile, games history and daily puzzle load real data from lichess.org, so
# the emulator must be online. It must be running, with its host-only network (see
# the Sailfish notes).
#
# To add a page, write a function page_<Label> ("-" in the label is "_" in the name)
# in the list below, using:
#   open 'Page { prop: 1 }'   what the app starts on ('Page' means 'Page { }')
#   tab N                     for MainPage: the tab to show (switched to after the start,
#                             as a tap on its name does)
#   wait S                    seconds to wait before the shot (default 6)
#   states S...               the states to shoot: logged-out and logged-in (the
#                             default), logged-in or logged-out when the page cannot
#                             be reached in the other, any-login when it looks the
#                             same in both. docs/pages.puml shows how pages are reached.
#   mock_friends              fill FriendsPage with made-up friends
# $STATE is the state being shot.
set -e
here=$(cd "$(dirname "$0")" && pwd)
repo=$(dirname "$here")
qml=$repo/qml

# --- The pages ----------------------------------------------------------------

page_MainPage_Play()        { open MainPage; tab 0; }
page_MainPage_Explore()     { open MainPage; tab 1; states any-login; }
page_MainPage_Me()          { open MainPage; tab 2; }
page_PuzzlesPage()          { open PuzzlesPage; states logged-in; }
page_PuzzlePage_daily()     { open 'PuzzlePage { daily: true }'; wait 12; states any-login; }
page_PuzzleDashboardPage()  { open PuzzleDashboardPage; wait 10; states logged-in; }
page_SeekPage()             { open SeekPage; states logged-in; }
page_NewChallengeDialog()   { open NewChallengeDialog; states logged-in; }
page_FriendPickerPage()     { open FriendPickerPage; states logged-in; }
page_FriendsPage()          { open FriendsPage; states logged-in; mock_friends; }
page_ComputerPage()         { open ComputerPage; }
page_HotseatDialog()        { open HotseatDialog; states any-login; }
page_HotseatPage()          { open HotseatPage; states any-login; }
page_BoardEditorPage()      { open BoardEditorPage; states any-login; }
page_AnalysisPage()         { open AnalysisPage; wait 8; states any-login; }
page_TvPage()               { open TvPage; wait 12; states any-login; }
page_PlayerSearchPage()     { open PlayerSearchPage; states logged-in; }
page_ProfilePage()          { open 'ProfilePage { username: "DrNykterstein" }'; wait 12; }
page_GamesHistoryPage()     { open 'GamesHistoryPage { username: "lichess" }'; wait 12; states logged-in; }
page_SettingsPage()         { open SettingsPage; states any-login; }
page_AboutPage()            { open AboutPage; states any-login; }
page_LoginPage()            { open LoginPage; states logged-out; }

# --- What a page function can call --------------------------------------------

open()         { P_OPEN=$1; [[ $P_OPEN == *"{"* ]] || P_OPEN="$P_OPEN { }"; }
tab()          { P_TAB=$1; }
wait()         { P_WAIT=$1; }
states()       { P_STATES=("$@"); }
mock_friends() { P_MOCK_FRIENDS=1; }

# Runs the function of a page for a state, leaving what it asked for in P_*
describe() {
    P_OPEN=; P_TAB=; P_WAIT=6; P_STATES=(logged-out logged-in); P_MOCK_FRIENDS=
    STATE=$2
    "page_${1//-/_}"
}

labels() {
    declare -F | awk '$3 ~ /^page_/ {sub(/^page_/, "", $3); gsub(/_/, "-", $3); print $3}'
}

# --- Which pages: the files a page uses ---------------------------------------

# The file of a component, relative to qml/, if there is one
component_file() {
    local d
    for d in pages components; do
        [ -f "$qml/$d/$1.qml" ] && { echo "$d/$1.qml"; return; }
    done
    return 0
}

# A component's file, the files of the components it instantiates ("Name {", or a
# "Name.qml" that is not a page being pushed) and of the JavaScript it imports, and
# theirs in turn
used_files() {
    local -A seen=()
    local queue=("$(component_file "$1")") f n c
    while [ ${#queue[@]} -gt 0 ]; do
        f=${queue[0]}; queue=("${queue[@]:1}")
        if [ -z "$f" ] || [ -n "${seen[$f]}" ]; then continue; fi
        seen[$f]=1
        for n in $(grep -v 'pageStack\|acceptDestination' "$qml/$f" | grep -oE '\b[A-Z][A-Za-z0-9]*[[:space:]]*\{|"[A-Za-z0-9]+\.qml"' | grep -oE '[A-Za-z0-9]+' | sort -u); do
            c=$(component_file "$n")
            [ -n "$c" ] && queue+=("$c")
        done
        for n in $(grep -oE 'js/[A-Za-z0-9]+\.js' "$qml/$f" | sort -u); do seen[$n]=1; done
    done
    printf '%s\n' "${!seen[@]}"
}

changed_files() {   # relative to qml/
    { git -C "$repo" diff --name-only "$1" -- qml
      git -C "$repo" ls-files --others --exclude-standard -- qml; } | sed 's|^qml/||' | sort -u
}

# --- Options and the choice of pages ------------------------------------------

out=$repo/docs/screenshots
only_state=; install=1; list=0; by_change=0; base=HEAD; by_new=0
names=()
while [ $# -gt 0 ]; do
    case $1 in
        -o) out=$2; shift ;;
        --changed) by_change=1 ;;
        --changed=*) by_change=1; base=${1#--changed=} ;;
        --new) by_new=1 ;;
        --state) only_state=$2; shift
                 case $only_state in
                     logged-in|logged-out|any-login) ;;
                     *) echo "--state: logged-in, logged-out or any-login" >&2; exit 1 ;;
                 esac ;;
        --no-install) install=0 ;;
        --list) list=1 ;;
        -h|--help) sed -n 4,40p "$0"; exit 0 ;;
        -*) echo "Unknown option $1 (see --help)" >&2; exit 1 ;;
        *) names+=("$1") ;;
    esac
    shift
done

if [ $list = 1 ]; then
    for label in $(labels); do
        describe "$label" logged-out
        printf '%s\t%s\n' "$label" "${P_STATES[*]}"
    done
    exit 0
fi

selected=()
if [ ${#names[@]} -eq 0 ] && [ $by_change = 0 ] && [ $by_new = 0 ]; then
    selected=($(labels))
else
    [ $by_change = 1 ] && changed=$(changed_files "$base")
    for label in $(labels); do
        describe "$label" logged-out
        pick=0
        for n in "${names[@]}"; do
            if [ "$n" = "$label" ] || [ "$n" = "${label%%-*}" ]; then pick=1; fi
        done
        if [ $by_change = 1 ]; then
            for f in $(used_files "${P_OPEN%% *}"); do
                grep -qxF "$f" <<< "$changed" && pick=1
            done
        fi
        if [ $by_new = 1 ]; then
            for st in "${P_STATES[@]}"; do
                [ -f "$out/${label}_${st}.png" ] || pick=1
            done
        fi
        [ $pick = 1 ] && selected+=("$label")
    done
    for n in "${names[@]}"; do
        labels | grep -qxE "$n|$n-.*" || echo "No page called $n (see --list)" >&2
    done
fi
if [ $by_change = 1 ] && git -C "$repo" diff --name-only "$base" -- src | grep -q .; then
    echo "Note: C++ sources changed too; --changed only follows QML." >&2
fi
if [ ${#selected[@]} -eq 0 ]; then echo "Nothing to shoot."; exit 0; fi
echo "Pages: ${selected[*]}"

# --- The shots ----------------------------------------------------------------

O="-o StrictHostKeyChecking=no -i $HOME/SailfishOS/vmshare/ssh/private_keys/sdk"
host=root@10.220.220.1
mkdir -p "$out"

if [ $install = 1 ]; then
    scp $O "$(ls -t "$repo"/../build-salichess-*i486*-Debug/RPMS/*.rpm | head -1)" $host:/root/t.rpm >/dev/null
fi
# FriendsPage with made-up friends, for the logged-in shot
mock=$(mktemp)
trap 'rm -f "$mock"' EXIT
sed -e '/model: friends/{r '"$here"'/friends-mock.model
d}' -e 's/friends\.count/4/g; s/friends\.loading/false/g' "$qml/pages/FriendsPage.qml" > "$mock"
scp $O "$mock" $host:/root/FriendsPage.mock.qml >/dev/null

# The commands for the emulator: first the setup, then one begin and finish per shot
script='Q=/usr/share/harbour-salichess/qml'
if [ $install = 1 ]; then
    script+=$'\nrpm -U --force /root/t.rpm 2>&1 | tail -3\nrm -rf /root/qml.orig'
fi
script+=$(cat <<'SETUP'

[ -d /root/qml.orig ] || cp -a $Q /root/qml.orig
rm -rf /home/defaultuser/shots
mkdir -p /home/defaultuser/shots
chown defaultuser /home/defaultuser/shots

# begin STATE: the pristine QML, with the logged-in mock if asked for
begin() {
    rm -rf $Q
    cp -a /root/qml.orig $Q
    if [ "$1" = logged-in ]; then
        find $Q -name '*.qml' -exec sed -i 's/session\.loggedIn/true/g' {} +
    fi
}
# finish NAME OPEN WAIT TAB: start the app on OPEN and take the shot
finish() {
    [ -n "$4" ] && sed -i "s/id: tabs\$/id: tabs\n        Timer { running: true; interval: 800; onTriggered: tabs.currentIndex = $4 }/" $Q/pages/MainPage.qml
    sed -i "s|MainPage { }|$2|" $Q/harbour-salichess.qml
    ps -eo pid,comm | awk '$2 ~ /^harbour-saliche/ {print $1}' | xargs -r kill
    sleep 1
    su -s /bin/sh defaultuser -c 'XDG_RUNTIME_DIR=/run/user/100000 WAYLAND_DISPLAY=../../display/wayland-0 QT_QPA_PLATFORM=wayland QT_LOGGING_TO_CONSOLE=1 nohup /usr/bin/harbour-salichess >/home/defaultuser/log.txt 2>&1 &' </dev/null
    sleep $3
    su -s /bin/sh -g privileged defaultuser -c "XDG_RUNTIME_DIR=/run/user/100000 DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/100000/dbus/user_bus_socket dbus-send --print-reply --session --dest=org.nemomobile.lipstick /org/nemomobile/lipstick/screenshot org.nemomobile.lipstick.saveScreenshot string:/home/defaultuser/shots/$1.png" >/dev/null </dev/null
    echo "$1: $(grep -ci 'qml.*\(error\|warn\)\|TypeError\|ReferenceError' /home/defaultuser/log.txt) QML message(s)"
}
SETUP
)
shots=0
for label in "${selected[@]}"; do
    describe "$label" logged-out
    for state in "${P_STATES[@]}"; do
        [ -n "$only_state" ] && [ "$state" != "$only_state" ] && continue
        shots=$((shots + 1))
        describe "$label" "$state"
        script+=$'\n'"begin $state"
        [ -n "$P_MOCK_FRIENDS" ] && script+=$'\ncp /root/FriendsPage.mock.qml $Q/pages/FriendsPage.qml'
        script+=$'\n'"finish '${label}_${state}' '${P_OPEN//&/\\&}' $P_WAIT '$P_TAB'"
    done
done
if [ $shots -eq 0 ]; then echo "Nothing to shoot in that state."; exit 0; fi
script+=$'\nps -eo pid,comm | awk \'$2 ~ /^harbour-saliche/ {print $1}\' | xargs -r kill\nrm -rf $Q; cp -a /root/qml.orig $Q'

ssh $O $host 'sh -s' <<< "$script"
scp $O "$host:/home/defaultuser/shots/*.png" "$out/" >/dev/null
ssh $O $host 'rm -rf /home/defaultuser/shots'
echo "Screenshots in $(cd "$out" && pwd)"
