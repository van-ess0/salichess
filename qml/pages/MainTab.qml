// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components"

// One tab of the main page: the offline banner and the pull-down menu around
// whatever the tab puts inside.
SilicaFlickable {
    id: tab

    default property alias content: column.data
    // Room at the top for the tab bar, which floats over the tabs
    property real topInset

    // Sent by the pull-down menu's Refresh, for tabs with data of their own.
    signal refreshed()

    contentHeight: column.height + Theme.paddingLarge

    PullDownMenu {
        MenuItem {
            enabled: !session.busy
            text: session.loggedIn ? qsTr("Log out") : qsTr("Log in")
            onClicked: {
                if (session.loggedIn)
                    Remorse.popupAction(tab, qsTr("Logging out"), function() { session.logout() })
                else
                    pageStack.push(Qt.resolvedUrl("LoginPage.qml"))
            }
        }
        MenuItem {
            text: qsTr("About")
            onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
        }
        MenuItem {
            text: qsTr("Settings")
            onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
        }
        MenuItem {
            visible: session.loggedIn
            text: qsTr("Refresh")
            onClicked: {
                challenges.refresh()
                ongoingGames.refresh()
                session.refreshAccount()
                tab.refreshed()
            }
        }
    }

    Column {
        id: column
        width: tab.width
        // Short tabs sit in the middle of the screen, in reach of a thumb.
        y: Math.max(0, (tab.height - height) / 2)

        Item {
            width: 1
            height: tab.topInset
        }

        OfflineBanner {
            description: puzzleStore.count > 0
                         ? qsTr("%n puzzle(s) are ready to play offline", "", puzzleStore.count)
                         : qsTr("Games and puzzles need a connection")
        }
    }

    VerticalScrollDecorator {}
}
