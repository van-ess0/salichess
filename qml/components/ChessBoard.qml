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
    // "white", "black", "both" (whoever is to move, for the analysis board)
    // or "" for none.
    property string movableColor: ""
    property int hintSquare: -1
    property int hintTarget: -1
    // Moves to point at, e.g. the ones the engine likes. Each is
    // {"from", "to", "weight"}, where the weight (0..1) is how strongly it
    // is drawn: the best move solid, the runners-up faint.
    property var arrows: []
    // Hotseat: the men of the side sitting at the far edge of the phone are
    // turned round, so that each player sees their own the right way up.
    property bool facePlayers: false
    readonly property string farSideColor: flipped ? "w" : "b"

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
    // Set just before a drag ends to let the dropped piece glide from under
    // the finger onto its square. It has to be on before the drag ends:
    // ending it changes the piece's position and whether it may animate at
    // the same moment, and the position wins.
    property bool gliding: false
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

    // The side that may move in the position on show. With variations a
    // move from an earlier position starts a side line, so browsing does not
    // stand in the way; without them only the latest position can be played.
    function movingColor() {
        if (!game)
            return ""
        return movableColor === "both" ? game.viewSideToMove : movableColor
    }

    function canMove() {
        if (!interactive || !game || movableColor === "")
            return false
        if (!game.atLatest && !game.allowVariations)
            return false
        return game.viewSideToMove === movingColor()
    }

    function ownPiece(sq) {
        var piece = game.pieceAt(sq)
        return piece !== "" && piece.charAt(0) === movingColor().charAt(0)
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

    onGlidingChanged: if (gliding) glideTimer.restart()
    Timer {
        id: glideTimer
        interval: 250
        onTriggered: board.gliding = false
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

    // The suggested moves
    Canvas {
        id: arrowCanvas
        anchors.fill: parent
        z: 12
        visible: board.arrows.length > 0 && board.squareSize > 0
        // Immediate, not Cooperative: the arrows never reached the screen in
        // Cooperative mode on the device, and it is the mode that cannot be
        // read back either, so a drawing bug there cannot even be seen. The
        // drawing is a few lines, so painting it on the spot costs nothing.
        renderStrategy: Canvas.Immediate

        // A string, not a colour: Canvas takes CSS colours, and a QML colour
        // value assigned to strokeStyle or fillStyle leaves it invalid, which
        // paints nothing at all.
        readonly property string arrowColor: "#3692e7"

        onVisibleChanged: requestPaint()
        onWidthChanged: requestPaint()

        Connections {
            target: board
            onArrowsChanged: arrowCanvas.requestPaint()
            onFlippedChanged: arrowCanvas.requestPaint()
        }

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.clearRect(0, 0, width, height)
            if (!visible)
                return

            // Drawn weakest first, so the best move ends up on top.
            var moves = board.arrows.slice().sort(function(a, b) {
                return (a.weight === undefined ? 1 : a.weight) - (b.weight === undefined ? 1 : b.weight)
            })
            for (var i = 0; i < moves.length; ++i)
                paintArrow(ctx, moves[i])
        }

        function paintArrow(ctx, move) {
            if (move.from < 0 || move.to < 0 || move.from === move.to)
                return
            var weight = move.weight === undefined ? 1 : move.weight
            var size = board.squareSize
            var centreX = board.squareX(move.from) + size / 2
            var centreY = board.squareY(move.from) + size / 2
            var targetX = board.squareX(move.to) + size / 2
            var targetY = board.squareY(move.to) + size / 2

            var dx = targetX - centreX
            var dy = targetY - centreY
            var length = Math.sqrt(dx * dx + dy * dy)
            if (length < 1)
                return
            var ux = dx / length
            var uy = dy / length
            var head = size * (0.26 + 0.12 * weight)
            // Start and end a little inside the squares, so the arrow points
            // at the pieces rather than covering them.
            var startX = centreX + ux * size * 0.22
            var startY = centreY + uy * size * 0.22
            var endX = targetX - ux * size * 0.18
            var endY = targetY - uy * size * 0.18

            ctx.strokeStyle = arrowColor
            ctx.fillStyle = arrowColor
            ctx.globalAlpha = 0.25 + 0.5 * weight
            ctx.lineWidth = size * (0.07 + 0.09 * weight)
            ctx.lineCap = "round"
            ctx.beginPath()
            ctx.moveTo(startX, startY)
            ctx.lineTo(endX - ux * head * 0.6, endY - uy * head * 0.6)
            ctx.stroke()

            ctx.beginPath()
            ctx.moveTo(endX, endY)
            ctx.lineTo(endX - ux * head + uy * head * 0.5, endY - uy * head - ux * head * 0.5)
            ctx.lineTo(endX - ux * head - uy * head * 0.5, endY - uy * head + ux * head * 0.5)
            ctx.closePath()
            ctx.fill()
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
            rotation: board.facePlayers && model.piece.charAt(0) === board.farSideColor ? 180 : 0

            Behavior on x {
                enabled: appSettings.animatePieces && !board.settling
                         && (board.dragSquare < 0 || board.gliding)
                NumberAnimation { duration: 180; easing.type: Easing.OutQuad }
            }
            Behavior on y {
                enabled: appSettings.animatePieces && !board.settling
                         && (board.dragSquare < 0 || board.gliding)
                NumberAnimation { duration: 180; easing.type: Easing.OutQuad }
            }
            Behavior on scale {
                enabled: board.gliding
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
            // Tapping an earlier position brings the game back to the end,
            // unless side lines are allowed: there the tap is a move.
            if (!board.game.atLatest && !board.game.allowVariations) {
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
                        rotation: board.facePlayers
                                  && board.movingColor().charAt(0) === board.farSideColor ? 180 : 0
                        source: "image://pieces/" + appSettings.pieceSet + "/"
                                + board.movingColor().charAt(0) + modelData.toUpperCase()
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
