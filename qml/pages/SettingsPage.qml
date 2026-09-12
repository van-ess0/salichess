// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components"

Page {
    id: page

    readonly property var themeKeys: ["brown", "blue", "green", "ambience"]

    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: page.width

            PageHeader {
                title: qsTr("Settings")
            }

            SectionHeader {
                text: qsTr("Board")
            }

            MiniBoard {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(page.width, page.height) / 2
                fen: "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3"
                lastMove: "b8c6"
            }

            ComboBox {
                label: qsTr("Board colors")
                currentIndex: Math.max(0, page.themeKeys.indexOf(appSettings.boardTheme))
                menu: ContextMenu {
                    MenuItem { text: qsTr("Brown"); onClicked: appSettings.boardTheme = "brown" }
                    MenuItem { text: qsTr("Blue"); onClicked: appSettings.boardTheme = "blue" }
                    MenuItem { text: qsTr("Green"); onClicked: appSettings.boardTheme = "green" }
                    MenuItem { text: qsTr("Ambience"); onClicked: appSettings.boardTheme = "ambience" }
                }
            }

            TextSwitch {
                text: qsTr("Show coordinates")
                automaticCheck: false
                checked: appSettings.showCoordinates
                onClicked: appSettings.showCoordinates = !appSettings.showCoordinates
            }

            TextSwitch {
                text: qsTr("Show legal moves")
                description: qsTr("Mark the squares a selected piece can move to")
                automaticCheck: false
                checked: appSettings.showLegalMoves
                onClicked: appSettings.showLegalMoves = !appSettings.showLegalMoves
            }

            TextSwitch {
                text: qsTr("Animate moves")
                automaticCheck: false
                checked: appSettings.animatePieces
                onClicked: appSettings.animatePieces = !appSettings.animatePieces
            }

            SectionHeader {
                text: qsTr("Puzzles")
            }

            Slider {
                width: parent.width
                minimumValue: 0
                maximumValue: 100
                stepSize: 10
                value: appSettings.offlinePuzzles
                label: qsTr("Puzzles kept for offline play")
                valueText: value > 0 ? qsTr("%n puzzle(s)", "", Math.round(value)) : qsTr("Off")
                onValueChanged: appSettings.offlinePuzzles = Math.round(value)
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: {
                    if (appSettings.offlinePuzzles === 0)
                        return qsTr("The healthy mix is downloaded ahead of time so it can be played without a connection.")
                    var lines = [qsTr("%n of %1 downloaded", "", puzzleStore.count).arg(puzzleStore.target)]
                    if (puzzleStore.filling)
                        lines.push(qsTr("Downloading…"))
                    else if (connection.offline && puzzleStore.count < puzzleStore.target)
                        lines.push(qsTr("Waiting for a connection"))
                    if (puzzleStore.pendingResults > 0)
                        lines.push(qsTr("%n result(s) to send", "", puzzleStore.pendingResults))
                    return lines.join(" • ")
                }
            }

            SectionHeader {
                text: qsTr("General")
            }

            TextSwitch {
                text: qsTr("Keep the screen on")
                description: qsTr("While a game or puzzle is on screen")
                automaticCheck: false
                checked: appSettings.keepScreenOn
                onClicked: appSettings.keepScreenOn = !appSettings.keepScreenOn
            }

            TextSwitch {
                text: qsTr("Notifications")
                description: qsTr("Incoming challenges and your turn in correspondence games, while salichess is running")
                automaticCheck: false
                checked: appSettings.notifications
                onClicked: appSettings.notifications = !appSettings.notifications
            }
        }

        VerticalScrollDecorator {}
    }
}
