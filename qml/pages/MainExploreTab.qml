// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components"

MainTab {
    TileGrid {
        MenuTile {
            onClicked: pageStack.push(Qt.resolvedUrl("BoardEditorPage.qml"))
            icon: "image://theme/icon-m-edit"
            title: qsTr("Set up a position")
            subtitle: qsTr("Copy a board to analyse it or play it out")
        }

        MenuTile {
            onClicked: pageStack.push(Qt.resolvedUrl("PuzzlePage.qml"), { daily: true })
            icon: "image://theme/icon-m-date"
            title: qsTr("Daily puzzle")
            subtitle: qsTr("The Lichess puzzle of the day")
        }

        LiveBoardTile {
            onClicked: pageStack.push(Qt.resolvedUrl("TvPage.qml"))
            icon: "image://theme/icon-m-video"
            title: qsTr("Lichess TV")
            subtitle: qsTr("Watch the games Lichess features")
        }
    }
}
