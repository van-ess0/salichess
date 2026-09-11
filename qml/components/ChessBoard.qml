// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../js/Util.js" as Util

// Displays a ChessGame and lets the user move pieces of |movableColor| by
// tapping or dragging. Moves are only reported through moveRequested();
// the owner decides whether to play them.
Item {
    id: board

    property QtObject game
    property bool flipped: false
    property bool interactive: false
    property string movableColor: ""   // "white", "black" or "" for none
    property int hintSquare: -1
    property int hintTarget: -1

    signal moveRequested(string uci)

    readonly property real squareSize: width / 8
    readonly property var colors: Util.boardColors(appSettings.boardTheme, Theme)

    property int selectedSquare: -1
    property var targets: []
    property int pressSquare: -1
    property int dragSquare: -1
    property real dragX: 0
    property real dragY: 0
    property bool settling: false
    property int promotionFrom: -1
    property int promotionTo: -1

    height: width

    function squareX(sq) {
        var file = sq % 8
        return (flipped ? 7 - file : file) * squareSize
    }

    function squareY(sq) {
        var rank = Math.floor(sq / 8)
        return (flipped ? rank : 7 - rank) * squareSize
    }

    function squareAt(x, y) {
        if (x < 0 || y < 0 || x >= width || y >= height)
            return -1
        var file = Math.floor(x / squareSize)
        var rank = 7 - Math.floor(y / squareSize)
        if (flipped) {
            file = 7 - file
            rank = 7 - rank
        }
        return rank * 8 + file
    }

    function canMove() {
        return interactive && game && movableColor !== ""
                && game.atLatest && game.sideToMove === movableColor
    }

    function ownPiece(sq) {
        var piece = game.pieceAt(sq)
        return piece !== "" && piece.charAt(0) === movableColor.charAt(0)
    }

    function select(sq) {
        selectedSquare = sq
        targets = game.legalTargets(sq)
    }

    function clearSelection() {
        selectedSquare = -1
        targets = []
    }

    function tryMove(from, to) {
        clearSelection()
        if (game.isPromotion(from, to)) {
            promotionFrom = from
            promotionTo = to
            return
        }
        var uci = game.uciForMove(from, to, "")
        if (uci !== "")
            moveRequested(uci)
    }

    function promote(piece) {
        var uci = game.uciForMove(promotionFrom, promotionTo, piece)
        promotionFrom = -1
        promotionTo = -1
        if (uci !== "")
            moveRequested(uci)
    }

    Connections {
        target: board.game
        onPositionChanged: {
            board.clearSelection()
            board.promotionFrom = -1
        }
    }

    onMovableColorChanged: clearSelection()
    onInteractiveChanged: clearSelection()

    Timer {
        id: settleTimer
        interval: 60
        onTriggered: board.settling = false
    }

    // Squares with coordinates
    Repeater {
        model: 64
        Rectangle {
            readonly property int file: index % 8
            readonly property int rank: Math.floor(index / 8)
            readonly property bool dark: (file + rank) % 2 === 0
            x: board.squareX(index)
            y: board.squareY(index)
            width: board.squareSize
            height: board.squareSize
            color: dark ? board.colors.dark : board.colors.light

            Text {
                // rank number on the left edge
                visible: appSettings.showCoordinates && file === (board.flipped ? 7 : 0)
                text: rank + 1
                color: parent.dark ? board.colors.light : board.colors.dark
                font.pixelSize: board.squareSize * 0.22
                font.bold: true
                anchors { left: parent.left; top: parent.top; margins: board.squareSize * 0.04 }
            }
            Text {
                // file letter on the bottom edge
                visible: appSettings.showCoordinates && rank === (board.flipped ? 7 : 0)
                text: String.fromCharCode(97 + file)
                color: parent.dark ? board.colors.light : board.colors.dark
                font.pixelSize: board.squareSize * 0.22
                font.bold: true
                anchors { right: parent.right; bottom: parent.bottom; margins: board.squareSize * 0.04 }
            }
        }
    }

    // Last move
    Repeater {
        model: board.game ? [board.game.lastMoveFrom, board.game.lastMoveTo] : []
        Rectangle {
            visible: modelData >= 0
            x: board.squareX(modelData)
            y: board.squareY(modelData)
            width: board.squareSize
            height: board.squareSize
            color: Qt.rgba(0.61, 0.78, 0, 0.41)
        }
    }

    // Selected piece and hint
    Rectangle {
        visible: board.selectedSquare >= 0
        x: board.squareX(board.selectedSquare)
        y: board.squareY(board.selectedSquare)
        width: board.squareSize
        height: board.squareSize
        color: Qt.rgba(0.08, 0.33, 0.12, 0.5)
    }

    // Hint: a fixed blue that stands out on every board theme, pulsing so it
    // can't be missed. The piece square is filled, the target gets a ring.
    Repeater {
        model: [board.hintSquare, board.hintTarget]
        Rectangle {
            visible: modelData >= 0
            x: board.squareX(modelData)
            y: board.squareY(modelData)
            width: board.squareSize
            height: board.squareSize
            radius: index === 1 ? width / 2 : 0
            color: index === 0 ? "#3692e7" : "transparent"
            border.color: "#3692e7"
            border.width: index === 1 ? board.squareSize * 0.1 : 0

            SequentialAnimation on opacity {
                running: visible
                loops: Animation.Infinite
                NumberAnimation { from: 0.85; to: 0.35; duration: 700; easing.type: Easing.InOutQuad }
                NumberAnimation { from: 0.35; to: 0.85; duration: 700; easing.type: Easing.InOutQuad }
            }
        }
    }

    // King in check
    Rectangle {
        visible: board.game !== null && board.game.checkSquare >= 0
        x: board.game ? board.squareX(board.game.checkSquare) : 0
        y: board.game ? board.squareY(board.game.checkSquare) : 0
        width: board.squareSize
        height: board.squareSize
        radius: width / 2
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(1, 0, 0, 0.9) }
            GradientStop { position: 0.6; color: Qt.rgba(0.9, 0, 0, 0.4) }
            GradientStop { position: 1.0; color: Qt.rgba(0.66, 0, 0, 0) }
        }
    }

    // Pieces
    Repeater {
        model: board.game ? board.game.pieces : null
        Image {
            readonly property bool dragged: model.square === board.dragSquare
            x: dragged ? board.dragX - width / 2 : board.squareX(model.square)
            y: dragged ? board.dragY - height * 0.9 : board.squareY(model.square)
            z: dragged ? 10 : 1
            width: board.squareSize
            height: board.squareSize
            scale: dragged ? 1.4 : 1
            sourceSize.width: Math.ceil(board.squareSize)
            sourceSize.height: Math.ceil(board.squareSize)
            source: board.squareSize > 0 ? "image://pieces/" + appSettings.pieceSet + "/" + model.piece : ""
            smooth: true

            Behavior on x {
                enabled: appSettings.animatePieces && board.dragSquare < 0 && !board.settling
                NumberAnimation { duration: 180; easing.type: Easing.OutQuad }
            }
            Behavior on y {
                enabled: appSettings.animatePieces && board.dragSquare < 0 && !board.settling
                NumberAnimation { duration: 180; easing.type: Easing.OutQuad }
            }
        }
    }

    // Legal move targets
    Repeater {
        model: appSettings.showLegalMoves ? board.targets : []
        Item {
            readonly property bool capture: board.game.pieceAt(modelData) !== ""
            x: board.squareX(modelData)
            y: board.squareY(modelData)
            z: 11
            width: board.squareSize
            height: board.squareSize

            Rectangle {
                visible: !parent.capture
                anchors.centerIn: parent
                width: parent.width * 0.3
                height: width
                radius: width / 2
                color: Qt.rgba(0.08, 0.33, 0.12, 0.5)
            }
            Rectangle {
                visible: parent.capture
                anchors.fill: parent
                radius: width / 2
                color: "transparent"
                border.color: Qt.rgba(0.08, 0.33, 0.12, 0.5)
                border.width: parent.width * 0.08
            }
        }
    }

    MouseArea {
        id: input
        anchors.fill: parent
        enabled: board.interactive && board.game !== null && board.promotionFrom < 0
        property real startX
        property real startY

        onPressed: {
            var sq = board.squareAt(mouse.x, mouse.y)
            if (!board.game.atLatest) {
                board.game.viewLatest()
                return
            }
            if (!board.canMove() || sq < 0)
                return
            if (board.selectedSquare >= 0 && board.targets.indexOf(sq) >= 0) {
                board.tryMove(board.selectedSquare, sq)
                return
            }
            if (board.ownPiece(sq)) {
                board.select(sq)
                board.pressSquare = sq
                startX = mouse.x
                startY = mouse.y
                // Keep the page and pulley menus from stealing the drag.
                preventStealing = true
            } else {
                board.clearSelection()
            }
        }

        onPositionChanged: {
            if (board.pressSquare < 0)
                return
            if (board.dragSquare < 0 && Math.abs(mouse.x - startX) + Math.abs(mouse.y - startY) > Theme.startDragDistance)
                board.dragSquare = board.pressSquare
            if (board.dragSquare >= 0) {
                board.dragX = mouse.x
                board.dragY = mouse.y
            }
        }

        onReleased: {
            preventStealing = false
            if (board.dragSquare >= 0) {
                var from = board.dragSquare
                var to = board.squareAt(mouse.x, mouse.y)
                board.settling = true
                settleTimer.restart()
                board.dragSquare = -1
                if (to !== from && board.targets.indexOf(to) >= 0)
                    board.tryMove(from, to)
            }
            board.pressSquare = -1
        }

        onCanceled: {
            preventStealing = false
            board.dragSquare = -1
            board.pressSquare = -1
        }
    }

    // Promotion picker
    Rectangle {
        anchors.fill: parent
        z: 20
        visible: board.promotionFrom >= 0
        color: Qt.rgba(0, 0, 0, 0.5)

        MouseArea {
            anchors.fill: parent
            onClicked: {
                board.promotionFrom = -1
                board.promotionTo = -1
            }
        }

        Column {
            readonly property bool fromTop: board.promotionTo >= 0
                                            && board.squareY(board.promotionTo) < board.height / 2
            x: board.promotionTo >= 0 ? board.squareX(board.promotionTo) : 0
            y: fromTop ? 0 : board.height - height
            Repeater {
                model: ["q", "n", "r", "b"]
                Rectangle {
                    width: board.squareSize
                    height: board.squareSize
                    radius: width / 2
                    color: promoArea.pressed ? Theme.highlightBackgroundColor : "#e0e0e0"
                    Image {
                        anchors.fill: parent
                        anchors.margins: parent.width * 0.05
                        sourceSize.width: width
                        sourceSize.height: height
                        source: "image://pieces/" + appSettings.pieceSet + "/"
                                + board.movableColor.charAt(0) + modelData.toUpperCase()
                    }
                    MouseArea {
                        id: promoArea
                        anchors.fill: parent
                        onClicked: board.promote(modelData)
                    }
                }
            }
        }
    }
}
