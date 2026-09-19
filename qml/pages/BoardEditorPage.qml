// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.salichess 1.0
import "../components"

// Sets up a position by hand, e.g. one standing on a real board, to study it
// or to play it out on this phone.
//
// Pick a piece under the board and tap squares to put it there; tapping a
// square that already holds it takes it away again. Pieces can be dragged
// to another square, or off the board to remove them.
Page {
    id: page

    // Where to start; the usual starting position if empty.
    property string initialFen
    property bool flipped: false
    // The piece the brush puts down, "wK" … "bP", "erase", or "" for none.
    property string brush: ""

    allowedOrientations: Orientation.All

    BoardEditor {
        id: editor
        Component.onCompleted: {
            if (page.initialFen !== "")
                editor.setFen(page.initialFen)
            // Where the page starts is not a step to undo.
            editor.clearHistory()
        }
    }

    function paint(square) {
        if (page.brush === "")
            return
        if (page.brush === "erase" || editor.pieceAt(square) === page.brush)
            editor.setPiece(square, "")
        else
            editor.setPiece(square, page.brush)
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: page.isPortrait ? header.height + board.height + controls.height + Theme.paddingLarge
                                       : Math.max(board.height, controls.height + header.height)

        PullDownMenu {
            MenuItem {
                text: qsTr("Copy FEN")
                onClicked: Clipboard.text = editor.fen
            }
            MenuItem {
                text: qsTr("Paste FEN")
                enabled: Clipboard.hasText
                onClicked: {
                    if (!editor.setFen(Clipboard.text))
                        pasteError.visible = true
                }
            }
            MenuItem {
                text: qsTr("Empty board")
                onClicked: editor.clear()
            }
            MenuItem {
                text: qsTr("Starting position")
                onClicked: editor.setStartPosition()
            }
            MenuItem {
                enabled: editor.valid
                text: qsTr("Pass and play from here")
                onClicked: pageStack.push(Qt.resolvedUrl("HotseatDialog.qml"),
                                          { startFen: editor.fen, whiteAtBottom: !page.flipped })
            }
            MenuItem {
                enabled: editor.valid
                text: qsTr("Analyse")
                onClicked: pageStack.push(Qt.resolvedUrl("AnalysisPage.qml"),
                                          { startFen: editor.fen,
                                            myColor: page.flipped ? "black" : "white" })
            }
        }

        PageHeader {
            id: header
            width: page.isPortrait ? parent.width : parent.width - board.width
            anchors.right: parent.right
            title: qsTr("Set up position")
        }

        ChessBoard {
            id: board
            y: page.isPortrait ? header.height : 0
            width: page.isPortrait ? page.width : Math.min(page.height, page.width * 0.55)
            game: editor
            flipped: page.flipped

            // Pieces that were only guessed at, e.g. read off a photo.
            Repeater {
                model: editor.uncertainSquares
                Rectangle {
                    x: board.squareX(modelData)
                    y: board.squareY(modelData)
                    z: 13
                    width: board.squareSize
                    height: board.squareSize
                    color: "transparent"
                    border.color: "#f5a623"
                    border.width: Math.max(2, board.squareSize * 0.06)
                }
            }

            MouseArea {
                id: input
                anchors.fill: parent
                z: 14
                property int pressSquare: -1
                property real startX
                property real startY

                function press(x, y) {
                    pressSquare = board.squareAt(x, y)
                    startX = x
                    startY = y
                    // Keep the page and its pulley from taking the drag.
                    preventStealing = editor.pieceAt(pressSquare) !== ""
                }

                function move(x, y) {
                    if (pressSquare < 0 || editor.pieceAt(pressSquare) === "")
                        return
                    if (board.dragSquare < 0
                            && Math.abs(x - startX) + Math.abs(y - startY) > Theme.startDragDistance)
                        board.dragSquare = pressSquare
                    if (board.dragSquare >= 0) {
                        board.dragX = x
                        board.dragY = y
                    }
                }

                function release(x, y) {
                    preventStealing = false
                    if (board.dragSquare >= 0) {
                        var from = board.dragSquare
                        var to = board.squareAt(x, y)
                        if (to < 0) {
                            // Dropped off the board: gone.
                            board.dragSquare = -1
                            editor.setPiece(from, "")
                        } else {
                            // The piece stays under the finger while it
                            // changes squares, and only then glides into
                            // place from there.
                            board.gliding = true
                            board.dragSquare = to
                            editor.movePiece(from, to)
                            board.dragSquare = -1
                        }
                    } else if (pressSquare >= 0) {
                        page.paint(pressSquare)
                    }
                    pressSquare = -1
                }

                onPressed: press(mouse.x, mouse.y)
                onPositionChanged: move(mouse.x, mouse.y)
                onReleased: release(mouse.x, mouse.y)
                onCanceled: {
                    preventStealing = false
                    board.dragSquare = -1
                    pressSquare = -1
                }
            }
        }

        Column {
            id: controls
            anchors {
                left: page.isPortrait ? parent.left : board.right
                right: parent.right
                top: page.isPortrait ? board.bottom : header.bottom
            }

            // Undo, redo, the eraser and turning the board round.
            Row {
                id: tools
                readonly property real buttonWidth: Math.min(Theme.itemSizeExtraLarge, parent.width / 4)
                anchors.horizontalCenter: parent.horizontalCenter
                topPadding: Theme.paddingSmall

                IconButton {
                    width: tools.buttonWidth
                    icon.source: "image://theme/icon-m-rotate-left"
                    enabled: editor.canUndo
                    onClicked: editor.undo()
                }
                IconButton {
                    width: tools.buttonWidth
                    icon.source: "image://theme/icon-m-rotate-right"
                    enabled: editor.canRedo
                    onClicked: editor.redo()
                }
                IconButton {
                    width: tools.buttonWidth
                    icon.source: "image://theme/icon-m-delete"
                    highlighted: down || page.brush === "erase"
                    onClicked: page.brush = page.brush === "erase" ? "" : "erase"
                }
                IconButton {
                    width: tools.buttonWidth
                    icon.source: "image://theme/icon-m-flip"
                    onClicked: page.flipped = !page.flipped
                }
            }

            // The pieces to put down, white above black. They stand on
            // squares like the board's own, so that black pieces show up on a
            // dark ambience too.
            Grid {
                // Not "palette": Silica items have a palette property of
                // their own, which the buttons found instead of this.
                id: pieceGrid
                readonly property real cellSize: Math.min(Theme.itemSizeMedium, parent.width / 6)
                x: (parent.width - width) / 2
                columns: 6

                Repeater {
                    model: ["wK", "wQ", "wR", "wB", "wN", "wP",
                            "bK", "bQ", "bR", "bB", "bN", "bP"]

                    MouseArea {
                        id: cell
                        readonly property bool selected: page.brush === modelData
                        width: pieceGrid.cellSize
                        height: pieceGrid.cellSize
                        onClicked: page.brush = selected ? "" : modelData

                        Rectangle {
                            anchors.fill: parent
                            color: (Math.floor(index / 6) + index) % 2 === 0 ? board.colors.light
                                                                             : board.colors.dark
                        }
                        // Picked, marked the way the board marks a selected piece.
                        Rectangle {
                            anchors.fill: parent
                            visible: cell.selected || cell.pressed
                            color: Qt.rgba(0.08, 0.33, 0.12, 0.5)
                        }
                        Image {
                            anchors.fill: parent
                            anchors.margins: Theme.paddingSmall
                            sourceSize.width: width
                            sourceSize.height: height
                            source: "image://pieces/" + appSettings.pieceSet + "/" + modelData
                        }
                    }
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                height: implicitHeight + Theme.paddingSmall
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: page.brush === ""
                      ? qsTr("Pick a piece to place it, or drag the pieces on the board. Drag one off the board to remove it.")
                      : page.brush === "erase" ? qsTr("Tap squares to empty them.")
                                               : qsTr("Tap squares to put the piece there, or to take it away again.")
            }

            ComboBox {
                id: sideBox
                label: qsTr("To move")
                currentIndex: editor.sideToMove === "black" ? 1 : 0
                menu: ContextMenu {
                    MenuItem { text: qsTr("White") }
                    MenuItem { text: qsTr("Black") }
                }
                onCurrentIndexChanged: editor.sideToMove = currentIndex === 1 ? "black" : "white"

                // Choosing from the menu replaces the binding above, so a
                // pasted FEN is followed from here.
                Connections {
                    target: editor
                    onPositionChanged: sideBox.currentIndex = editor.sideToMove === "black" ? 1 : 0
                }
            }

            SectionHeader { text: qsTr("Castling") }

            // Only offered where the king and rook are still at home.
            Grid {
                width: parent.width
                columns: 2

                TextSwitch {
                    width: parent.width / 2
                    text: qsTr("White O-O")
                    enabled: editor.whiteKingsidePossible
                    checked: editor.whiteKingside && enabled
                    automaticCheck: false
                    onClicked: editor.whiteKingside = !editor.whiteKingside
                }
                TextSwitch {
                    width: parent.width / 2
                    text: qsTr("White O-O-O")
                    enabled: editor.whiteQueensidePossible
                    checked: editor.whiteQueenside && enabled
                    automaticCheck: false
                    onClicked: editor.whiteQueenside = !editor.whiteQueenside
                }
                TextSwitch {
                    width: parent.width / 2
                    text: qsTr("Black O-O")
                    enabled: editor.blackKingsidePossible
                    checked: editor.blackKingside && enabled
                    automaticCheck: false
                    onClicked: editor.blackKingside = !editor.blackKingside
                }
                TextSwitch {
                    width: parent.width / 2
                    text: qsTr("Black O-O-O")
                    enabled: editor.blackQueensidePossible
                    checked: editor.blackQueenside && enabled
                    automaticCheck: false
                    onClicked: editor.blackQueenside = !editor.blackQueenside
                }
            }

            Label {
                id: pasteError
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: false
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.errorColor
                text: qsTr("The clipboard does not hold a FEN")

                Connections {
                    target: editor
                    onPositionChanged: pasteError.visible = false
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: !editor.valid
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.errorColor
                text: editor.problem
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                topPadding: Theme.paddingSmall
                wrapMode: Text.WrapAnywhere
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: editor.fen
            }
        }

        VerticalScrollDecorator {}
    }
}
