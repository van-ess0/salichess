// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../js/Util.js" as Util

// Static board drawn from a FEN, for lists and the cover.
Item {
    id: mini

    property string fen
    property bool flipped: false
    property string lastMove   // UCI, e.g. "e2e4"

    readonly property var cells: Util.fenToPieces(fen)
    readonly property var colors: Util.boardColors(appSettings.boardTheme, Theme)
    readonly property real squareSize: width / 8
    readonly property int lastFrom: Util.squareIndex(lastMove.substring(0, 2))
    readonly property int lastTo: Util.squareIndex(lastMove.substring(2, 4))

    height: width

    Grid {
        columns: 8
        Repeater {
            model: 64
            Rectangle {
                // index runs top-left to bottom-right as seen on screen
                readonly property int row: Math.floor(index / 8)
                readonly property int col: index % 8
                readonly property int square: mini.flipped ? row * 8 + (7 - col) : (7 - row) * 8 + col
                readonly property string piece: mini.cells[square]
                width: mini.squareSize
                height: mini.squareSize
                color: (row + col) % 2 === 1 ? mini.colors.dark : mini.colors.light

                Rectangle {
                    anchors.fill: parent
                    visible: parent.square === mini.lastFrom || parent.square === mini.lastTo
                    color: Qt.rgba(0.61, 0.78, 0, 0.41)
                }

                Image {
                    anchors.fill: parent
                    visible: parent.piece !== ""
                    sourceSize.width: Math.ceil(width)
                    sourceSize.height: Math.ceil(height)
                    source: parent.piece !== "" && width > 0
                            ? "image://pieces/" + appSettings.pieceSet + "/" + parent.piece : ""
                }
            }
        }
    }
}
