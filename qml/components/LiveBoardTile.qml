// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../js/Util.js" as Util

// A tile across a whole row of a TileGrid. Behind its text a blurred board
// plays through a short game, over and over, with the pieces sliding.
BackgroundItem {
    id: tile

    property alias icon: iconImage.source
    property alias title: titleLabel.text
    property alias subtitle: subtitleLabel.text

    // Morphy against the Duke of Brunswick and Count Isouard, 1858. Castling
    // is the king's move; moveUci() brings the rook along.
    readonly property var moves: [
        "e2e4", "e7e5", "g1f3", "d7d6", "d2d4", "c8g4", "d4e5", "g4f3", "d1f3", "d6e5",
        "f1c4", "g8f6", "f3b3", "d8e7", "b1c3", "c7c6", "c1g5", "b7b5", "c3b5", "c6b5",
        "c4b5", "b8d7", "e1c1", "a8d8", "d1d7", "d8d7", "h1d1", "e7e6", "b5d7", "f6d7",
        "b3b8", "d7b8", "d1d8"
    ]
    // The game rests on the final position for a few beats before it restarts.
    readonly property int restBeats: 4
    readonly property var startCells: Util.fenToPieces("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w")

    // Per piece: its code, and the square it stands on (-1 once taken).
    property var codes: []
    property var squares: []
    property int beat: 0

    // The game fills the tile's height; the checkerboard carries on to either side.
    readonly property real squareSize: height / 8
    readonly property real boardX: (width - 8 * squareSize) / 2
    readonly property var colors: Util.boardColors(appSettings.boardTheme, Theme)

    width: parent.width
    height: parent.tileHeight
    clip: true

    function reset() {
        var c = [], s = []
        for (var i = 0; i < 64; ++i) {
            if (startCells[i] !== "") {
                c.push(startCells[i])
                s.push(i)
            }
        }
        codes = c
        squares = s
    }

    function pieceOn(square) {
        return squares.indexOf(square)
    }

    function movePiece(s, from, to) {
        var mover = s.indexOf(from)
        var taken = s.indexOf(to)
        if (taken >= 0)
            s[taken] = -1
        if (mover >= 0)
            s[mover] = to
    }

    function playMove(uci) {
        var from = Util.squareIndex(uci.substring(0, 2))
        var to = Util.squareIndex(uci.substring(2, 4))
        var s = squares.slice()
        var mover = s.indexOf(from)
        movePiece(s, from, to)
        // Queenside castling, the only castling in the game
        if (mover >= 0 && codes[mover].charAt(1) === "K" && from % 8 === 4 && to % 8 === 2)
            movePiece(s, from - 4, from - 1)
        squares = s
    }

    function advance() {
        if (beat < moves.length) {
            playMove(moves[beat])
        } else if (beat === moves.length + restBeats) {
            reset()
            beat = -1
        }
        ++beat
    }

    Component.onCompleted: reset()

    Timer {
        running: tile.visible && Qt.application.active
        interval: 1300
        repeat: true
        onTriggered: tile.advance()
    }

    // The board is drawn at a lower resolution and scaled up smoothly, which
    // softens it a little.
    Item {
        id: board
        anchors.fill: parent

        layer.enabled: true
        layer.smooth: true
        layer.textureSize: Qt.size(width / 2, height / 2)

        Grid {
            // Squares to either side of the game keep the board's colours going
            readonly property int extra: Math.ceil(tile.boardX / tile.squareSize)
            x: tile.boardX - extra * tile.squareSize
            columns: 8 + 2 * extra + 2
            Repeater {
                model: parent.columns * 8
                Rectangle {
                    width: tile.squareSize
                    height: tile.squareSize
                    color: (((index % parent.columns) - parent.extra) % 2 + 2 + Math.floor(index / parent.columns)) % 2 === 1
                           ? tile.colors.dark : tile.colors.light
                }
            }
        }

        Repeater {
            model: tile.codes.length
            Image {
                readonly property int square: tile.squares[index]
                x: tile.boardX + (square < 0 ? 0 : (square % 8)) * tile.squareSize
                y: (square < 0 ? 0 : (7 - Math.floor(square / 8))) * tile.squareSize
                width: tile.squareSize
                height: tile.squareSize
                sourceSize: Qt.size(width, height)
                opacity: square < 0 ? 0 : 1
                source: "image://pieces/" + appSettings.pieceSet + "/" + tile.codes[index]

                Behavior on x { NumberAnimation { duration: 700; easing.type: Easing.InOutQuad } }
                Behavior on y { NumberAnimation { duration: 700; easing.type: Easing.InOutQuad } }
                Behavior on opacity { NumberAnimation { duration: 400 } }
            }
        }
    }

    // Keeps the text readable, and shows the press
    Rectangle {
        anchors.fill: parent
        color: tile.highlighted ? Theme.rgba(Theme.highlightBackgroundColor, 0.5)
                                : Theme.rgba(Theme.colorScheme === Theme.LightOnDark ? "black" : "white", 0.55)
    }

    Row {
        anchors.centerIn: parent
        spacing: Theme.paddingLarge

        HighlightImage {
            id: iconImage
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.iconSizeMedium
            height: width
            sourceSize: Qt.size(width, height)
            highlighted: tile.highlighted
        }
        Column {
            anchors.verticalCenter: parent.verticalCenter
            Label {
                id: titleLabel
                font.pixelSize: Theme.fontSizeMedium
                color: tile.highlighted ? Theme.highlightColor : Theme.primaryColor
            }
            Label {
                id: subtitleLabel
                font.pixelSize: Theme.fontSizeExtraSmall
                color: tile.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
            }
        }
    }
}
