// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0

// Lays MenuTiles out in rows; hidden tiles leave no gap.
Flow {
    id: grid

    readonly property int columns: Math.max(2, Math.floor(width / (Theme.itemSizeHuge * 1.5)))
    readonly property real tileWidth: Math.floor((width - (columns - 1) * spacing) / columns)
    // Every tile has the same slots (icon, two lines of title, two of detail),
    // so that the text lines up across a row however long it is.
    readonly property real titleHeight: Math.ceil(Theme.fontSizeSmall * 1.3) * 2
    readonly property real subtitleHeight: Math.ceil(Theme.fontSizeExtraSmall * 1.3) * 2
    readonly property real tileHeight: 2 * Theme.paddingLarge + Theme.iconSizeMedium
                                       + titleHeight + subtitleHeight + 2 * Theme.paddingSmall

    x: Theme.horizontalPageMargin
    width: parent.width - 2 * x
    spacing: Theme.paddingMedium
}
