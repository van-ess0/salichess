// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.salichess 1.0
import "../js/Util.js" as Util

// The moves of a game the way a PGN reads them: the game itself first, and
// every side line indented underneath the move it branches off from. Tap a
// move to put it on the board.
//
// It scrolls on its own, so the page around it can stay put, and it follows
// the board: the move being looked at is always brought into view.
SilicaFlickable {
    id: view

    property QtObject game
    // Lichess judgments, one per ply of the main line (GameAnalysis).
    property var annotations: []

    readonly property int currentNode: tree.currentNode
    // Where the move on the board sits, so it can be scrolled to.
    property real currentY: -1
    property real currentHeight: 0

    contentHeight: column.height
    clip: true

    function showCurrent() {
        if (currentY < 0 || contentHeight <= height)
            return
        var margin = Theme.paddingMedium
        if (currentY - margin < contentY)
            contentY = Math.max(0, currentY - margin)
        else if (currentY + currentHeight + margin > contentY + height)
            contentY = Math.min(contentHeight - height,
                                currentY + currentHeight + margin - height)
    }

    onCurrentNodeChanged: showCurrent()
    onHeightChanged: showCurrent()

    // The C++ flattener; it is registered as MoveTreeModel because this
    // file has taken the name MoveTree.
    MoveTreeModel {
        id: tree
        game: view.game
    }

    Column {
        id: column
        width: view.width

        Repeater {
            model: tree.paragraphs

            Flow {
                id: run
                readonly property int depth: modelData.depth
                x: Theme.horizontalPageMargin + depth * Theme.paddingLarge
                width: view.width - x - Theme.horizontalPageMargin

                Repeater {
                    model: modelData.moves

                    BackgroundItem {
                        id: moveItem
                        readonly property bool current: modelData.node === view.currentNode
                        readonly property bool mainline: run.depth === 0
                        readonly property bool hidden: view.game && mainline
                                                       && modelData.ply < view.game.firstViewablePly
                        // Judgments belong to the game as it was played.
                        readonly property string judgment: mainline
                                                           && modelData.ply - 1 < view.annotations.length
                                                           ? view.annotations[modelData.ply - 1] : ""
                        width: moveLabel.implicitWidth + Theme.paddingMedium
                        height: moveLabel.implicitHeight + Theme.paddingSmall
                        enabled: !hidden
                        highlighted: down || current
                        onClicked: view.game.goToNode(modelData.node)

                        onCurrentChanged: if (current) reportPosition()
                        Component.onCompleted: if (current) reportPosition()

                        function reportPosition() {
                            view.currentY = run.y + y
                            view.currentHeight = height
                            view.showCurrent()
                        }

                        Rectangle {
                            anchors.fill: parent
                            visible: moveItem.current
                            radius: Theme.paddingSmall / 2
                            color: Theme.rgba(Theme.highlightBackgroundColor, 0.3)
                        }

                        Label {
                            id: moveLabel
                            anchors.centerIn: parent
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: {
                                if (moveItem.current)
                                    return Theme.highlightColor
                                if (moveItem.hidden)
                                    return Theme.secondaryColor
                                if (moveItem.judgment !== "")
                                    return Util.judgmentColor(moveItem.judgment)
                                return moveItem.mainline ? Theme.primaryColor : Theme.secondaryColor
                            }
                            text: {
                                // "12." before a white move, "12…" when a run
                                // starts on a black one.
                                var prefix = ""
                                if (modelData.white)
                                    prefix = modelData.number + "."
                                else if (modelData.first)
                                    prefix = modelData.number + "…"
                                return prefix + modelData.san + Util.judgmentGlyph(moveItem.judgment)
                            }
                        }
                    }
                }
            }
        }
    }

    VerticalScrollDecorator {}
}
