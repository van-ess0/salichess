// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../js/Util.js" as Util

// What Stockfish is thinking: one row per variation, best first. Tapping a
// row plays its first move on the board, which starts a side line.
Column {
    id: view

    // The ChessGame the moves would be played on.
    property QtObject game
    // At most this many rows, so the board keeps its room.
    property int maximumRows: 3
    readonly property real rowHeight: Theme.itemSizeExtraSmall * 0.8
    readonly property int rowCount: Math.min(engine.lines.length, maximumRows)

    height: visible ? rowCount * rowHeight : 0

    Repeater {
        model: view.rowCount

        BackgroundItem {
            id: lineItem
            readonly property var line: engine.lines[index]
            width: view.width
            height: view.rowHeight
            enabled: !!line && !!line.uci
            opacity: index === 0 ? 1 : 0.7
            onClicked: view.game.playUci(line.uci)

            Label {
                id: scoreLabel
                x: Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(implicitWidth, Theme.itemSizeExtraSmall)
                font.pixelSize: Theme.fontSizeExtraSmall
                font.family: Theme.fontFamilyHeading
                color: lineItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                text: lineItem.line ? Util.formatEval(lineItem.line.cp, lineItem.line.mate) : ""
            }

            Label {
                anchors {
                    left: scoreLabel.right
                    leftMargin: Theme.paddingMedium
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                truncationMode: TruncationMode.Fade
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: lineItem.line ? lineItem.line.pv : ""
            }
        }
    }
}
