// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0

// Icon, title and subtitle for a BackgroundItem entry.
Item {
    id: entry

    property alias icon: iconImage.source
    property alias title: titleLabel.text
    property alias subtitle: subtitleLabel.text
    readonly property bool highlighted: !!(parent && parent.highlighted)

    anchors.fill: parent

    HighlightImage {
        id: iconImage
        x: Theme.horizontalPageMargin
        anchors.verticalCenter: parent.verticalCenter
        highlighted: entry.highlighted
    }

    Column {
        anchors {
            left: iconImage.right
            leftMargin: Theme.paddingLarge
            right: parent.right
            rightMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }
        Label {
            id: titleLabel
            width: parent.width
            truncationMode: TruncationMode.Fade
            color: entry.highlighted ? Theme.highlightColor : Theme.primaryColor
        }
        Label {
            id: subtitleLabel
            width: parent.width
            truncationMode: TruncationMode.Fade
            font.pixelSize: Theme.fontSizeExtraSmall
            color: entry.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
        }
    }
}
