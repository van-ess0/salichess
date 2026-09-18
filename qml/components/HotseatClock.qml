// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.salichess 1.0
import "../js/Util.js" as Util

// One player's end of the hotseat screen: their clock, and the button that
// ends their turn. It fills the space left over beside the board, and is
// drawn upside-down for the player sitting opposite, so that both read it
// the right way up from where they sit.
BackgroundItem {
    id: clock

    property QtObject controller
    property string color // "white" or "black"
    property bool inverted: false

    signal newGameRequested()

    readonly property bool running: controller && controller.runningClock === color
    readonly property bool awaiting: controller && controller.awaitingPress === color
    readonly property bool toMove: controller && !controller.gameOver && controller.sideToMove === color
    readonly property int timeMs: !controller ? 0
                                              : (color === "white" ? controller.whiteTime : controller.blackTime)
    readonly property bool lowTime: running && timeMs < 15000
    readonly property string name: color === "white" ? qsTr("White") : qsTr("Black")

    // Tapping only does something when there is something to do: end this
    // player's turn, take a paused game up again, or start the next one.
    enabled: controller && (awaiting || controller.gameOver
                            || controller.state === HotseatController.Paused)

    onClicked: {
        if (!controller)
            return
        if (controller.awaitingPress === color)
            controller.pressClock()
        else if (controller.gameOver)
            newGameRequested()
        else if (controller.state === HotseatController.Paused)
            controller.resume()
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: Theme.paddingSmall
        radius: Theme.paddingMedium
        color: {
            if (clock.down)
                return Theme.rgba(Theme.highlightBackgroundColor, 0.5)
            if (clock.awaiting)
                return Theme.rgba(Theme.highlightBackgroundColor, 0.35)
            if (clock.lowTime)
                return Theme.rgba(Theme.errorColor, 0.45)
            if (clock.running)
                return Theme.rgba(Theme.highlightBackgroundColor, 0.18)
            return Theme.rgba(Theme.primaryColor, 0.06)
        }
        border.width: clock.toMove ? Math.round(Theme.paddingSmall / 3) : 0
        border.color: Theme.rgba(Theme.highlightColor, 0.6)
    }

    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.paddingLarge
        rotation: clock.inverted ? 180 : 0

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: Theme.fontSizeHuge
            font.family: Theme.fontFamilyHeading
            // The result and the colour names are words rather than digits,
            // so let them shrink instead of running off the button.
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: Theme.fontSizeMedium
            color: {
                if (!clock.controller || clock.controller.gameOver)
                    return Theme.highlightColor
                if (clock.lowTime)
                    return Theme.errorColor
                return clock.running ? Theme.primaryColor : Theme.secondaryColor
            }
            text: {
                if (!clock.controller)
                    return ""
                if (clock.controller.gameOver)
                    return clock.controller.resultTextFor(clock.color)
                if (clock.controller.untimed)
                    return clock.name
                return Util.formatClock(clock.timeMs)
            }
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeSmall
            color: clock.awaiting ? Theme.highlightColor : Theme.secondaryColor
            text: {
                if (!clock.controller)
                    return ""
                if (clock.controller.gameOver)
                    return qsTr("Tap for a new game")
                if (clock.awaiting)
                    return qsTr("%1 — tap to end your turn").arg(clock.name)
                if (clock.controller.state === HotseatController.Paused)
                    return qsTr("Paused — tap to go on")
                if (clock.toMove)
                    return qsTr("%1 to move").arg(clock.name)
                return clock.name
            }
        }
    }
}
