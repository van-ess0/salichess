// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0

// First / previous / next / latest buttons for browsing a game.
Row {
    id: controls

    property QtObject game
    property bool flipEnabled: true
    signal flipRequested()

    readonly property real buttonWidth: width / (flipEnabled ? 5 : 4)

    IconButton {
        width: controls.buttonWidth
        icon.source: "image://theme/icon-m-simple-previous"
        enabled: controls.game && controls.game.viewPly > controls.game.firstViewablePly
        onClicked: controls.game.viewFirst()
    }
    IconButton {
        width: controls.buttonWidth
        icon.source: "image://theme/icon-m-left"
        enabled: controls.game && controls.game.viewPly > controls.game.firstViewablePly
        onClicked: controls.game.viewPrevious()
    }
    IconButton {
        visible: controls.flipEnabled
        width: controls.buttonWidth
        icon.source: "image://theme/icon-m-flip"
        onClicked: controls.flipRequested()
    }
    IconButton {
        width: controls.buttonWidth
        icon.source: "image://theme/icon-m-right"
        enabled: controls.game && !controls.game.atLatest
        onClicked: controls.game.viewNext()
    }
    IconButton {
        width: controls.buttonWidth
        icon.source: "image://theme/icon-m-simple-next"
        enabled: controls.game && !controls.game.atLatest
        onClicked: controls.game.viewLatest()
    }
}
