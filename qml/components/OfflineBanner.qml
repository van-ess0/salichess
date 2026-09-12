// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0

// Says that nothing can reach lichess.org at the moment. "connection" is the
// LichessApi: it is offline once a request failed to leave the phone, and
// online again as soon as one gets an answer.
Item {
    id: banner

    // Shown under the text, e.g. what still works offline.
    property string description

    width: parent ? parent.width : 0
    height: visible ? content.height + 2 * Theme.paddingMedium : 0
    visible: connection.offline || connection.rateLimited

    Rectangle {
        anchors.fill: parent
        color: Theme.rgba(Theme.errorColor, 0.15)
    }

    Row {
        id: content
        anchors.verticalCenter: parent.verticalCenter
        x: Theme.horizontalPageMargin
        width: parent.width - 2 * x
        spacing: Theme.paddingMedium

        Image {
            id: icon
            anchors.verticalCenter: parent.verticalCenter
            source: "image://theme/icon-s-high-importance?" + Theme.errorColor
        }

        Column {
            width: content.width - icon.width - content.spacing
            anchors.verticalCenter: parent.verticalCenter

            Label {
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.primaryColor
                text: connection.offline ? qsTr("No internet connection")
                                         : qsTr("Lichess is asking salichess to slow down")
            }
            Label {
                width: parent.width
                wrapMode: Text.Wrap
                visible: text !== ""
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: connection.offline ? banner.description
                                         : qsTr("Requests continue in a minute")
            }
        }
    }
}
