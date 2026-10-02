// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.salichess 1.0
import "../components"
import "../js/Computer.js" as Computer

// Sets up a game against the computer: the Lichess AI, played on lichess.org,
// or Stockfish on the phone, which needs no connection and no account once
// its networks have been downloaded.
Page {
    id: page

    // Without an account there is no Lichess AI to play.
    readonly property bool useEngine: !session.loggedIn || appSettings.computerOffline
    readonly property bool engineReady: engineWeights.ready
    readonly property var colorKeys: ["random", "white", "black"]
    readonly property bool canPlay: useEngine ? engineReady : !aiChallenge.busy

    allowedOrientations: Orientation.All

    AiChallenge {
        id: aiChallenge
        onStarted: pageStack.replace(Qt.resolvedUrl("GamePage.qml"), { gameId: gameId })
        onFailed: app.notice(error)
    }

    function play() {
        appSettings.computerLevel = levelBox.currentIndex + 1
        appSettings.computerColor = colorKeys[colorBox.currentIndex]
        if (useEngine) {
            pageStack.replace(Qt.resolvedUrl("ComputerGamePage.qml"),
                              { level: appSettings.computerLevel, color: appSettings.computerColor })
        } else {
            appSettings.computerClock = clockBox.currentIndex
            var clock = Computer.clocks[clockBox.currentIndex]
            aiChallenge.start(appSettings.computerLevel, appSettings.computerColor,
                              clock.seconds, clock.increment)
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: page.width

            PageHeader {
                title: qsTr("Play the computer")
                description: page.useEngine ? qsTr("Stockfish on this phone") : qsTr("The Lichess AI")
            }

            TextSwitch {
                enabled: session.loggedIn
                automaticCheck: false
                checked: page.useEngine
                text: qsTr("Play offline")
                description: {
                    if (!session.loggedIn)
                        return qsTr("Log in to play the Lichess AI instead")
                    if (page.useEngine)
                        return qsTr("Stockfish runs on this phone, without a connection")
                    return connection.offline ? qsTr("You seem to be offline. Switch this on to play on the phone.")
                                              : qsTr("Off: play the Lichess AI on lichess.org")
                }
                onClicked: appSettings.computerOffline = !appSettings.computerOffline
            }

            // The engine's networks have to be on the phone before Stockfish
            // may move. Playing is held back until they are.
            Column {
                width: parent.width
                visible: page.useEngine && !page.engineReady
                spacing: Theme.paddingMedium

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    wrapMode: Text.Wrap
                    color: engineWeights.errorString !== "" ? Theme.errorColor : Theme.highlightColor
                    font.pixelSize: Theme.fontSizeSmall
                    text: {
                        if (engineWeights.errorString !== "")
                            return engineWeights.errorString
                        if (engineWeights.downloading)
                            return qsTr("Downloading the engine…")
                        return qsTr("Stockfish needs its neural networks, about %1 MB, downloaded once. They stay on the phone.")
                               .arg(Math.round((engineWeights.totalBytes > 0 ? engineWeights.totalBytes : 78 * 1024 * 1024) / 1048576))
                    }
                }

                ProgressBar {
                    width: parent.width
                    visible: engineWeights.downloading
                    minimumValue: 0
                    maximumValue: 1
                    value: engineWeights.progress
                }

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: engineWeights.downloading ? qsTr("Cancel download") : qsTr("Download the engine")
                    enabled: !connection.offline || engineWeights.downloading
                    onClicked: engineWeights.downloading ? engineWeights.cancel() : engineWeights.download()
                }

                Label {
                    visible: connection.offline && !engineWeights.downloading
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    text: qsTr("The download needs a connection.")
                }
            }

            ComboBox {
                id: levelBox
                label: qsTr("Strength")
                currentIndex: appSettings.computerLevel - 1
                description: qsTr("About %1 on lichess.org").arg(Computer.levelRating(currentIndex + 1))
                menu: ContextMenu {
                    Repeater {
                        model: 8
                        MenuItem {
                            text: qsTr("Level %1").arg(index + 1) + " • " + Computer.levelRating(index + 1)
                        }
                    }
                }
            }

            ComboBox {
                id: colorBox
                label: qsTr("Your color")
                currentIndex: Math.max(0, page.colorKeys.indexOf(appSettings.computerColor))
                menu: ContextMenu {
                    MenuItem { text: qsTr("Random") }
                    MenuItem { text: qsTr("White") }
                    MenuItem { text: qsTr("Black") }
                }
            }

            ComboBox {
                id: clockBox
                visible: !page.useEngine
                label: qsTr("Time control")
                currentIndex: Math.min(appSettings.computerClock, Computer.clocks.length - 1)
                menu: ContextMenu {
                    Repeater {
                        model: Computer.clocks
                        MenuItem { text: Computer.clockName(modelData) }
                    }
                }
            }

            Label {
                visible: page.useEngine
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("Games on the phone have no clock and are not rated.")
            }

            Item {
                width: 1
                height: Theme.paddingLarge
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                enabled: page.canPlay
                text: aiChallenge.busy ? qsTr("Starting…") : qsTr("Play")
                onClicked: page.play()
            }
        }

        VerticalScrollDecorator {}
    }
}
