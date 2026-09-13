// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.salichess 1.0
import "../components"
import "../js/Util.js" as Util

// A finished game, move by move, with what Lichess and Stockfish make of it.
//
// The page itself never scrolls: the board, the players and the controls keep
// their places and only the moves scroll, so a long game does not push
// anything off the screen. Everything that does not have to be here — the
// evaluation graph and how well each side played — is a pull-down away.
Page {
    id: page

    property string gameId
    // Which way round the board starts; the colour the user played by
    // default, worked out from the game once it has loaded.
    property string myColor
    // The move to open at, e.g. the position a puzzle was taken from.
    property int startPly: -1
    property bool userFlipped: false
    property bool jumpedToStart: false
    readonly property bool flipped: (page.myColor === "black") !== userFlipped

    // Fixed parts of the layout, so the board can be given what is left.
    readonly property real barHeight: Theme.itemSizeSmall
    readonly property real gaugeHeight: Theme.paddingLarge
    readonly property real statusHeight: Theme.itemSizeExtraSmall * 0.9
    readonly property real minMovesHeight: Theme.itemSizeMedium

    allowedOrientations: Orientation.All

    GameAnalysis {
        id: gameAnalysis
        gameId: page.gameId

        // The analysis board is the one place where a move from an earlier
        // position starts a side line instead of being refused.
        Component.onCompleted: game.allowVariations = true

        onInfoChanged: {
            if (page.startPly > 0 && !page.jumpedToStart && gameAnalysis.game.ply > 0) {
                page.jumpedToStart = true
                gameAnalysis.game.goToPly(Math.min(page.startPly, gameAnalysis.game.ply))
            }
            if (page.myColor === "" && session.loggedIn) {
                var me = session.userId.toLowerCase()
                if (gameAnalysis.white.id && gameAnalysis.white.id.toLowerCase() === me)
                    page.myColor = "white"
                else if (gameAnalysis.black.id && gameAnalysis.black.id.toLowerCase() === me)
                    page.myColor = "black"
            }
        }
    }

    // There is one engine; whichever board is on screen gets it. Done from a
    // function rather than from onStatusChanged alone: a page that is already
    // Active when it is created never sees the status change, and the engine
    // would then sit there with no board and nothing to say about it.
    function claimEngine() {
        if (page.status === PageStatus.Active) {
            app.boardVisible = true
            engine.game = gameAnalysis.game
        } else {
            app.boardVisible = false
            if (engine.game === gameAnalysis.game)
                engine.game = null
        }
    }

    onStatusChanged: claimEngine()
    Component.onCompleted: claimEngine()

    Component.onDestruction: {
        app.boardVisible = false
        if (engine.game === gameAnalysis.game)
            engine.game = null
    }

    // The engine speaks for the position on the board; Lichess only for the
    // game as it was played.
    readonly property bool engineSpeaking: engine.enabled && engine.available && engine.hasEval
    readonly property bool anyEval: engineSpeaking || gameAnalysis.hasEval
    readonly property real shownWinPercent: engineSpeaking ? engine.winPercent
                                                           : gameAnalysis.winPercent
    readonly property string shownEval: {
        if (engineSpeaking)
            return Util.formatEval(engine.evalCp, engine.evalMate)
        if (gameAnalysis.hasEval)
            return Util.formatEval(gameAnalysis.evalCp, gameAnalysis.evalMate)
        return ""
    }

    // What to point at on the board: the moves the engine likes, the best one
    // solid and the rest fainter so a second idea can be seen without taking
    // over the board. With the engine off, Lichess' own choice for this
    // position stands in.
    readonly property var engineArrows: {
        var out = []
        if (engineSpeaking) {
            for (var i = 0; i < engine.lines.length && i < 3; ++i) {
                var uci = engine.lines[i].uci
                if (!uci)
                    continue
                out.push({ "from": Util.squareIndex(uci.substring(0, 2)),
                           "to": Util.squareIndex(uci.substring(2, 4)),
                           "weight": i === 0 ? 1 : 0.35 })
            }
            return out
        }
        var best = gameAnalysis.nextBestMove
        if (best !== "")
            out.push({ "from": Util.squareIndex(best.substring(0, 2)),
                       "to": Util.squareIndex(best.substring(2, 4)),
                       "weight": 1 })
        return out
    }

    function playerOnTop(top) {
        // The player shown on top is the one at the far side of the board.
        var whiteOnTop = page.flipped
        return (top === whiteOnTop) ? "white" : "black"
    }

    function engineMenuText() {
        if (engineWeights.downloading)
            return qsTr("Downloading Stockfish… %1%").arg(Math.round(engineWeights.progress * 100))
        if (engine.enabled)
            return qsTr("Turn Stockfish off")
        if (!engine.available)
            return qsTr("Turn Stockfish on (75 MB download)")
        return qsTr("Turn Stockfish on")
    }

    SilicaFlickable {
        id: flickable
        anchors.fill: parent
        // The page holds still; only the moves scroll.
        contentHeight: height

        PullDownMenu {
            MenuItem {
                visible: gameAnalysis.errorString !== "" && !gameAnalysis.loading
                text: qsTr("Try again")
                onClicked: gameAnalysis.reload()
            }
            MenuItem {
                text: qsTr("Open in browser")
                onClicked: Qt.openUrlExternally("https://lichess.org/" + page.gameId)
            }
            MenuItem {
                visible: !gameAnalysis.hasServerAnalysis && !gameAnalysis.requestingAnalysis
                         && !gameAnalysis.loading && gameAnalysis.game.ply > 0
                         && gameAnalysis.errorString === ""
                text: qsTr("Request a computer analysis")
                onClicked: gameAnalysis.requestAnalysis()
            }
            MenuItem {
                visible: gameAnalysis.game.ply > 0 && gameAnalysis.errorString === ""
                text: qsTr("Game summary")
                onClicked: pageStack.push(Qt.resolvedUrl("AnalysisSummaryPage.qml"),
                                          { analysis: gameAnalysis })
            }
            MenuItem {
                enabled: !engineWeights.downloading
                text: page.engineMenuText()
                onClicked: {
                    if (engine.enabled)
                        engine.enabled = false
                    else
                        engine.enableWithDownload()
                }
            }
        }

        PageHeader {
            id: header
            visible: page.isPortrait
            height: visible ? implicitHeight : 0
            width: parent.width
            title: gameAnalysis.perfName !== ""
                   ? gameAnalysis.perfName + " • " + (gameAnalysis.rated ? qsTr("Rated")
                                                                              : qsTr("Casual"))
                   : qsTr("Analysis")
            description: [gameAnalysis.resultText, gameAnalysis.openingName]
                         .filter(function(s) { return s !== "" }).join(" • ")
        }

        // Board, the players either side of it and the evaluation bar.
        Column {
            id: boardColumn
            anchors.top: header.bottom
            width: page.isPortrait ? page.width : board.width

            PlayerBar {
                readonly property string color: page.playerOnTop(true)
                width: parent.width
                height: page.barHeight
                player: color === "white" ? gameAnalysis.white : gameAnalysis.black
                showClock: (color === "white" ? gameAnalysis.whiteClockMs
                                              : gameAnalysis.blackClockMs) >= 0
                timeMs: Math.max(0, color === "white" ? gameAnalysis.whiteClockMs
                                                      : gameAnalysis.blackClockMs)
                material: color === "white" ? gameAnalysis.game.whiteMaterial
                                            : gameAnalysis.game.blackMaterial
                materialScore: color === "white" ? gameAnalysis.game.materialScore
                                                 : -gameAnalysis.game.materialScore
                toMove: gameAnalysis.game.sideToMove === color
            }

            ChessBoard {
                id: board
                width: {
                    // What is left once everything that has a fixed place is
                    // taken off, so the page never has to scroll.
                    var free = page.height - header.height - 2 * page.barHeight
                             - page.gaugeHeight - controls.height
                    if (page.isPortrait) {
                        free -= page.statusHeight + engineLines.height + banner.height
                              + page.minMovesHeight
                        return Math.max(page.width / 2, Math.min(page.width, free))
                    }
                    return Math.max(page.height / 2, Math.min(page.width * 0.55, free))
                }
                game: gameAnalysis.game
                flipped: page.flipped
                interactive: true
                movableColor: "both"
                arrows: page.engineArrows
                onMoveRequested: gameAnalysis.game.playUci(uci)
            }

            PlayerBar {
                readonly property string color: page.playerOnTop(false)
                width: parent.width
                height: page.barHeight
                player: color === "white" ? gameAnalysis.white : gameAnalysis.black
                showClock: (color === "white" ? gameAnalysis.whiteClockMs
                                              : gameAnalysis.blackClockMs) >= 0
                timeMs: Math.max(0, color === "white" ? gameAnalysis.whiteClockMs
                                                      : gameAnalysis.blackClockMs)
                material: color === "white" ? gameAnalysis.game.whiteMaterial
                                            : gameAnalysis.game.blackMaterial
                materialScore: color === "white" ? gameAnalysis.game.materialScore
                                                 : -gameAnalysis.game.materialScore
                toMove: gameAnalysis.game.sideToMove === color
            }

            EvalGauge {
                width: parent.width
                height: page.gaugeHeight
                visible: page.anyEval
                winPercent: page.shownWinPercent
                text: page.shownEval
            }
        }

        // Everything else: under the board in portrait, beside it otherwise.
        Item {
            id: detail
            anchors {
                left: page.isPortrait ? parent.left : boardColumn.right
                right: parent.right
                top: page.isPortrait ? boardColumn.bottom : parent.top
                bottom: controls.top
            }

            OfflineBanner {
                id: banner
                anchors.top: parent.top
                description: qsTr("Evaluations need a connection")
            }

            // One line for whatever is worth saying about this position.
            Item {
                // Not "status": an id shadows the page's own status property,
                // and everything that asked whether the page was Active then
                // compared against this Item instead.
                id: statusLine
                anchors { left: parent.left; right: parent.right; top: banner.bottom }
                height: page.statusHeight

                BusyIndicator {
                    id: busy
                    anchors {
                        left: parent.left
                        leftMargin: Theme.horizontalPageMargin
                        verticalCenter: parent.verticalCenter
                    }
                    running: gameAnalysis.loading || gameAnalysis.requestingAnalysis
                             || engineWeights.downloading
                    size: BusyIndicatorSize.ExtraSmall
                }

                Label {
                    anchors {
                        left: busy.running ? busy.right : parent.left
                        leftMargin: busy.running ? Theme.paddingMedium : Theme.horizontalPageMargin
                        right: parent.right
                        rightMargin: Theme.horizontalPageMargin
                        verticalCenter: parent.verticalCenter
                    }
                    truncationMode: TruncationMode.Fade
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: {
                        if (gameAnalysis.errorString !== "" || gameAnalysis.requestError !== "")
                            return Theme.errorColor
                        if (gameAnalysis.judgment !== "")
                            return Util.judgmentColor(gameAnalysis.judgment)
                        return Theme.secondaryColor
                    }
                    text: {
                        if (gameAnalysis.errorString !== "")
                            return gameAnalysis.errorString
                        if (gameAnalysis.loading)
                            return qsTr("Loading game…")
                        if (engineWeights.downloading)
                            return qsTr("Downloading Stockfish… %1%")
                                   .arg(Math.round(engineWeights.progress * 100))
                        if (gameAnalysis.requestError !== "")
                            return gameAnalysis.requestError
                        if (gameAnalysis.requestingAnalysis)
                            return qsTr("Lichess is analysing the game…")
                        // Whatever the engine is doing comes first while it is on: it
                        // is the thing drawing on the board, and "it is not running"
                        // has to be visible rather than guessed at.
                        if (engine.enabled && engineWeights.errorString !== "")
                            return engineWeights.errorString
                        if (engine.enabled && engine.errorString !== "")
                            return engine.errorString
                        if (engine.enabled && !engine.available)
                            return qsTr("Stockfish is waiting for its networks")
                        if (page.engineSpeaking)
                            return engine.searching
                                   ? qsTr("Stockfish, depth %1").arg(engine.depth)
                                   : qsTr("Stockfish, depth %1 — done").arg(engine.depth)
                        if (engine.enabled)
                            return qsTr("Stockfish is starting…")
                        if (gameAnalysis.judgmentComment !== "")
                            return gameAnalysis.judgmentComment
                        if (gameAnalysis.nextBestVariation !== "")
                            return qsTr("Best: %1").arg(gameAnalysis.nextBestVariation)
                        if (gameAnalysis.evalSource === "cloud")
                            return qsTr("Lichess cloud evaluation")
                        if (!gameAnalysis.hasServerAnalysis && gameAnalysis.game.ply > 0)
                            return qsTr("Not analysed by Lichess")
                        // The header carries the result, but only in portrait.
                        return page.isPortrait ? "" : gameAnalysis.resultText
                    }
                }
            }

            EngineLines {
                id: engineLines
                anchors { left: parent.left; right: parent.right; top: statusLine.bottom }
                visible: engine.enabled && engine.lines.length > 0
                game: gameAnalysis.game
            }

            // The moves, and the only thing on the page that scrolls.
            MoveTree {
                id: moves
                anchors {
                    left: parent.left
                    right: parent.right
                    top: engineLines.bottom
                    bottom: variationRow.top
                }
                game: gameAnalysis.game
                annotations: gameAnalysis.judgments
            }

            // Shown only while a side line is being looked at.
            Row {
                id: variationRow
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: gameAnalysis.game.inVariation ? Theme.itemSizeExtraSmall : 0
                visible: gameAnalysis.game.inVariation

                Repeater {
                    model: [
                        { "text": qsTr("Back"), "action": "exit" },
                        { "text": qsTr("Promote"), "action": "promote" },
                        { "text": qsTr("Delete"), "action": "delete" }
                    ]

                    BackgroundItem {
                        width: variationRow.width / 3
                        height: variationRow.height
                        onClicked: {
                            if (modelData.action === "exit")
                                gameAnalysis.game.exitVariation()
                            else if (modelData.action === "promote")
                                gameAnalysis.game.promoteVariation()
                            else
                                gameAnalysis.game.deleteVariation()
                        }
                        Label {
                            anchors.centerIn: parent
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: parent.highlighted ? Theme.highlightColor : Theme.primaryColor
                            text: modelData.text
                        }
                    }
                }
            }
        }

        HistoryControls {
            id: controls
            anchors {
                left: page.isPortrait ? parent.left : boardColumn.right
                right: parent.right
                bottom: parent.bottom
            }
            game: gameAnalysis.game
            onFlipRequested: page.userFlipped = !page.userFlipped
        }
    }
}
