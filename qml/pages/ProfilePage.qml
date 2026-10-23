// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
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

    // Profiles are for logged-in users: without an account there is only a way in,
    // under the same header.
    PageHeader {
        visible: !session.loggedIn
        title: page.username
    }

    Column {
        anchors.centerIn: parent
        width: parent.width
        visible: !session.loggedIn
        spacing: Theme.paddingLarge

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * x
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: Theme.secondaryHighlightColor
            font.pixelSize: Theme.fontSizeLarge
            text: qsTr("Log in first")
        }
        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Log in")
            onClicked: pageStack.push(Qt.resolvedUrl("LoginPage.qml"))
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        visible: session.loggedIn
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
            username: session.loggedIn ? page.username : ""
        }

        VerticalScrollDecorator {}
    }
}
