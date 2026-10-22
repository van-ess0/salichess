#!/bin/bash
# SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
# SPDX-License-Identifier: GPL-3.0-or-later
# Screenshot the app's pages on the emulator, logged out and with the logged-in
# interface mocked.
#
#   tools/screenshot-pages.sh [options] [PAGE...]
#
#   PAGE            a page label (see --list) or the part before its "-"
#                   (MainPage shoots all three tabs); none means every page
#   --changed[=REF] the pages whose QML changed since REF (default HEAD), counting
#                   uncommitted and untracked files; every page counts the
#                   components it uses, and their components in turn
#   --new           the pages that have no screenshot in the output directory yet
#   --state S       logged-in, logged-out or both (default); a page without a
#                   login difference is shot once whatever S is, unless S names a state
#                   explicitly
#   -o DIR          where the shots go (default: docs/screenshots/ in the repository)
#   --wait N        wait N seconds before each shot instead of the page's own
#   --no-install    do not install the newest i486 RPM first
#   --list          list the pages and the states they are shot in, and exit
#   --dry-run       say what would be shot and exit
#
# Several of PAGE, --changed and --new together shoot the union. Files are named
# PAGE_STATE.png, e.g. PuzzlesPage_logged-out.png, and a new run replaces the old
# shot (git keeps the older ones). A page that looks the same either way gets
# one shot, PAGE_any-login.png (see below).
#
# "Logged in" is a mock: `session.loggedIn` becomes `true` in every installed QML
# file and the session stays empty, so nothing from an account shows; pages
# that want data say so in their function. Pages that need a running game or a
# controller handed to them (GamePage, ChatPage, ComputerGamePage in play,
# AnalysisSummaryPage, ChallengeWaitingPage, SeekWaitingPage) are not here.
# Pages that load from lichess.org use public accounts ("lichess", "DrNykterstein") and need
# the emulator online. The emulator must be running, with its host-only network
# (see the Sailfish notes). Only QML is mapped by --changed: after a C++ change
# shoot the pages by name, or all of them.
#
# To add a page, write a function page_<Label> (a "-" in the label is a "_" in
# the name) in the list below. It describes how to open the page, with:
#   open 'Page { prop: 1 }'   what the app starts on; plain 'Page' means 'Page { }'
#   tab N                     for MainPage: the tab to show
#   wait S                    seconds to wait before the shot (default 6)
#   patch FILE SED            run sed -e SED on the installed qml/FILE, to reach a
#                             state that needs a tap
#   mock_friends              fill FriendsPage with made-up friends
#   login_matters             shoot the page in both states
#   login_irrelevant          shoot the page once, whatever the login state
#   needs_login               the page can only be reached logged in (its way in is
#                             hidden otherwise), so it is shot only in that state
#   needs_logout              likewise, only reachable logged out
# Without either, the page is shot in both states when it or a component it uses
# looks at `session.loggedIn`, and once when none does. A page function can look at $STATE (logged-in or logged-out). That is all: which files the
# page depends on is worked out from what it uses.
set -e
here=$(cd "$(dirname "$0")" && pwd)
repo=$(dirname "$here")
qml=$repo/qml

# --- The pages ------------------------------------------------------------
# login_irrelevant: the page looks the same logged in or out (what differs is
# live data, or nothing the mock can show); login_matters: it does not.
# needs_login / needs_logout: the page cannot be reached in the other state
# (docs/pages.puml shows how each page is reached).

page_MainPage_Play()        { login_matters; open MainPage; tab 0; }
page_MainPage_Explore()     { login_irrelevant; open MainPage; tab 1; }
page_MainPage_Me()          { login_matters; open MainPage; tab 2; }
page_PuzzlesPage()          { needs_login; login_matters; open PuzzlesPage; }
page_PuzzlePage_daily()     { login_irrelevant; open 'PuzzlePage { daily: true }'; wait 12; }
page_PuzzleDashboardPage()  { needs_login; login_irrelevant; open PuzzleDashboardPage; wait 10; }
page_SeekPage()             { needs_login; open SeekPage; }
page_NewChallengeDialog()   { needs_login; open NewChallengeDialog; }
page_FriendPickerPage()     { needs_login; open FriendPickerPage; }
page_FriendsPage()          { needs_login; open FriendsPage; login_matters; [ "$STATE" = logged-in ] && mock_friends; return 0; }
page_ComputerPage()         { login_matters; open ComputerPage; }
page_HotseatDialog()        { login_irrelevant; open HotseatDialog; }
page_HotseatPage()          { login_irrelevant; open HotseatPage; }
page_BoardEditorPage()      { login_irrelevant; open BoardEditorPage; }
page_AnalysisPage()         { login_irrelevant; open AnalysisPage; wait 8; }
page_TvPage()               { login_irrelevant; open TvPage; wait 12; }
page_PlayerSearchPage()     { needs_login; login_irrelevant; open PlayerSearchPage; }
page_ProfilePage()          { login_matters; open 'ProfilePage { username: "DrNykterstein" }'; wait 12; }
page_GamesHistoryPage()     { needs_login; login_irrelevant; open 'GamesHistoryPage { username: "lichess" }'; wait 12; }
page_SettingsPage()         { login_irrelevant; open SettingsPage; }
page_AboutPage()            { open AboutPage; }
page_LoginPage()            { needs_logout; login_irrelevant; open LoginPage; }

# --- What a page function can call ---------------------------------------

open()         { P_OPEN=$1; [[ $P_OPEN == *"{"* ]] || P_OPEN="$P_OPEN { }"; }
tab()          { P_TAB=$1; }
wait()         { P_WAIT=$1; }
mock_friends() { P_MOCK_FRIENDS=1; }
login_matters()     { P_LOGIN=yes; }
login_irrelevant()  { P_LOGIN=no; }
needs_login()       { P_REACH=in; }
needs_logout()      { P_REACH=out; }
patch()        { P_PATCHES+="sed -i '${2//\'/\'\\\'\'}' \"\$Q/$1\""$'\n'; }

# --- Which pages, and which files they use -----------------------------------

labels() {
    declare -F | awk '$3 ~ /^page_/ {sub(/^page_/, "", $3); gsub(/_/, "-", $3); print $3}'
}

# The file of a QML component, relative to qml/, or nothing
resolve() {
    local d
    for d in pages components .; do
        [ -f "$qml/$d/$1.qml" ] && { echo "${d#./}/$1.qml" | sed 's|^\./||; s|^/||'; return; }
    done
    return 0
}

# Every file a component needs: its own, the components it uses and theirs, and
# the JavaScript they import. Components are found by the "Name {" and
# "Name.qml" they are written as.
closure() {
    local -A seen=()
    local queue=("$(resolve "$1")") f n r
    while [ ${#queue[@]} -gt 0 ]; do
        f=${queue[0]}; queue=("${queue[@]:1}")
        if [ -z "$f" ] || [ -n "${seen[$f]}" ]; then continue; fi
        seen[$f]=1
        for n in $(grep -oE '\b[A-Z][A-Za-z0-9]*[[:space:]]*\{|"[A-Za-z0-9]+\.qml"' "$qml/$f" | grep -oE '[A-Za-z0-9]+' | sort -u); do
            r=$(resolve "$n")
            [ -n "$r" ] && queue+=("$r")
        done
        for n in $(grep -oE 'js/[A-Za-z0-9]+\.js' "$qml/$f" | sort -u); do
            seen[$n]=1
        done
    done
    printf '%s\n' "${!seen[@]}"
}

changed_files() {   # relative to qml/
    { git -C "$repo" diff --name-only "$1" -- qml
      git -C "$repo" ls-files --others --exclude-standard -- qml; } | sed 's|^qml/||' | sort -u
}

# --- Options -------------------------------------------------------------------

out=$repo/docs/screenshots
states=(logged-out logged-in)
install=1; list=0; dry=0; wait_all=; by_change=0; base=HEAD; by_new=0
names=()
while [ $# -gt 0 ]; do
    case $1 in
        -o) out=$2; shift ;;
        --changed) by_change=1 ;;
        --changed=*) by_change=1; base=${1#--changed=} ;;
        --new) by_new=1 ;;
        --state) case $2 in
                     both) states=(logged-out logged-in) ;;
                     logged-in|logged-out) states=("$2") ;;
                     *) echo "--state: logged-in, logged-out or both" >&2; exit 1 ;;
                 esac; shift ;;
        --wait) wait_all=$2; shift ;;
        --no-install) install=0 ;;
        --list) list=1 ;;
        --dry-run) dry=1 ;;
        -h|--help) sed -n 4,40p "$0"; exit 0 ;;
        -*) echo "Unknown option $1 (see --help)" >&2; exit 1 ;;
        *) names+=("$1") ;;
    esac
    shift
done

# Run every page function once, to learn what it opens and which files it uses
declare -A page_open page_files page_both page_reach
for label in $(labels); do
    STATE=logged-out; P_OPEN=; P_TAB=; P_WAIT=6; P_PATCHES=; P_MOCK_FRIENDS=; P_LOGIN=; P_REACH=
    "page_${label//-/_}"
    page_reach[$label]=$P_REACH
    page_open[$label]=${P_OPEN%% *}
    page_files[$label]=$(closure "${P_OPEN%% *}" | tr '\n' ' ')
    page_both[$label]=$P_LOGIN
    if [ -z "$P_LOGIN" ]; then
        page_both[$label]=no
        for f in ${page_files[$label]}; do
            case $f in *.qml) grep -q 'session\.loggedIn' "$qml/$f" && page_both[$label]=yes ;; esac
        done
    fi
done

if [ $list = 1 ]; then
    for label in $(labels); do
        case ${page_reach[$label]} in
            in) what='logged-in only' ;;
            out) what='logged-out only' ;;
            *) [ "${page_both[$label]}" = yes ] && what='logged-in and logged-out' || what='once, either way' ;;
        esac
        printf '%s\t%s\n' "$label" "$what"
    done
    exit 0
fi

selected=()
if [ ${#names[@]} -eq 0 ] && [ $by_change = 0 ] && [ $by_new = 0 ]; then
    selected=($(labels))
else
    changed=$(changed_files "$base")
    for label in $(labels); do
        pick=0
        for n in "${names[@]}"; do
            [ "$n" = "$label" ] || [ "$n" = "${label%%-*}" ] && pick=1
        done
        if [ $by_change = 1 ]; then
            for f in ${page_files[$label]}; do
                grep -qxF "$f" <<< "$changed" && pick=1
            done
        fi
        if [ $by_new = 1 ] && ! ls "$out/${label}"_*.png >/dev/null 2>&1; then
            pick=1
        fi
        [ $pick = 1 ] && selected+=("$label")
    done
fi
for n in "${names[@]}"; do
    labels | grep -qxE "$n|$n-.*" || echo "No page called $n (see --list)" >&2
done
if [ $by_change = 1 ] && git -C "$repo" diff --name-only "$base" -- src | grep -q .; then
    echo "Note: C++ sources changed too; --changed only follows QML." >&2
fi
if [ ${#selected[@]} -eq 0 ]; then echo "Nothing to shoot."; exit 0; fi
echo "Pages: ${selected[*]}"
echo "States: ${states[*]}"
[ $dry = 1 ] && exit 0

# --- The shots ---------------------------------------------------------------------

O="-o StrictHostKeyChecking=no -i $HOME/SailfishOS/vmshare/ssh/private_keys/sdk"
host=root@10.220.220.1
mkdir -p "$out"

if [ $install = 1 ]; then
    rpm=$(ls -t "$repo"/../build-salichess-*i486*-Debug/RPMS/*.rpm | head -1)
    scp $O "$rpm" $host:/root/t.rpm >/dev/null
fi
# FriendsPage with made-up friends, for the logged-in shots
mock=$(mktemp)
trap 'rm -f "$mock"' EXIT
sed -e '/model: friends/{r '"$here"'/friends-mock.model
d}' -e 's/friends\.count/4/g; s/friends\.loading/false/g' "$qml/pages/FriendsPage.qml" > "$mock"
scp $O "$mock" $host:/root/FriendsPage.mock.qml >/dev/null

script=$(cat <<'HEAD'
Q=/usr/share/harbour-salichess/qml
HEAD
)
if [ $install = 1 ]; then
    script+=$'\nrpm -U --force /root/t.rpm 2>&1 | tail -3\nrm -rf /root/qml.orig'
fi
script+=$(cat <<'HEAD'

[ -d /root/qml.orig ] || cp -a $Q /root/qml.orig
rm -rf /home/defaultuser/shots
mkdir -p /home/defaultuser/shots
chown defaultuser /home/defaultuser/shots

# begin NAME STATE: the pristine QML, with the logged-in mock if asked for
begin() {
    rm -rf $Q
    cp -a /root/qml.orig $Q
    if [ "$2" = logged-in ]; then
        find $Q -name '*.qml' -exec sed -i 's/session\.loggedIn/true/g' {} +
    fi
}
# finish NAME OPEN WAIT TAB: start the app on OPEN and take the shot
finish() {
    [ -n "$4" ] && sed -i "s/id: tabs\$/id: tabs\n        currentIndex: $4/" $Q/pages/MainPage.qml
    sed -i "s|MainPage { }|$2|" $Q/harbour-salichess.qml
    ps -eo pid,comm | awk '$2 ~ /^harbour-saliche/ {print $1}' | xargs -r kill
    sleep 1
    su -s /bin/sh defaultuser -c 'XDG_RUNTIME_DIR=/run/user/100000 WAYLAND_DISPLAY=../../display/wayland-0 QT_QPA_PLATFORM=wayland QT_LOGGING_TO_CONSOLE=1 nohup /usr/bin/harbour-salichess >/home/defaultuser/log.txt 2>&1 &' </dev/null
    sleep $3
    su -s /bin/sh -g privileged defaultuser -c "XDG_RUNTIME_DIR=/run/user/100000 DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/100000/dbus/user_bus_socket dbus-send --print-reply --session --dest=org.nemomobile.lipstick /org/nemomobile/lipstick/screenshot org.nemomobile.lipstick.saveScreenshot string:/home/defaultuser/shots/$1.png" >/dev/null </dev/null
    echo "$1: $(grep -ci 'qml.*\(error\|warn\)\|TypeError\|ReferenceError' /home/defaultuser/log.txt) QML message(s)"
}
HEAD
)
for label in "${selected[@]}"; do
    # The states the page can be reached in, and wanted
    shoot_states=()
    for st in "${states[@]}"; do
        case ${page_reach[$label]}:$st in
            in:logged-out|out:logged-in) ;;
            *) shoot_states+=("$st") ;;
        esac
    done
    # A page without a login difference is shot once, unless one state was asked for
    if [ -z "${page_reach[$label]}" ] && [ "${page_both[$label]}" = no ] && [ ${#states[@]} -eq 2 ]; then
        shoot_states=(any-login)
    fi
    if [ ${#shoot_states[@]} -eq 0 ]; then
        echo "Skipping $label: it cannot be reached ${states[*]/#/}" >&2
        continue
    fi
    for state in "${shoot_states[@]}"; do
        STATE=$state; P_OPEN=; P_TAB=; P_WAIT=6; P_PATCHES=; P_MOCK_FRIENDS=; P_LOGIN=
        "page_${label//-/_}"
        [ -n "$wait_all" ] && P_WAIT=$wait_all
        script+=$'\n'"begin '${label}_${state}' $state"
        [ -n "$P_MOCK_FRIENDS" ] && script+=$'\ncp /root/FriendsPage.mock.qml $Q/pages/FriendsPage.qml'
        script+=$'\n'"$P_PATCHES"
        script+="finish '${label}_${state}' '${P_OPEN//&/\\&}' $P_WAIT '$P_TAB'"
    done
done
script+=$'\nps -eo pid,comm | awk \'$2 ~ /^harbour-saliche/ {print $1}\' | xargs -r kill\nrm -rf $Q; cp -a /root/qml.orig $Q'

ssh $O $host 'sh -s' <<< "$script"
scp $O "$host:/home/defaultuser/shots/*.png" "$out/" >/dev/null
ssh $O $host 'rm -rf /home/defaultuser/shots'
echo "Screenshots in $(cd "$out" && pwd)"
