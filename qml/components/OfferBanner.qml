// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0

// "Your opponent offers a draw  [Accept] [Decline]"
Column {
    id: banner

    property alias text: label.text
    signal accepted()
    signal declined()

    width: parent ? parent.width : 0
    spacing: Theme.paddingSmall

    Label {
        id: label
        x: Theme.horizontalPageMargin
        width: parent.width - 2 * x
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        color: Theme.highlightColor
    }

    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: Theme.paddingLarge

        Button {
            text: qsTr("Accept")
            onClicked: banner.accepted()
        }
        Button {
            text: qsTr("Decline")
            onClicked: banner.declined()
        }
    }
}
