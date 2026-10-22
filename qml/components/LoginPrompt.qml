// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0

// Shown on the main page's tabs while nobody is logged in.
Item {
    visible: !session.loggedIn
    width: parent.width
    height: loginColumn.height + 2 * Theme.paddingLarge

    Column {
        id: loginColumn
        y: Theme.paddingLarge
        width: parent.width
        spacing: Theme.paddingLarge

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * x
            wrapMode: Text.Wrap
            color: Theme.secondaryHighlightColor
            font.pixelSize: Theme.fontSizeMedium
            text: qsTr("Log in with your Lichess account to play games with friends and to keep track of your puzzle rating.")
        }
        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Log in")
            onClicked: pageStack.push(Qt.resolvedUrl("../pages/LoginPage.qml"))
        }
    }
}
