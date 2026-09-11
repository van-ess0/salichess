// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components"

// Seeks a game with a random opponent: Lichess' quick pairing time controls,
// a custom clock or a correspondence game. The Board API doesn't allow blitz
// or bullet seeks.
Page {
    id: page

    readonly property var presets: [
        { "minutes": 10, "increment": 0 },
        { "minutes": 10, "increment": 5 },
        { "minutes": 15, "increment": 10 },
        { "minutes": 30, "increment": 0 },
        { "minutes": 30, "increment": 20 }
    ]
    readonly property var ratingDeltas: [0, 100, 200, 300, 500]
    readonly property var colorKeys: ["random", "white", "black"]
    readonly property var dayOptions: [1, 2, 3, 5, 7, 10, 14]

    // The options chosen on this page, plus the time control in |clock|.
    function options(clock) {
        var result = {
            "rated": ratedSwitch.checked,
            "color": colorKeys[colorBox.currentIndex],
            "ratingDelta": ratingDeltas[rangeBox.currentIndex]
        }
        for (var key in clock)
            result[key] = clock[key]
        return result
    }

    function seek(clock) {
        pageStack.push(Qt.resolvedUrl("SeekWaitingPage.qml"), { options: options(clock) })
    }

    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: page.width

            PageHeader {
                title: qsTr("Random opponent")
                description: qsTr("Rapid, classical or correspondence")
            }

            TextSwitch {
                id: ratedSwitch
                checked: true
                text: qsTr("Rated")
                description: qsTr("The result affects your rating")
            }

            ComboBox {
                id: rangeBox
                label: qsTr("Opponent rating")
                description: qsTr("Relative to your rating in the time control")
                menu: ContextMenu {
                    Repeater {
                        model: page.ratingDeltas
                        MenuItem { text: modelData === 0 ? qsTr("Any") : qsTr("±%1").arg(modelData) }
                    }
                }
            }

            ComboBox {
                id: colorBox
                label: qsTr("Your color")
                description: currentIndex > 0 ? qsTr("Finding an opponent may take longer") : ""
                menu: ContextMenu {
                    MenuItem { text: qsTr("Random") }
                    MenuItem { text: qsTr("White") }
                    MenuItem { text: qsTr("Black") }
                }
            }

            SectionHeader {
                text: qsTr("Quick pairing")
            }

            Repeater {
                model: page.presets

                BackgroundItem {
                    id: presetItem
                    readonly property bool rapid: modelData.minutes * 60 + 40 * modelData.increment < 1500
                    height: Theme.itemSizeSmall
                    onClicked: page.seek(modelData)

                    Label {
                        id: clockLabel
                        x: Theme.horizontalPageMargin
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.minutes + "+" + modelData.increment
                        color: presetItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                    }

                    Label {
                        anchors {
                            left: clockLabel.right
                            leftMargin: Theme.paddingLarge
                            right: parent.right
                            rightMargin: Theme.horizontalPageMargin
                            baseline: clockLabel.baseline
                        }
                        truncationMode: TruncationMode.Fade
                        font.pixelSize: Theme.fontSizeSmall
                        color: presetItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                        text: {
                            session.account // re-evaluate when the ratings change
                            var name = presetItem.rapid ? qsTr("Rapid") : qsTr("Classical")
                            var rating = session.rating(presetItem.rapid ? "rapid" : "classical")
                            return rating > 0 ? qsTr("%1 • your rating %2").arg(name).arg(rating) : name
                        }
                    }
                }
            }

            BackgroundItem {
                height: Theme.itemSizeMedium
                onClicked: pageStack.push(Qt.resolvedUrl("CustomSeekDialog.qml"), { options: page.options({}) })
                MenuEntry {
                    icon: "image://theme/icon-m-timer"
                    title: qsTr("Custom time control")
                    subtitle: qsTr("Choose the minutes and the increment")
                }
            }

            SectionHeader {
                text: qsTr("Correspondence")
            }

            ComboBox {
                id: daysBox
                label: qsTr("Days per move")
                currentIndex: 2
                menu: ContextMenu {
                    Repeater {
                        model: page.dayOptions
                        MenuItem { text: modelData === 1 ? qsTr("1 day") : qsTr("%1 days").arg(modelData) }
                    }
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("A correspondence seek stays in the Lichess lobby until someone joins. salichess can't withdraw it; that is only possible on lichess.org.")
            }

            Item {
                width: 1
                height: Theme.paddingLarge
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Post seek")
                onClicked: page.seek({ "correspondence": true, "days": page.dayOptions[daysBox.currentIndex] })
            }
        }

        VerticalScrollDecorator {}
    }
}
