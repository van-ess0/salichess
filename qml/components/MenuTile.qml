// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0

// A tile with an icon, a title and a line of detail, for a TileGrid.
BackgroundItem {
    id: tile

    property alias icon: iconImage.source
    property alias title: titleLabel.text
    property alias subtitle: subtitleLabel.text

    width: parent.tileWidth
    height: parent.tileHeight

    Rectangle {
        anchors.fill: parent
        radius: Theme.paddingSmall
        color: Theme.rgba(tile.highlighted ? Theme.highlightBackgroundColor : Theme.primaryColor,
                          tile.highlighted ? Theme.highlightBackgroundOpacity : 0.15)
    }

    Column {
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            margins: Theme.paddingMedium
            topMargin: Theme.paddingLarge
        }
        spacing: Theme.paddingSmall

        HighlightImage {
            id: iconImage
            anchors.horizontalCenter: parent.horizontalCenter
            width: Theme.iconSizeMedium
            height: width
            sourceSize: Qt.size(width, height)
            highlighted: tile.highlighted
        }
        Label {
            id: titleLabel
            width: parent.width
            height: tile.parent.titleHeight
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
            font.pixelSize: Theme.fontSizeSmall
            color: tile.highlighted ? Theme.highlightColor : Theme.primaryColor
        }
        Label {
            id: subtitleLabel
            width: parent.width
            height: tile.parent.subtitleHeight
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignTop
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
            font.pixelSize: Theme.fontSizeExtraSmall
            color: tile.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
        }
    }
}
