// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components"

// The players the user follows on Lichess, as tiles; tapping one opens their profile.
Page {
    id: page

    allowedOrientations: Orientation.All

    // The games against each friend are fetched for as long as the page is open
    Component.onCompleted: friends.refresh(true)
    Component.onDestruction: friends.stopDetails()

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: qsTr("Refresh")
                onClicked: friends.refresh(true)
            }
        }

        Column {
            id: column
            width: page.width

            Column {
                id: top
                width: parent.width

                PageHeader {
                    title: qsTr("Friends")
                    description: qsTr("Players you follow on Lichess")
                }

                OfflineBanner {
                    description: qsTr("Friends need a connection")
                }

                BusyLabel {
                    visible: running
                    running: friends.loading && friends.count === 0
                    text: qsTr("Loading friends…")
                }

                Label {
                    visible: !friends.loading && friends.count === 0
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    topPadding: Theme.paddingLarge
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    color: Theme.secondaryHighlightColor
                    font.pixelSize: Theme.fontSizeLarge
                    text: qsTr("You are not following anyone yet")
                }
            }

            // A short list sits in the middle of the screen, in reach of a thumb.
            Item {
                width: 1
                height: Math.max(0, (page.height - grid.height) / 2 - top.height)
            }

            TileGrid {
                id: grid
                Repeater {
                    model: friends
                    FriendTile {
                        username: model.username
                        title: model.title
                        online: model.online
                        playing: model.playing
                        games: model.gamesCount
                        lastGameAt: model.lastGameAt
                        seenAt: model.seenAt
                        ratings: {
                            var list = []
                            if (model.blitzRating > 0)
                                list.push({ name: qsTr("Blitz"), rating: model.blitzRating })
                            if (model.rapidRating > 0)
                                list.push({ name: qsTr("Rapid"), rating: model.rapidRating })
                            if (model.classicalRating > 0)
                                list.push({ name: qsTr("Classical"), rating: model.classicalRating })
                            return list
                        }
                        onClicked: pageStack.push(Qt.resolvedUrl("ProfilePage.qml"), { username: model.userId })
                    }
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
