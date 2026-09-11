// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0

// Settings for a challenge to a friend. Accepting sends the challenge and
// continues to the waiting page.
Dialog {
    id: dialog

    property string username

    // Board API games can't be bullet, so the fastest option is 3 minutes.
    readonly property var minuteOptions: [3, 4, 5, 6, 7, 8, 9, 10, 12, 15, 20, 25, 30, 40, 45, 60, 75, 90, 120, 150, 180]
    readonly property var incrementOptions: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 15, 20, 25, 30, 40, 45, 60]
    readonly property var dayOptions: [1, 2, 3, 5, 7, 10, 14]
    readonly property var colorKeys: ["random", "white", "black"]

    readonly property bool correspondence: modeBox.currentIndex === 1
    readonly property int minutes: minuteOptions[minutesSlider.value]
    readonly property int increment: incrementOptions[incrementSlider.value]
    // Lichess estimates game length as initial time + 40 × increment.
    readonly property int estimate: minutes * 60 + 40 * increment
    readonly property string speed: correspondence ? "correspondence"
                                  : estimate < 180 ? "bullet"
                                  : estimate < 480 ? "blitz"
                                  : estimate < 1500 ? "rapid" : "classical"
    readonly property bool validName: /^[A-Za-z0-9_-]{2,30}$/.test(nameField.text.trim())

    allowedOrientations: Orientation.All
    canAccept: validName && speed !== "bullet"
    acceptDestination: Qt.resolvedUrl("ChallengeWaitingPage.qml")
    acceptDestinationAction: PageStackAction.Replace
    // Silica creates the destination page as soon as the dialog opens (so it
    // can be peeked at), so the settings are handed over on accept; the
    // waiting page sends the challenge once it is active.
    onAccepted: {
        acceptDestinationInstance.username = nameField.text.trim()
        acceptDestinationInstance.options = {
            "correspondence": correspondence,
            "minutes": minutes,
            "increment": increment,
            "days": dayOptions[daysBox.currentIndex],
            "rated": ratedSwitch.checked,
            "color": colorKeys[colorBox.currentIndex]
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width

            DialogHeader {
                acceptText: qsTr("Challenge")
                title: qsTr("Play with a friend")
            }

            Row {
                width: parent.width

                TextField {
                    id: nameField
                    width: parent.width - friendsButton.width - Theme.horizontalPageMargin
                    label: qsTr("Lichess username")
                    placeholderText: qsTr("Username of your friend")
                    text: dialog.username
                    inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                    EnterKey.iconSource: "image://theme/icon-m-enter-close"
                    EnterKey.onClicked: focus = false
                }

                IconButton {
                    id: friendsButton
                    anchors.verticalCenter: nameField.verticalCenter
                    icon.source: "image://theme/icon-m-people"
                    onClicked: {
                        var picker = pageStack.push(Qt.resolvedUrl("FriendPickerPage.qml"))
                        picker.selected.connect(function(name) { nameField.text = name })
                    }
                }
            }

            ComboBox {
                id: modeBox
                label: qsTr("Time control")
                menu: ContextMenu {
                    MenuItem { text: qsTr("Real time") }
                    MenuItem { text: qsTr("Correspondence") }
                }
            }

            Slider {
                id: minutesSlider
                visible: !dialog.correspondence
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
                visible: !dialog.correspondence
                width: parent.width
                minimumValue: 0
                maximumValue: dialog.incrementOptions.length - 1
                stepSize: 1
                value: 0
                label: qsTr("Increment in seconds")
                valueText: dialog.increment
            }

            ComboBox {
                id: daysBox
                visible: dialog.correspondence
                label: qsTr("Days per move")
                currentIndex: 2
                menu: ContextMenu {
                    Repeater {
                        model: dialog.dayOptions
                        MenuItem { text: modelData === 1 ? qsTr("1 day") : qsTr("%1 days").arg(modelData) }
                    }
                }
            }

            ComboBox {
                id: colorBox
                label: qsTr("Your color")
                menu: ContextMenu {
                    MenuItem { text: qsTr("Random") }
                    MenuItem { text: qsTr("White") }
                    MenuItem { text: qsTr("Black") }
                }
            }

            TextSwitch {
                id: ratedSwitch
                text: qsTr("Rated")
                description: qsTr("The result affects both players' ratings")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: dialog.speed === "bullet" ? Theme.errorColor : Theme.secondaryHighlightColor
                text: {
                    switch (dialog.speed) {
                    case "bullet": return qsTr("Bullet games can't be played through the Lichess Board API. Add time or increment.")
                    case "blitz": return qsTr("Blitz game")
                    case "rapid": return qsTr("Rapid game")
                    case "classical": return qsTr("Classical game")
                    default: return qsTr("Correspondence game")
                    }
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
