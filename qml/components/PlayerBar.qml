// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../js/Time.js" as Time
import "../js/Util.js" as Util

// Player name, rating and clock above or below the board.
Item {
    id: bar

    property var player: ({})
    property bool showClock: true
    property int timeMs: 0
    // Correspondence: timeMs is the time left for the current move.
    property bool turnTimer: false
    readonly property int lowTimeMs: turnTimer ? 3 * 3600 * 1000 : 10000
    property bool running: false
    property bool toMove: false
    property string extraText

    width: parent ? parent.width : 0
    height: Theme.itemSizeSmall

    Row {
        anchors {
            left: parent.left
            leftMargin: Theme.horizontalPageMargin
            right: clock.left
            rightMargin: Theme.paddingMedium
            verticalCenter: parent.verticalCenter
        }
        spacing: Theme.paddingSmall

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.paddingSmall
            height: width
            radius: width / 2
            color: bar.toMove ? Theme.highlightColor : "transparent"
        }
        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: bar.player && bar.player.title ? bar.player.title : ""
            visible: text !== ""
            color: Theme.highlightColor
            font.bold: true
        }
        Label {
            id: nameLabel
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, parent.width - x - ratingLabel.width - Theme.paddingSmall)
            text: bar.player && bar.player.name ? bar.player.name : ""
            truncationMode: TruncationMode.Fade
            color: bar.toMove ? Theme.highlightColor : Theme.primaryColor
        }
        Label {
            id: ratingLabel
            anchors.verticalCenter: parent.verticalCenter
            text: {
                if (bar.extraText)
                    return bar.extraText
                if (!bar.player || !bar.player.rating)
                    return ""
                return bar.player.rating + (bar.player.provisional ? "?" : "")
            }
            color: Theme.secondaryColor
            font.pixelSize: Theme.fontSizeSmall
        }
    }

    Rectangle {
        id: clock
        visible: bar.showClock
        anchors {
            right: parent.right
            rightMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }
        width: clockLabel.implicitWidth + 2 * Theme.paddingMedium
        height: clockLabel.implicitHeight + Theme.paddingSmall
        radius: Theme.paddingSmall
        color: {
            if (!bar.running)
                return Theme.rgba(Theme.primaryColor, 0.1)
            if (bar.timeMs < bar.lowTimeMs)
                return Theme.rgba(Theme.errorColor, 0.6)
            return Theme.rgba(Theme.highlightBackgroundColor, 0.6)
        }

        Label {
            id: clockLabel
            anchors.centerIn: parent
            text: bar.turnTimer ? Time.formatTurnTime(bar.timeMs) : Util.formatClock(bar.timeMs)
            font.pixelSize: Theme.fontSizeLarge
            font.family: Theme.fontFamilyHeading
            color: bar.running ? Theme.primaryColor : Theme.secondaryColor
        }
    }
}
