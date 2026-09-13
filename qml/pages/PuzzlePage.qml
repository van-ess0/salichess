// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.salichess 1.0
import "../components"
import "../js/PuzzleThemes.js" as Themes

Page {
    id: page

    property bool daily: false
    property string angle: "mix"
    property bool userFlipped: false
    readonly property bool flipped: (puzzle.playerColor === "black") !== userFlipped
    readonly property bool playing: puzzle.state === PuzzleController.Playing
    readonly property bool finished: puzzle.state === PuzzleController.Finished

    allowedOrientations: Orientation.All

    PuzzleController {
        id: puzzle
        angle: page.angle
    }

    Component.onCompleted: {
        if (daily)
            puzzle.loadDaily()
        else
            puzzle.loadNext()
    }

    onStatusChanged: {
        if (status === PageStatus.Active)
            app.boardVisible = true
        else if (status === PageStatus.Deactivating)
            app.boardVisible = false
    }

    Component.onDestruction: app.boardVisible = false

    function nextPuzzle() {
        page.daily = false
        page.userFlipped = false
        puzzle.loadNext()
    }

    function feedbackText() {
        var side = puzzle.playerColor === "white" ? qsTr("white") : qsTr("black")
        switch (puzzle.state) {
        case PuzzleController.Loading:
        case PuzzleController.Idle:
            return ""
        case PuzzleController.Error:
            return puzzle.errorString
        case PuzzleController.ShowingSolution:
            return qsTr("Solution")
        case PuzzleController.Finished:
            if (puzzle.solved)
                return qsTr("Success! Puzzle solved.")
            return puzzle.feedback === PuzzleController.SolutionShown
                    ? qsTr("Puzzle complete. Better luck with the next one!")
                    : qsTr("Puzzle solved, but not on the first try.")
        }
        switch (puzzle.feedback) {
        case PuzzleController.GoodMove:
            return qsTr("Best move! Keep going…")
        case PuzzleController.WrongMove:
            return qsTr("That's not the move! Try something else.")
        default:
            return qsTr("Your turn: find the best move for %1.").arg(side)
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: layout.height

        PullDownMenu {
            MenuItem {
                visible: puzzle.puzzleId !== ""
                text: qsTr("Open puzzle in browser")
                onClicked: Qt.openUrlExternally(puzzle.puzzleUrl)
            }
            MenuItem {
                visible: puzzle.puzzleId !== ""
                text: qsTr("Copy link to clipboard")
                onClicked: {
                    Clipboard.text = puzzle.puzzleUrl
                    app.notice(qsTr("Link copied"))
                }
            }
            MenuItem {
                visible: page.finished && puzzle.gameId !== ""
                text: qsTr("View the original game")
                onClicked: pageStack.push(Qt.resolvedUrl("AnalysisPage.qml"),
                                          { gameId: puzzle.gameId,
                                            myColor: puzzle.playerColor,
                                            startPly: puzzle.gamePly })
            }
            MenuItem {
                visible: page.playing && !page.daily
                text: qsTr("Skip puzzle")
                onClicked: page.nextPuzzle()
            }
        }

        Item {
            id: layout
            width: page.width
            height: page.isPortrait ? header.height + board.height + infoColumn.height + Theme.paddingLarge
                                    : Math.max(page.height, infoColumn.height + Theme.paddingLarge)

            readonly property real boardSize: page.isPortrait ? page.width : Math.min(page.height, page.width * 0.55)

            PageHeader {
                id: header
                visible: page.isPortrait
                height: visible ? implicitHeight : 0
                title: page.daily || puzzle.isDaily ? qsTr("Daily puzzle") : Themes.name(page.angle)
                description: puzzle.puzzleId !== ""
                             ? qsTr("Puzzle %1 • rating %2").arg(puzzle.puzzleId).arg(puzzle.puzzleRating)
                             : ""
            }

            ChessBoard {
                id: board
                y: header.height
                width: layout.boardSize
                game: puzzle.game
                flipped: page.flipped
                interactive: page.playing
                movableColor: puzzle.playerColor
                hintSquare: puzzle.hintSquare
                hintTarget: puzzle.hintTarget
                onMoveRequested: puzzle.move(uci)
            }

            BusyIndicator {
                anchors.centerIn: board
                size: BusyIndicatorSize.Large
                running: puzzle.state === PuzzleController.Loading
            }

            Column {
                id: infoColumn
                x: page.isPortrait ? 0 : board.width
                y: page.isPortrait ? board.y + board.height + Theme.paddingMedium : Theme.paddingLarge
                width: page.isPortrait ? page.width : page.width - board.width
                spacing: Theme.paddingMedium

                OfflineBanner {
                    description: session.loggedIn
                                 ? qsTr("Your results are sent when the connection is back")
                                 : qsTr("Stored puzzles can still be played")
                }

                Label {
                    visible: !page.isPortrait && puzzle.puzzleId !== ""
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    truncationMode: TruncationMode.Fade
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeLarge
                    text: page.daily || puzzle.isDaily ? qsTr("Daily puzzle") : Themes.name(page.angle)
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    visible: text !== ""
                    font.pixelSize: Theme.fontSizeLarge
                    color: {
                        if (puzzle.state === PuzzleController.Error || puzzle.feedback === PuzzleController.WrongMove)
                            return Theme.errorColor
                        return Theme.highlightColor
                    }
                    text: page.feedbackText()
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    horizontalAlignment: Text.AlignHCenter
                    visible: page.finished && puzzle.resultSubmitted && puzzle.userRating > 0
                    color: Theme.secondaryHighlightColor
                    text: qsTr("Your puzzle rating: %1 (%2)")
                          .arg(puzzle.userRating)
                          .arg((puzzle.ratingDiff >= 0 ? "+" : "") + puzzle.ratingDiff)
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: Theme.paddingLarge
                    visible: page.playing

                    Button {
                        text: puzzle.hintSquare < 0 ? qsTr("Hint") : qsTr("Show move")
                        enabled: puzzle.hintTarget < 0
                        onClicked: puzzle.showHint()
                    }
                    Button {
                        text: qsTr("View solution")
                        onClicked: puzzle.viewSolution()
                    }
                }

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: page.finished || puzzle.state === PuzzleController.Error
                    text: {
                        if (puzzle.state === PuzzleController.Error)
                            return qsTr("Retry")
                        return page.daily ? qsTr("Continue training") : qsTr("Next puzzle")
                    }
                    onClicked: {
                        if (puzzle.state === PuzzleController.Error && page.daily)
                            puzzle.loadDaily()
                        else
                            page.nextPuzzle()
                    }
                }

                HistoryControls {
                    width: parent.width
                    game: puzzle.game
                    onFlipRequested: page.userFlipped = !page.userFlipped
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    wrapMode: Text.Wrap
                    visible: page.finished && puzzle.themes.length > 0
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryColor
                    text: qsTr("Themes: %1").arg(puzzle.themes.map(function(key) { return Themes.name(key) }).join(", "))
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
