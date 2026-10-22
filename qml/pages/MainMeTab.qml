// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components"

MainTab {

    LoginPrompt {}

    TileGrid {
        MenuTile {
            visible: session.loggedIn
            onClicked: pageStack.push(Qt.resolvedUrl("ProfilePage.qml"), { username: session.userId })
            icon: "image://theme/icon-m-contact"
            title: qsTr("My profile")
            subtitle: qsTr("Your ratings and results")
        }

        MenuTile {
            visible: session.loggedIn
            onClicked: pageStack.push(Qt.resolvedUrl("GamesHistoryPage.qml"))
            icon: "image://theme/icon-m-history"
            title: qsTr("Games")
            subtitle: qsTr("Look back at the games you played")
        }
    }
}
