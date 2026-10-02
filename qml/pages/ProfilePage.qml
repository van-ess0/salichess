// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.salichess 1.0
import "../components"
import "../js/Util.js" as Util

// A Lichess player: ratings and how they changed, game results and what
// they say about themselves. Opened from player names, the friends list and
// the player search.
Page {
    id: page

    property string username
    readonly property bool isMe: session.loggedIn && profile.loaded
                                 && session.userId.toLowerCase() === profile.name.toLowerCase()

    // The rating whose history is drawn: the one the user chose, else Blitz,
    // else the first that has a history.
    property string chosenPerf
    readonly property string graphPerf: {
        var names = profile.historyNames
        if (names.indexOf(chosenPerf) >= 0)
            return chosenPerf
        if (names.indexOf("Blitz") >= 0)
            return "Blitz"
        return names.length > 0 ? names[0] : ""
    }

    allowedOrientations: Orientation.All

    UserProfile {
        id: profile
        username: page.username
    }

    function presence() {
        if (!profile.loaded)
            return ""
        if (profile.playing)
            return qsTr("Playing now")
        if (profile.online)
            return qsTr("Online")
        if (profile.seenAt > 0) {
            var seen = new Date(profile.seenAt)
            var today = new Date()
            if (seen.toDateString() === today.toDateString())
                return qsTr("Last seen today")
            return qsTr("Last seen %1").arg(Qt.formatDate(seen, seen.getFullYear() === today.getFullYear() ? "d MMM" : "d MMM yyyy"))
        }
        return ""
    }

    function formatPlayTime(seconds) {
        var hours = seconds / 3600
        if (hours >= 100)
            return qsTr("%1 hours").arg(Math.round(hours))
        if (hours >= 1)
            return qsTr("%1 hours").arg(hours.toFixed(1))
        return qsTr("%1 minutes").arg(Math.round(seconds / 60))
    }

    function percent(part) {
        var all = profile.counts.all
        return all > 0 ? Math.round(100 * part / all) : 0
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: qsTr("Open in browser")
                onClicked: Qt.openUrlExternally("https://lichess.org/@/" + (profile.loaded ? profile.name : page.username))
            }
            MenuItem {
                text: qsTr("Refresh")
                onClicked: profile.reload()
            }
            MenuItem {
                visible: profile.loaded && profile.counts.all > 0
                text: qsTr("Games")
                onClicked: pageStack.push(Qt.resolvedUrl("GamesHistoryPage.qml"), { username: profile.name })
            }
            MenuItem {
                visible: session.loggedIn && profile.loaded && !page.isMe && !profile.closed
                text: qsTr("Challenge")
                onClicked: pageStack.push(Qt.resolvedUrl("NewChallengeDialog.qml"), { username: profile.name })
            }
        }

        Column {
            id: column
            width: page.width

            PageHeader {
                title: profile.loaded ? Util.playerName(profile.name, profile.title) : page.username
                description: page.presence()
            }

            OfflineBanner {
                description: qsTr("Profiles need a connection")
            }

            BusyLabel {
                visible: running
                running: profile.loading
                text: qsTr("Loading profile…")
            }

            Label {
                visible: profile.errorString !== ""
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                color: Theme.errorColor
                text: profile.errorString
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: profile.errorString !== "" && !profile.loading
                text: qsTr("Try again")
                onClicked: profile.reload()
            }

            Label {
                visible: profile.closed
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
                text: qsTr("This account is closed.")
            }

            Label {
                visible: profile.loaded && (profile.bio !== "" || profile.location !== "")
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeSmall
                text: [profile.bio, profile.location].filter(function(s) { return s !== "" }).join("\n")
            }

            // --- Ratings ---

            SectionHeader {
                visible: profile.ratings.length > 0
                text: qsTr("Ratings")
            }

            Repeater {
                model: profile.ratings

                BackgroundItem {
                    id: ratingItem
                    // Rating rows tell; tapping one chooses the graph below.
                    height: Theme.itemSizeExtraSmall
                    highlighted: down || page.graphPerf === modelData.name
                    enabled: profile.historyNames.indexOf(modelData.name) >= 0
                    onClicked: page.chosenPerf = modelData.name

                    Label {
                        anchors {
                            left: parent.left
                            leftMargin: Theme.horizontalPageMargin
                            right: gamesLabel.left
                            rightMargin: Theme.paddingMedium
                            verticalCenter: parent.verticalCenter
                        }
                        truncationMode: TruncationMode.Fade
                        text: modelData.name
                        color: ratingItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                    }

                    Label {
                        id: gamesLabel
                        anchors {
                            right: progLabel.left
                            verticalCenter: parent.verticalCenter
                        }
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.secondaryColor
                        text: modelData.key === "puzzle" ? "" : qsTr("%n game(s)", "", modelData.games)
                    }

                    // The change over the last games, in a column of its own
                    // so the ratings line up.
                    Label {
                        id: progLabel
                        anchors {
                            right: ratingLabel.left
                            verticalCenter: parent.verticalCenter
                        }
                        width: Theme.itemSizeSmall
                        horizontalAlignment: Text.AlignRight
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: modelData.prog > 0 ? "#629924" : "#df5353"
                        text: modelData.prog === 0 ? "" : (modelData.prog > 0 ? "▲" : "▼") + Math.abs(modelData.prog)
                    }

                    Label {
                        id: ratingLabel
                        anchors {
                            right: parent.right
                            rightMargin: Theme.horizontalPageMargin
                            verticalCenter: parent.verticalCenter
                        }
                        width: Theme.itemSizeMedium
                        horizontalAlignment: Text.AlignRight
                        color: Theme.highlightColor
                        font.family: Theme.fontFamilyHeading
                        text: modelData.rating + (modelData.provisional ? "?" : "")
                    }
                }
            }

            // --- Rating history ---

            SectionHeader {
                visible: profile.historyNames.length > 0
                text: qsTr("Rating history")
            }

            ComboBox {
                visible: profile.historyNames.length > 0
                label: qsTr("Rating")
                value: page.graphPerf
                menu: ContextMenu {
                    Repeater {
                        model: profile.historyNames
                        MenuItem {
                            text: modelData
                            onClicked: page.chosenPerf = modelData
                        }
                    }
                }
            }

            RatingGraph {
                width: parent.width
                points: page.graphPerf !== "" ? profile.historyPoints(page.graphPerf) : []
            }

            // --- Games ---

            SectionHeader {
                visible: profile.loaded && profile.counts.all > 0
                text: qsTr("Games")
            }

            Item {
                visible: profile.loaded && profile.counts.all > 0
                width: parent.width
                height: resultBar.height + resultLabels.height + Theme.paddingLarge

                Row {
                    id: resultBar
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    height: Theme.paddingLarge
                    Rectangle {
                        width: parent.width * (profile.counts.all > 0 ? profile.counts.win / profile.counts.all : 0)
                        height: parent.height
                        color: "#629924"
                    }
                    Rectangle {
                        width: parent.width * (profile.counts.all > 0 ? profile.counts.draw / profile.counts.all : 0)
                        height: parent.height
                        color: Theme.secondaryColor
                    }
                    Rectangle {
                        width: parent.width * (profile.counts.all > 0 ? profile.counts.loss / profile.counts.all : 0)
                        height: parent.height
                        color: "#df5353"
                    }
                }

                Row {
                    id: resultLabels
                    anchors.top: resultBar.bottom
                    anchors.topMargin: Theme.paddingSmall
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    Label {
                        width: parent.width / 3
                        font.pixelSize: Theme.fontSizeSmall
                        text: qsTr("%1 wins").arg(profile.counts.win) + " (" + page.percent(profile.counts.win) + "%)"
                    }
                    Label {
                        width: parent.width / 3
                        horizontalAlignment: Text.AlignHCenter
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.secondaryColor
                        text: qsTr("%1 draws").arg(profile.counts.draw)
                    }
                    Label {
                        width: parent.width / 3
                        horizontalAlignment: Text.AlignRight
                        font.pixelSize: Theme.fontSizeSmall
                        text: qsTr("%1 losses").arg(profile.counts.loss)
                    }
                }
            }

            DetailItem {
                visible: profile.loaded && profile.counts.all > 0
                label: qsTr("Games played")
                value: profile.counts.all + (profile.counts.rated > 0 ? " • " + qsTr("%1 rated").arg(profile.counts.rated) : "")
            }

            DetailItem {
                visible: profile.loaded && profile.playTime > 0
                label: qsTr("Time playing")
                value: page.formatPlayTime(profile.playTime)
            }

            DetailItem {
                visible: profile.loaded && profile.createdAt > 0
                label: qsTr("Member since")
                value: Qt.formatDate(new Date(profile.createdAt), "d MMM yyyy")
            }

            DetailItem {
                visible: profile.loaded && profile.followers > 0
                label: qsTr("Followers")
                value: profile.followers
            }

            Item {
                width: 1
                height: Theme.paddingMedium
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.paddingLarge
                visible: profile.loaded && !profile.closed

                Button {
                    visible: session.loggedIn && !page.isMe
                    text: qsTr("Challenge")
                    onClicked: pageStack.push(Qt.resolvedUrl("NewChallengeDialog.qml"), { username: profile.name })
                }
                Button {
                    visible: profile.counts.all > 0
                    text: qsTr("Games")
                    onClicked: pageStack.push(Qt.resolvedUrl("GamesHistoryPage.qml"), { username: profile.name })
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
