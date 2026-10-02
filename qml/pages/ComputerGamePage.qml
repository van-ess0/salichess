// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.salichess 1.0
import "../components"
import "../js/Computer.js" as Computer

// A game against Stockfish on the phone. Without its networks the computer
// cannot move: the page says so and offers to download them, and the game
// carries on by itself once they are there.
Page {
    id: page

    property int level: 3
    // "white", "black" or "random"
    property string color: "random"
    property bool started: false
    property bool userFlipped: false
    readonly property bool flipped: (computer.playerColor === "black") !== userFlipped
    readonly property bool playing: computer.state === ComputerGame.Playing
    readonly property var you: ({ "name": qsTr("You") })
    readonly property var stockfish: ({ "name": qsTr("Stockfish"), "rating": Computer.levelRating(level) })
    // Moves needed before a take-back has something to take.
    readonly property bool canTakeBack: playing && computer.game.ply >= (computer.isMyTurn ? 2 : 1)
                                         + (computer.playerColor === "white" ? 0 : 1)

    allowedOrientations: Orientation.All

    ComputerGame {
        id: computer
        level: page.level
    }

    onStatusChanged: {
        if (status === PageStatus.Active) {
            app.boardVisible = true
            // Not in Component.onCompleted: the page may be created long
            // before it is shown.
            if (!page.started) {
                page.started = true
                computer.start(page.color)
            }
        } else if (status === PageStatus.Deactivating) {
            app.boardVisible = false
        }
    }

    Component.onDestruction: app.boardVisible = false

    function newGame() {
        page.userFlipped = false
        computer.start(appSettings.computerColor)
    }

    function statusText() {
        if (computer.errorString !== "")
            return computer.errorString
        if (computer.gameOver)
            return computer.resultText
        if (computer.waitingForEngine)
            return engineWeights.ready ? qsTr("Stockfish is waking up…")
                                       : qsTr("Stockfish needs its networks before it can move.")
        if (computer.thinking)
            return qsTr("Stockfish is thinking…")
        if (computer.isMyTurn)
            return qsTr("Your turn")
        return ""
    }

    SilicaFlickable {
        id: flickable
        anchors.fill: parent
        contentHeight: layout.height

        PullDownMenu {
            MenuItem {
                visible: page.playing
                text: qsTr("Resign")
                onClicked: Remorse.popupAction(page, qsTr("Resigning"), function() { computer.resign() })
            }
            MenuItem {
                visible: page.canTakeBack
                text: qsTr("Take back move")
                onClicked: computer.takeBack()
            }
            MenuItem {
                visible: computer.gameOver && computer.game.ply > 0
                text: qsTr("Analysis")
                onClicked: pageStack.push(Qt.resolvedUrl("AnalysisPage.qml"),
                                          { startMoves: computer.game.sanMoves.join(" "),
                                            myColor: computer.playerColor })
            }
            MenuItem {
                text: qsTr("New game")
                onClicked: {
                    if (page.playing && computer.game.ply > 1)
                        Remorse.popupAction(page, qsTr("Starting a new game"), function() { page.newGame() })
                    else
                        page.newGame()
                }
            }
        }

        Item {
            id: layout
            width: page.width
            height: page.isPortrait ? header.height + boardColumn.height + infoColumn.height + Theme.paddingLarge
                                    : Math.max(page.height, infoColumn.height)

            readonly property real boardSize: page.isPortrait
                                              ? page.width
                                              : Math.min(page.height - 2 * Theme.itemSizeSmall, page.width * 0.55)

            PageHeader {
                id: header
                visible: page.isPortrait
                height: visible ? implicitHeight : 0
                title: qsTr("Stockfish")
                description: qsTr("Level %1").arg(page.level)
            }

            Column {
                id: boardColumn
                y: header.height
                width: layout.boardSize

                PlayerBar {
                    readonly property bool whiteTop: page.flipped
                    width: parent.width
                    player: whiteTop === (computer.playerColor === "white") ? page.you : page.stockfish
                    showClock: false
                    material: whiteTop ? computer.game.whiteMaterial : computer.game.blackMaterial
                    materialScore: whiteTop ? computer.game.materialScore : -computer.game.materialScore
                    toMove: !computer.gameOver && computer.game.sideToMove === (whiteTop ? "white" : "black")
                }

                ChessBoard {
                    id: board
                    width: parent.width
                    game: computer.game
                    flipped: page.flipped
                    interactive: computer.isMyTurn && !computer.thinking
                    movableColor: computer.playerColor
                    onMoveRequested: computer.move(uci)
                }

                PlayerBar {
                    readonly property bool whiteTop: page.flipped
                    width: parent.width
                    player: whiteTop === (computer.playerColor === "white") ? page.stockfish : page.you
                    showClock: false
                    material: whiteTop ? computer.game.blackMaterial : computer.game.whiteMaterial
                    materialScore: whiteTop ? -computer.game.materialScore : computer.game.materialScore
                    toMove: !computer.gameOver && computer.game.sideToMove === (whiteTop ? "black" : "white")
                }
            }

            Column {
                id: infoColumn
                x: page.isPortrait ? 0 : boardColumn.width
                y: page.isPortrait ? boardColumn.y + boardColumn.height + Theme.paddingSmall : Theme.paddingLarge
                width: page.isPortrait ? page.width : page.width - boardColumn.width
                spacing: Theme.paddingMedium

                MoveList {
                    width: parent.width
                    game: computer.game
                }

                HistoryControls {
                    width: parent.width
                    game: computer.game
                    onFlipRequested: page.userFlipped = !page.userFlipped
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    visible: text !== ""
                    font.pixelSize: computer.gameOver ? Theme.fontSizeLarge : Theme.fontSizeMedium
                    color: computer.errorString !== "" ? Theme.errorColor : Theme.highlightColor
                    text: page.statusText()
                }

                // Waiting for the networks.
                Column {
                    width: parent.width
                    spacing: Theme.paddingMedium
                    visible: computer.waitingForEngine && !engineWeights.ready && computer.errorString === ""

                    ProgressBar {
                        width: parent.width
                        visible: engineWeights.downloading
                        minimumValue: 0
                        maximumValue: 1
                        value: engineWeights.progress
                    }

                    Label {
                        visible: engineWeights.errorString !== ""
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * x
                        wrapMode: Text.Wrap
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.errorColor
                        text: engineWeights.errorString
                    }

                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: engineWeights.downloading ? qsTr("Cancel download") : qsTr("Download the engine")
                        onClicked: engineWeights.downloading ? engineWeights.cancel() : computer.downloadEngine()
                    }
                }

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: computer.errorString !== "" && page.playing
                    text: qsTr("Try again")
                    onClicked: computer.retry()
                }

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: computer.gameOver
                    text: qsTr("New game")
                    onClicked: page.newGame()
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
