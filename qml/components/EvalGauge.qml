// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0

// How the position stands, as a bar: white's share of it fills from the
// left. The Lichess app puts this beside the board; on a phone the board
// wants the whole width, so it sits under it.
Item {
    id: gauge

    // 0..100, white's chances.
    property real winPercent: 50
    // The evaluation in words, e.g. "+1.25"; drawn on whichever side has
    // room for it.
    property string text

    // Fixed colours: the bar reads as "white against black" whatever the
    // ambience is.
    readonly property color whiteColor: "#e8e8e8"
    readonly property color blackColor: "#3a3a3a"
    readonly property bool whiteAhead: winPercent >= 50

    height: Theme.paddingLarge

    Rectangle {
        anchors.fill: parent
        color: gauge.blackColor
    }

    Rectangle {
        width: parent.width * Math.max(0, Math.min(100, gauge.winPercent)) / 100
        height: parent.height
        color: gauge.whiteColor

        Behavior on width {
            enabled: appSettings.animatePieces
            NumberAnimation { duration: 200; easing.type: Easing.OutQuad }
        }
    }

    Label {
        anchors {
            left: gauge.whiteAhead ? parent.left : undefined
            right: gauge.whiteAhead ? undefined : parent.right
            leftMargin: Theme.paddingSmall
            rightMargin: Theme.paddingSmall
            verticalCenter: parent.verticalCenter
        }
        visible: gauge.text !== ""
        font.pixelSize: Theme.fontSizeTiny
        font.bold: true
        color: gauge.whiteAhead ? gauge.blackColor : gauge.whiteColor
        text: gauge.text
    }
}
