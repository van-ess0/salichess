// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../js/PuzzleThemes.js" as Themes

// One theme of the puzzle dashboard: its name, how often it came up and how
// often it was solved at the first try.
BackgroundItem {
    id: row

    // A map of "theme", "puzzles" and "solvedPercent" (PuzzleDashboard).
    property var theme: ({})

    height: Theme.itemSizeSmall

    Label {
        anchors {
            left: parent.left
            leftMargin: Theme.horizontalPageMargin
            right: share.left
            rightMargin: Theme.paddingLarge
            verticalCenter: parent.verticalCenter
        }
        truncationMode: TruncationMode.Fade
        color: row.highlighted ? Theme.highlightColor : Theme.primaryColor
        text: Themes.name(row.theme.theme)
    }

    Label {
        id: share
        anchors {
            right: parent.right
            rightMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }
        font.pixelSize: Theme.fontSizeSmall
        // Green for a theme that mostly goes right, red for one that mostly
        // does not.
        color: row.theme.solvedPercent >= 70 ? "#629924"
                                             : (row.theme.solvedPercent < 40 ? "#df5353" : Theme.secondaryColor)
        text: row.theme.solvedPercent + "% • " + qsTr("%n puzzle(s)", "", row.theme.puzzles)
    }
}
