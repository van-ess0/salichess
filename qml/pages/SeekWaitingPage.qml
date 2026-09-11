// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.salichess 1.0

// Shows the seek for a random opponent (lobbySeek). Set |options| to start a
// new seek once the page is active. Leaving the page keeps a real-time seek
// open; the app opens the game when an opponent is found (see
// harbour-salichess.qml).
Page {
    id: page

    property var options
    // For harbour-salichess.qml: this page gives way to the game.
    readonly property bool showsSeek: true

    property var lastOptions
    readonly property int seekState: lobbySeek.state
    readonly property bool ended: seekState === LobbySeek.Expired || seekState === LobbySeek.Canceled
                                  || seekState === LobbySeek.Failed

    allowedOrientations: Orientation.All

    // Not on creation: a dialog creates this page before the user accepts.
    onStatusChanged: {
        if (status === PageStatus.Active && options) {
            lastOptions = options
            options = undefined
            lobbySeek.create(lastOptions)
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Random opponent")
                description: lobbySeek.description
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                size: BusyIndicatorSize.Large
                running: lobbySeek.seeking
                visible: running
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeLarge
                color: Theme.highlightColor
                text: {
                    switch (page.seekState) {
                    case LobbySeek.Seeking:
                        return lobbySeek.correspondence ? qsTr("Posting your seek…") : qsTr("Looking for an opponent…")
                    case LobbySeek.Found:
                        return qsTr("Opponent found")
                    case LobbySeek.Posted:
                        return qsTr("Your seek is in the lobby")
                    case LobbySeek.Expired:
                        return qsTr("No opponent was found")
                    case LobbySeek.Canceled:
                        return qsTr("The seek was canceled")
                    case LobbySeek.Failed:
                        return qsTr("The seek failed")
                    default:
                        return ""
                    }
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
                visible: text !== ""
                text: {
                    switch (page.seekState) {
                    case LobbySeek.Failed:
                        return lobbySeek.errorString
                    case LobbySeek.Posted:
                        return qsTr("When someone joins, the game appears under Ongoing games. The seek can only be withdrawn on lichess.org.")
                    case LobbySeek.Seeking:
                        return lobbySeek.correspondence ? ""
                                : qsTr("You can leave this page: the seek stays open while salichess runs, and the game opens as soon as an opponent is found.")
                    default:
                        return ""
                    }
                }
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: lobbySeek.seeking && !lobbySeek.correspondence
                text: qsTr("Cancel seek")
                onClicked: lobbySeek.cancel()
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: page.ended && page.lastOptions !== undefined
                text: qsTr("Try again")
                onClicked: lobbySeek.create(page.lastOptions)
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: !lobbySeek.seeking
                text: qsTr("Back")
                onClicked: pageStack.pop()
            }
        }
    }
}
