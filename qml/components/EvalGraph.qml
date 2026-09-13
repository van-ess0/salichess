// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../js/Util.js" as Util

// The course of a game as Lichess draws it: the chance of white winning,
// ply by ply, filled from the bottom. Inaccuracies, mistakes and blunders
// are marked, and tapping or dragging moves the board to that move.
Item {
    id: graph

    // A GameAnalysis, for evalPoints(), and the ChessGame it belongs to.
    property QtObject analysis
    property QtObject game

    // evalPoints() is a function, so the graph is rebuilt whenever the
    // analysis says it has new data rather than by a binding.
    property var points: []
    readonly property int plies: game ? game.ply : 0
    readonly property bool hasData: points.length > 0

    // The white area, and the ground it sits on. Fixed colours: the graph is
    // read as "white above, black below" whatever the ambience. Strings, not
    // colours: Canvas takes CSS colours, and a QML colour value assigned to
    // fillStyle leaves it invalid, which paints nothing at all.
    readonly property string whiteColor: "#e8e8e8"
    readonly property string blackColor: "#3a3a3a"

    height: Theme.itemSizeMedium
    visible: hasData

    function reloadPoints() {
        points = analysis ? analysis.evalPoints() : []
        canvas.requestPaint()
    }

    function plyAt(x) {
        if (plies <= 0)
            return 0
        return Math.max(1, Math.min(plies, Math.round(x / width * plies)))
    }

    onAnalysisChanged: reloadPoints()
    onWidthChanged: canvas.requestPaint()
    Component.onCompleted: reloadPoints()

    Connections {
        target: graph.analysis
        onInfoChanged: graph.reloadPoints()
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        // See the arrow canvas in ChessBoard.qml: Cooperative mode did not
        // put anything on the screen on the device.
        renderStrategy: Canvas.Immediate

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.clearRect(0, 0, width, height)
            if (!graph.hasData || graph.plies <= 0)
                return

            var pts = graph.points
            var w = width
            var h = height

            function xOf(ply) { return ply / graph.plies * w }
            function yOf(win) { return h - win / 100 * h }

            ctx.fillStyle = graph.blackColor
            ctx.fillRect(0, 0, w, h)

            // The white area follows the win chances, and stays flat over
            // the plies Lichess did not evaluate.
            ctx.beginPath()
            ctx.moveTo(0, yOf(50))
            for (var i = 0; i < pts.length; ++i)
                ctx.lineTo(xOf(pts[i].ply), yOf(pts[i].win))
            ctx.lineTo(xOf(pts[pts.length - 1].ply), h)
            ctx.lineTo(0, h)
            ctx.closePath()
            ctx.fillStyle = graph.whiteColor
            ctx.fill()

            // The middle line, so an even game is easy to spot.
            ctx.beginPath()
            ctx.moveTo(0, yOf(50))
            ctx.lineTo(w, yOf(50))
            ctx.lineWidth = 1
            ctx.strokeStyle = "rgba(128, 128, 128, 0.6)"
            ctx.stroke()

            // Mistakes, as dots on the curve.
            for (var j = 0; j < pts.length; ++j) {
                var color = Util.judgmentColor(pts[j].judgment)
                if (color === "")
                    continue
                ctx.beginPath()
                ctx.arc(xOf(pts[j].ply), yOf(pts[j].win), Math.max(2, h * 0.05), 0, 2 * Math.PI)
                ctx.fillStyle = color
                ctx.fill()
            }
        }
    }

    // Where the board stands. Drawn outside the canvas so that stepping
    // through the game does not repaint the whole graph.
    Rectangle {
        visible: graph.game && graph.game.viewPly > 0
        x: graph.game && graph.plies > 0 ? graph.game.viewPly / graph.plies * graph.width - width / 2 : 0
        width: Math.max(2, Theme.paddingSmall / 2)
        height: graph.height
        color: Theme.highlightColor
        opacity: 0.9
    }

    MouseArea {
        anchors.fill: parent
        enabled: graph.game !== null
        onPressed: graph.game.goToPly(graph.plyAt(mouse.x))
        onPositionChanged: {
            if (pressed)
                graph.game.goToPly(graph.plyAt(mouse.x))
        }
    }
}
