// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0

// A real-time seek with a custom clock. |options| holds the other settings
// from the seek page; accepting continues to the waiting page.
Dialog {
    id: dialog

    property var options

    readonly property var minuteOptions: [3, 4, 5, 6, 7, 8, 9, 10, 12, 15, 20, 25, 30, 40, 45, 60, 75, 90, 120, 150, 180]
    readonly property var incrementOptions: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 15, 20, 25, 30, 40, 45, 60]
    readonly property int minutes: minuteOptions[minutesSlider.value]
    readonly property int increment: incrementOptions[incrementSlider.value]
    // Lichess estimates game length as initial time + 40 × increment. The
    // shortest clock here (3+0) is blitz already.
    readonly property int estimate: minutes * 60 + 40 * increment
    readonly property string speed: estimate < 480 ? "blitz" : estimate < 1500 ? "rapid" : "classical"

    allowedOrientations: Orientation.All
    canAccept: speed !== "blitz"
    acceptDestination: Qt.resolvedUrl("SeekWaitingPage.qml")
    acceptDestinationAction: PageStackAction.Replace
    // The waiting page exists before the user accepts (see NewChallengeDialog),
    // so the settings are handed over now.
    onAccepted: {
        var result = {}
        for (var key in options)
            result[key] = options[key]
        result.minutes = minutes
        result.increment = increment
        acceptDestinationInstance.options = result
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width

            DialogHeader {
                acceptText: qsTr("Seek")
                title: qsTr("Custom time control")
            }

            Slider {
                id: minutesSlider
                width: parent.width
                minimumValue: 0
                maximumValue: dialog.minuteOptions.length - 1
                stepSize: 1
                value: 7 // 10 minutes
                label: qsTr("Minutes per side")
                valueText: dialog.minutes
            }

            Slider {
                id: incrementSlider
                width: parent.width
                minimumValue: 0
                maximumValue: dialog.incrementOptions.length - 1
                stepSize: 1
                value: 0
                label: qsTr("Increment in seconds")
                valueText: dialog.increment
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: dialog.speed === "blitz" ? Theme.errorColor : Theme.secondaryHighlightColor
                text: {
                    switch (dialog.speed) {
                    case "blitz": return qsTr("Blitz and bullet games can't be played with random opponents through the Lichess Board API. Add time or increment.")
                    case "rapid": return qsTr("Rapid game")
                    default: return qsTr("Classical game")
                    }
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
