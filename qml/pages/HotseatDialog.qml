// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0

// Settings for a game two people play on this phone. Accepting continues to
// the board; the choices are kept for the next game.
Dialog {
    id: dialog

    readonly property var presets: [
        { "seconds": 180, "increment": 2 },
        { "seconds": 300, "increment": 0 },
        { "seconds": 600, "increment": 0 },
        { "seconds": 900, "increment": 10 },
        { "seconds": 0, "increment": 0 }
    ]
    readonly property int customIndex: presets.length
    readonly property bool custom: timeBox.currentIndex === customIndex

    readonly property var minuteOptions: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 15, 20, 25, 30, 40, 45, 60, 90, 120, 180]
    readonly property var incrementOptions: [0, 1, 2, 3, 5, 10, 15, 20, 30, 45, 60]

    readonly property int seconds: custom ? minuteOptions[minutesSlider.value] * 60
                                          : presets[timeBox.currentIndex].seconds
    readonly property int increment: custom ? incrementOptions[incrementSlider.value]
                                            : presets[timeBox.currentIndex].increment

    // The preset the stored settings stand for, or "custom" if they match none.
    function storedIndex() {
        for (var i = 0; i < presets.length; ++i) {
            if (presets[i].seconds === appSettings.hotseatSeconds
                    && presets[i].increment === appSettings.hotseatIncrement)
                return i
        }
        return customIndex
    }

    function nearest(options, value) {
        var best = 0
        for (var i = 0; i < options.length; ++i) {
            if (Math.abs(options[i] - value) < Math.abs(options[best] - value))
                best = i
        }
        return best
    }

    allowedOrientations: Orientation.All
    acceptDestination: Qt.resolvedUrl("HotseatPage.qml")
    acceptDestinationAction: PageStackAction.Replace

    // Silica creates the destination page as soon as the dialog opens, so the
    // settings are handed over on accept; the board starts the game once it
    // is active.
    onAccepted: {
        appSettings.hotseatSeconds = dialog.seconds
        appSettings.hotseatIncrement = dialog.increment
        appSettings.hotseatAutoClock = autoSwitch.checked

        acceptDestinationInstance.initialSeconds = dialog.seconds
        acceptDestinationInstance.increment = dialog.increment
        acceptDestinationInstance.autoClock = autoSwitch.checked
        acceptDestinationInstance.whiteAtBottom = colorBox.currentIndex === 0
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width

            DialogHeader {
                acceptText: qsTr("Play")
                title: qsTr("Pass and play")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeSmall
                text: qsTr("Two players share this phone. Lay it on the table between you: each side's pieces face its own player, and each of you gets a clock on your own edge of the screen.")
            }

            ComboBox {
                id: timeBox
                label: qsTr("Time control")
                currentIndex: dialog.storedIndex()
                menu: ContextMenu {
                    MenuItem { text: qsTr("3+2 • Blitz") }
                    MenuItem { text: qsTr("5+0 • Blitz") }
                    MenuItem { text: qsTr("10+0 • Rapid") }
                    MenuItem { text: qsTr("15+10 • Classical") }
                    MenuItem { text: qsTr("No clock") }
                    MenuItem { text: qsTr("Custom") }
                }
            }

            Slider {
                id: minutesSlider
                visible: dialog.custom
                width: parent.width
                minimumValue: 0
                maximumValue: dialog.minuteOptions.length - 1
                stepSize: 1
                value: dialog.nearest(dialog.minuteOptions, Math.max(1, appSettings.hotseatSeconds / 60))
                label: qsTr("Minutes per side")
                valueText: dialog.minuteOptions[value]
            }

            Slider {
                id: incrementSlider
                visible: dialog.custom
                width: parent.width
                minimumValue: 0
                maximumValue: dialog.incrementOptions.length - 1
                stepSize: 1
                value: dialog.nearest(dialog.incrementOptions, appSettings.hotseatIncrement)
                label: qsTr("Increment in seconds")
                valueText: dialog.incrementOptions[value]
            }

            ComboBox {
                id: colorBox
                label: qsTr("Nearest player")
                description: qsTr("Who sits at the bottom edge of the screen")
                menu: ContextMenu {
                    MenuItem { text: qsTr("White") }
                    MenuItem { text: qsTr("Black") }
                }
            }

            TextSwitch {
                id: autoSwitch
                text: qsTr("Switch the clock automatically")
                description: qsTr("Otherwise the clock works like a real one: play your move, then tap your own clock to end your turn.")
                checked: appSettings.hotseatAutoClock
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryHighlightColor
                visible: dialog.seconds === 0
                text: qsTr("Without a clock the buttons only say whose turn it is.")
            }
        }

        VerticalScrollDecorator {}
    }
}
