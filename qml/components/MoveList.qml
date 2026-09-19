// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../js/Util.js" as Util

// Horizontal strip of moves; tap a move to view that position.
SilicaListView {
    id: list

    property QtObject game
    // Lichess judgments, one per ply (GameAnalysis.judgments()). Left empty
    // where a game has not been analysed, and by the pages that only play.
    property var annotations: []

    orientation: ListView.Horizontal
    height: Theme.itemSizeExtraSmall
    clip: true
    spacing: Theme.paddingSmall
    model: game ? game.sanMoves : []
    currentIndex: game ? game.viewPly - 1 : -1
    highlightFollowsCurrentItem: true
    highlightRangeMode: ListView.ApplyRange
    preferredHighlightBegin: width / 3
    preferredHighlightEnd: width * 2 / 3
    header: Item { width: Theme.horizontalPageMargin; height: 1 }
    footer: Item { width: Theme.horizontalPageMargin; height: 1 }

    delegate: BackgroundItem {
        readonly property bool current: game && game.viewPly === index + 1
        readonly property bool hidden: game && index + 1 < game.firstViewablePly
        readonly property string judgment: index < list.annotations.length
                                           ? list.annotations[index] : ""
        width: moveLabel.implicitWidth + 2 * Theme.paddingSmall
        height: list.height
        enabled: !hidden
        highlighted: down || current

        Label {
            id: moveLabel
            anchors.centerIn: parent
            // Counted from the game's first move, which a game set up
            // from a position may not have started with.
            readonly property int played: index + (game ? game.startPly : 0)
            text: (played % 2 === 0 ? (Math.floor(played / 2) + 1) + ". "
                                    : (index === 0 ? (Math.floor(played / 2) + 1) + "… " : ""))
                  + modelData
                  + Util.judgmentGlyph(parent.judgment)
            color: {
                if (parent.current)
                    return Theme.highlightColor
                if (parent.hidden)
                    return Theme.secondaryColor
                if (parent.judgment !== "")
                    return Util.judgmentColor(parent.judgment)
                return Theme.primaryColor
            }
            font.pixelSize: Theme.fontSizeSmall
        }
        onClicked: game.goToPly(index + 1)
    }

    HorizontalScrollDecorator {}
}
