// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../js/Util.js" as Util

// Players the user follows; picking one returns the name via selected().
Page {
    id: page

    signal selected(string username)

    allowedOrientations: Orientation.All

    Component.onCompleted: friends.refresh()

    SilicaListView {
        id: list
        anchors.fill: parent
        model: friends

        header: PageHeader {
            title: qsTr("Friends")
            description: qsTr("Players you follow on Lichess")
        }

        PullDownMenu {
            MenuItem {
                text: qsTr("Refresh")
                onClicked: friends.refresh()
            }
        }

        delegate: BackgroundItem {
            id: item
            height: Theme.itemSizeSmall
            onClicked: {
                page.selected(model.username)
                pageStack.pop()
            }

            Rectangle {
                id: dot
                x: Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.paddingMedium
                height: width
                radius: width / 2
                color: model.online ? Theme.presenceColor(Theme.PresenceAvailable)
                                    : Theme.presenceColor(Theme.PresenceOffline)
            }

            Label {
                anchors {
                    left: dot.right
                    leftMargin: Theme.paddingMedium
                    right: presence.left
                    rightMargin: Theme.paddingMedium
                    verticalCenter: parent.verticalCenter
                }
                truncationMode: TruncationMode.Fade
                text: Util.playerName(model.username, model.title)
                color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
            }

            Label {
                id: presence
                anchors {
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
                text: model.playing ? qsTr("playing") : (model.online ? qsTr("online") : "")
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0 && !friends.loading
            text: qsTr("No friends yet")
            hintText: qsTr("Follow players on lichess.org to see them here, or type a username.")
        }

        BusyIndicator {
            anchors.centerIn: parent
            size: BusyIndicatorSize.Large
            running: friends.loading && list.count === 0
        }

        VerticalScrollDecorator {}
    }
}
