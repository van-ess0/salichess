// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.salichess 1.0
import "../components"

// A Lichess player: ratings and how they changed, game results and what
// they say about themselves. Opened from player names, the friends list and
// the player search.
Page {
    id: page

    property string username
    property alias profile: content.profile
    readonly property bool isMe: content.isMe

    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height + Theme.paddingLarge

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
                text: qsTr("Games history")
                onClicked: pageStack.push(Qt.resolvedUrl("GamesHistoryPage.qml"), { username: profile.name })
            }
            MenuItem {
                visible: session.loggedIn && profile.loaded && !page.isMe && !profile.closed
                text: qsTr("Challenge")
                onClicked: pageStack.push(Qt.resolvedUrl("NewChallengeDialog.qml"), { username: profile.name })
            }
        }

        ProfileContent {
            id: content
            username: page.username
        }

        VerticalScrollDecorator {}
    }
}
