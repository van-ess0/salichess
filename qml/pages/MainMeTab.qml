// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components"

// The user's own profile, with the way on to their games and their friends.
MainTab {
    onRefreshed: profileContent.profile.reload()

    LoginPrompt {}

    ProfileContent {
        id: profileContent
        visible: session.loggedIn
        username: session.loggedIn ? session.userId : ""
        showActions: false

        TileGrid {
            visible: session.loggedIn

            MenuTile {
                onClicked: pageStack.push(Qt.resolvedUrl("GamesHistoryPage.qml"))
                icon: "image://theme/icon-m-history"
                title: qsTr("Games history")
                subtitle: qsTr("Look back at the games you played")
            }

            MenuTile {
                onClicked: pageStack.push(Qt.resolvedUrl("FriendsPage.qml"))
                icon: "image://theme/icon-m-people"
                title: qsTr("Friends")
                subtitle: qsTr("The players you follow")
            }
        }
    }
}
