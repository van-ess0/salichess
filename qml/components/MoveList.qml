// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0

// Horizontal strip of moves; tap a move to view that position.
SilicaListView {
    id: list

    property QtObject game

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
        width: moveLabel.implicitWidth + 2 * Theme.paddingSmall
        height: list.height
        enabled: !hidden
        highlighted: down || current

        Label {
            id: moveLabel
            anchors.centerIn: parent
            text: (index % 2 === 0 ? (Math.floor(index / 2) + 1) + ". " : "") + modelData
            color: parent.current ? Theme.highlightColor
                                  : (parent.hidden ? Theme.secondaryColor : Theme.primaryColor)
            font.pixelSize: Theme.fontSizeSmall
        }
        onClicked: game.viewPly(index + 1)
    }

    HorizontalScrollDecorator {}
}
