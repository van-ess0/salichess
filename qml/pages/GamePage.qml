// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.salichess 1.0
import "../components"
import "../js/Util.js" as Util

Page {
    id: page

    property string gameId
    property bool userFlipped: false
    readonly property bool flipped: (controller.myColor === "black") !== userFlipped
    readonly property bool playing: controller.myColor !== "" && !controller.gameOver
    readonly property var opponent: controller.myColor === "black" ? controller.white : controller.black
    property int claimCountdown: 0

    allowedOrientations: Orientation.All

    GameController {
        id: controller
        gameId: page.gameId
        onMoveRejected: app.notice(qsTr("Move rejected: %1").arg(error))
        onActionFailed: app.notice(error)
        onStateChanged: {
            if (opponentGone && page.claimCountdown === 0 && claimWinInSeconds > 0)
                page.claimCountdown = claimWinInSeconds
        }
    }

    Timer {
        running: page.claimCountdown > 0 && controller.opponentGone
        interval: 1000
        repeat: true
        onTriggered: page.claimCountdown = Math.max(0, page.claimCountdown - 1)
    }

    onStatusChanged: {
        if (status === PageStatus.Active) {
            app.activeGame = controller
            app.boardVisible = true
        } else if (status === PageStatus.Deactivating) {
            app.boardVisible = false
        }
    }

    Component.onDestruction: {
        if (app.activeGame === controller)
            app.activeGame = null
        app.boardVisible = false
    }

    function whiteOrBlackPlayer(top) {
        // The player shown on top is the one at the far side of the board.
        var whiteOnTop = page.flipped
        return (top === whiteOnTop) ? "white" : "black"
    }

    SilicaFlickable {
        id: flickable
        anchors.fill: parent
        contentHeight: layout.height

        // The bottom item is reached first when pulling, so the irreversible
        // actions are at the top, and each gets a remorse timer.
        PullDownMenu {
            MenuItem {
                visible: page.playing && controller.canAbort
                text: qsTr("Abort game")
                onClicked: Remorse.popupAction(page, qsTr("Aborting game"), function() { controller.abort() })
            }
            MenuItem {
                visible: page.playing && !controller.canAbort
                text: qsTr("Resign")
                onClicked: Remorse.popupAction(page, qsTr("Resigning"), function() { controller.resign() })
            }
            MenuItem {
                visible: page.playing && controller.canTakeback && !controller.iProposeTakeback
                text: qsTr("Propose takeback")
                onClicked: Remorse.popupAction(page, qsTr("Proposing takeback"), function() { controller.proposeTakeback() })
            }
            MenuItem {
                visible: page.playing && !controller.canAbort && !controller.iOfferDraw
                text: qsTr("Offer draw")
                onClicked: controller.offerDraw()
            }
            MenuItem {
                visible: controller.myColor !== ""
                text: controller.chat.count > 0 ? qsTr("Chat (%1)").arg(controller.chat.count) : qsTr("Chat")
                onClicked: pageStack.push(Qt.resolvedUrl("ChatPage.qml"), { controller: controller })
            }
            MenuItem {
                text: qsTr("Open in browser")
                onClicked: Qt.openUrlExternally("https://lichess.org/" + page.gameId)
            }
            MenuItem {
                visible: controller.gameOver && !!page.opponent && !!page.opponent.id
                text: qsTr("Challenge again")
                onClicked: pageStack.push(Qt.resolvedUrl("NewChallengeDialog.qml"),
                                          { username: page.opponent.name })
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
                title: controller.perfName !== ""
                       ? controller.perfName + " • " + (controller.rated ? qsTr("Rated") : qsTr("Casual"))
                       : qsTr("Game")
                description: controller.hasClock || controller.daysPerTurn === 0 ? ""
                           : (controller.daysPerTurn === 1 ? qsTr("1 day per move")
                                                           : qsTr("%1 days per move").arg(controller.daysPerTurn))
            }

            Column {
                id: boardColumn
                y: header.height
                width: layout.boardSize

                PlayerBar {
                    readonly property string color: page.whiteOrBlackPlayer(true)
                    width: parent.width
                    player: color === "white" ? controller.white : controller.black
                    showClock: controller.hasClock || controller.hasTurnTimer
                    turnTimer: controller.hasTurnTimer
                    timeMs: color === "white" ? controller.whiteTime : controller.blackTime
                    running: controller.runningClock === color
                    toMove: !controller.gameOver && controller.game.sideToMove === color
                }

                ChessBoard {
                    id: board
                    width: parent.width
                    game: controller.game
                    flipped: page.flipped
                    interactive: page.playing
                    movableColor: controller.myColor
                    onMoveRequested: controller.move(uci)
                }

                PlayerBar {
                    readonly property string color: page.whiteOrBlackPlayer(false)
                    width: parent.width
                    player: color === "white" ? controller.white : controller.black
                    showClock: controller.hasClock || controller.hasTurnTimer
                    turnTimer: controller.hasTurnTimer
                    timeMs: color === "white" ? controller.whiteTime : controller.blackTime
                    running: controller.runningClock === color
                    toMove: !controller.gameOver && controller.game.sideToMove === color
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
                    game: controller.game
                }

                HistoryControls {
                    width: parent.width
                    game: controller.game
                    onFlipRequested: page.userFlipped = !page.userFlipped
                }

                BusyLabel {
                    running: controller.loading
                    text: qsTr("Loading game…")
                }

                Label {
                    id: statusLabel
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    visible: text !== "" && !controller.loading
                    font.pixelSize: controller.gameOver ? Theme.fontSizeLarge : Theme.fontSizeMedium
                    color: controller.errorString !== "" ? Theme.errorColor : Theme.highlightColor
                    text: {
                        if (controller.errorString !== "")
                            return controller.errorString
                        if (controller.gameOver)
                            return controller.resultText
                        if (controller.myColor === "")
                            return ""
                        if (controller.isMyTurn)
                            return qsTr("Your turn")
                        return qsTr("Waiting for opponent")
                    }
                }

                // Offers from the opponent
                OfferBanner {
                    visible: page.playing && controller.opponentOffersDraw
                    text: qsTr("Your opponent offers a draw")
                    onAccepted: controller.answerDraw(true)
                    onDeclined: controller.answerDraw(false)
                }

                OfferBanner {
                    visible: page.playing && controller.opponentProposesTakeback
                    text: qsTr("Your opponent proposes a takeback")
                    onAccepted: controller.answerTakeback(true)
                    onDeclined: controller.answerTakeback(false)
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    horizontalAlignment: Text.AlignHCenter
                    visible: page.playing && (controller.iOfferDraw || controller.iProposeTakeback)
                    color: Theme.secondaryHighlightColor
                    font.pixelSize: Theme.fontSizeSmall
                    text: controller.iOfferDraw ? qsTr("Draw offer sent") : qsTr("Takeback proposal sent")
                }

                // Opponent left
                Column {
                    width: parent.width
                    spacing: Theme.paddingMedium
                    visible: page.playing && controller.opponentGone

                    Label {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * x
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        color: Theme.highlightColor
                        text: page.claimCountdown > 0
                              ? qsTr("Your opponent left the game. You can claim victory in %1 s.").arg(page.claimCountdown)
                              : qsTr("Your opponent left the game.")
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        enabled: page.claimCountdown === 0
                        text: qsTr("Claim victory")
                        onClicked: controller.claimVictory()
                    }
                }

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: controller.errorString !== "" && !controller.loading && !controller.gameOver
                    text: qsTr("Retry")
                    onClicked: controller.reconnect()
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
