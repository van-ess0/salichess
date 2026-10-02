// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0

// A rating over time, as a line with its highest and lowest value marked.
Item {
    id: graph

    // [[ms since the epoch, rating], ...], oldest first.
    property var points: []
    readonly property bool hasData: points.length > 1
    // Strings, not colours: Canvas takes CSS colours.
    readonly property string lineColor: css(Theme.highlightColor, 1)
    readonly property string fillColor: css(Theme.highlightColor, 0.22)

    property int minRating: 0
    property int maxRating: 0

    height: Theme.itemSizeHuge
    visible: hasData

    // A CSS colour for the canvas, which does not read the #AARRGGBB strings
    // Qt gives for colours with an alpha.
    function css(color, alpha) {
        return "rgba(" + Math.round(color.r * 255) + "," + Math.round(color.g * 255) + ","
                + Math.round(color.b * 255) + "," + alpha + ")"
    }

    onPointsChanged: {
        var low = 0
        var high = 0
        for (var i = 0; i < points.length; ++i) {
            var r = points[i][1]
            if (i === 0 || r < low)
                low = r
            if (i === 0 || r > high)
                high = r
        }
        minRating = low
        maxRating = high
        canvas.requestPaint()
    }
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        renderStrategy: Canvas.Immediate

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            var pts = graph.points
            if (pts.length < 2)
                return

            var padX = Theme.horizontalPageMargin
            var padY = Theme.paddingLarge
            var left = padX
            var right = width - padX
            var top = padY
            var bottom = height - padY
            var t0 = pts[0][0]
            var t1 = pts[pts.length - 1][0]
            var span = Math.max(1, t1 - t0)
            var lo = graph.minRating
            var hi = graph.maxRating
            if (hi - lo < 50) {
                lo -= 25
                hi += 25
            }

            function xOf(t) { return left + (t - t0) / span * (right - left) }
            function yOf(r) { return bottom - (r - lo) / (hi - lo) * (bottom - top) }

            // Area under the line, then the line.
            ctx.beginPath()
            ctx.moveTo(xOf(pts[0][0]), bottom)
            for (var i = 0; i < pts.length; ++i)
                ctx.lineTo(xOf(pts[i][0]), yOf(pts[i][1]))
            ctx.lineTo(xOf(t1), bottom)
            ctx.closePath()
            ctx.fillStyle = graph.fillColor
            ctx.fill()

            ctx.beginPath()
            for (var j = 0; j < pts.length; ++j) {
                if (j === 0)
                    ctx.moveTo(xOf(pts[j][0]), yOf(pts[j][1]))
                else
                    ctx.lineTo(xOf(pts[j][0]), yOf(pts[j][1]))
            }
            ctx.lineWidth = Math.max(2, Theme.paddingSmall / 2)
            ctx.strokeStyle = graph.lineColor
            ctx.stroke()
        }
    }

    Label {
        anchors {
            left: parent.left
            leftMargin: Theme.horizontalPageMargin
            top: parent.top
        }
        font.pixelSize: Theme.fontSizeExtraSmall
        color: Theme.secondaryColor
        text: graph.maxRating
    }

    Label {
        anchors {
            left: parent.left
            leftMargin: Theme.horizontalPageMargin
            bottom: parent.bottom
        }
        font.pixelSize: Theme.fontSizeExtraSmall
        color: Theme.secondaryColor
        text: graph.minRating
    }

    Label {
        anchors {
            right: parent.right
            rightMargin: Theme.horizontalPageMargin
            bottom: parent.bottom
        }
        font.pixelSize: Theme.fontSizeExtraSmall
        color: Theme.secondaryColor
        text: graph.points.length > 1
              ? Qt.formatDate(new Date(graph.points[0][0]), "MMM yyyy") + " – "
                + Qt.formatDate(new Date(graph.points[graph.points.length - 1][0]), "MMM yyyy")
              : ""
    }
}
