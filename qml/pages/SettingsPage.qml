// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components"

Page {
    id: page

    function sizeText(bytes) {
        if (bytes >= 1024 * 1024)
            return qsTr("%1 MB").arg(Math.round(bytes / (1024 * 1024)))
        return qsTr("%1 kB").arg(Math.round(bytes / 1024))
    }

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
                // sliderValueChanged, not valueChanged: it only fires when the
                // user moves the slider, so writing the setting back does not
                // loop through the value binding.
                onSliderValueChanged: appSettings.offlinePuzzles = Math.round(value)
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
                text: qsTr("Engine")
            }

            TextSwitch {
                text: qsTr("Analyse with Stockfish")
                description: {
                    if (engineWeights.downloading)
                        return qsTr("Downloading… %1%").arg(Math.round(engineWeights.progress * 100))
                    if (engineWeights.errorString !== "")
                        return engineWeights.errorString
                    if (engineWeights.ready)
                        return qsTr("On the analysis board, using %1").arg(sizeText(engineWeights.storedBytes))
                    return qsTr("Needs a one-off download of about 75 MB")
                }
                automaticCheck: false
                checked: appSettings.engineEnabled
                onClicked: {
                    if (appSettings.engineEnabled)
                        appSettings.engineEnabled = false
                    else
                        engine.enableWithDownload()
                }
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: engineWeights.downloading
                text: qsTr("Stop the download")
                onClicked: engineWeights.cancel()
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: engineWeights.ready && !engineWeights.downloading
                text: qsTr("Delete the engine data")
                onClicked: Remorse.popupAction(page, qsTr("Deleting"), function() {
                    appSettings.engineEnabled = false
                    engineWeights.remove()
                })
            }

            Slider {
                width: parent.width
                visible: appSettings.engineEnabled
                minimumValue: 1
                maximumValue: 5
                stepSize: 1
                value: appSettings.engineLines
                label: qsTr("Variations shown")
                valueText: value
                onSliderValueChanged: appSettings.engineLines = value
            }

            Slider {
                width: parent.width
                visible: appSettings.engineEnabled
                minimumValue: 6
                maximumValue: 30
                stepSize: 1
                value: appSettings.engineDepth
                label: qsTr("Search depth")
                valueText: value
                onSliderValueChanged: appSettings.engineDepth = value
            }

            // Slider has no description property: an unknown property makes
            // QML reject the whole page, so the hint is a label of its own.
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: appSettings.engineEnabled
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: qsTr("Deeper is stronger, and costs more battery")
            }

            Slider {
                width: parent.width
                visible: appSettings.engineEnabled
                minimumValue: 1
                maximumValue: 4
                stepSize: 1
                value: appSettings.engineThreads
                label: qsTr("Processor cores")
                valueText: value
                onSliderValueChanged: appSettings.engineThreads = value
            }

            Slider {
                width: parent.width
                visible: appSettings.engineEnabled
                minimumValue: 8
                maximumValue: 128
                stepSize: 8
                value: appSettings.engineHash
                label: qsTr("Memory for the engine")
                valueText: value + " MB"
                onSliderValueChanged: appSettings.engineHash = value
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
