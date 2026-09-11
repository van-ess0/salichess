// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components"
import "../js/Util.js" as Util

CoverBackground {
    id: cover

    readonly property QtObject game: app.activeGame

    // Current game: mini board with turn and clocks.
    Column {
        visible: cover.game !== null
        anchors {
            top: parent.top
            topMargin: Theme.paddingMedium
            left: parent.left
            right: parent.right
            margins: Theme.paddingMedium
        }
        spacing: Theme.paddingSmall

        MiniBoard {
            width: parent.width
            fen: cover.game ? cover.game.game.fen : ""
            flipped: cover.game ? cover.game.myColor === "black" : false
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            truncationMode: TruncationMode.Fade
            color: Theme.highlightColor
            text: {
                if (!cover.game)
                    return ""
                if (cover.game.gameOver)
                    return qsTr("Game over")
                return cover.game.isMyTurn ? qsTr("Your turn") : qsTr("Waiting")
            }
        }

        Label {
            visible: cover.game !== null && cover.game.hasClock
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.secondaryColor
            // The clocks change five times a second. Only follow them while
            // the cover is shown, and only redraw once a second.
            text: cover.status !== Cover.Inactive && cover.game
                  ? Util.formatClockSeconds(cover.game.whiteTime) + " – " + Util.formatClockSeconds(cover.game.blackTime)
                  : ""
        }
    }

    // Overview
    Column {
        visible: cover.game === null
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.paddingLarge
        spacing: Theme.paddingMedium

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: "salichess"
            font.pixelSize: Theme.fontSizeLarge
            color: Theme.highlightColor
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            visible: session.loggedIn
            color: ongoingGames.myTurnCount > 0 ? Theme.highlightColor : Theme.primaryColor
            font.pixelSize: ongoingGames.myTurnCount > 0 ? Theme.fontSizeMedium : Theme.fontSizeSmall
            font.bold: ongoingGames.myTurnCount > 0
            text: {
                var opponents = ongoingGames.myTurnOpponents
                if (opponents.length === 1)
                    return qsTr("Your move against %1").arg(opponents[0])
                if (opponents.length > 1)
                    return qsTr("Your move in %1 games").arg(opponents.length)
                return qsTr("Games: %1").arg(ongoingGames.count)
            }
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            visible: challenges.incomingCount > 0
            color: Theme.highlightColor
            text: qsTr("Challenges: %1").arg(challenges.incomingCount)
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            visible: outgoingChallenges.pendingCount > 0
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.secondaryColor
            text: qsTr("Sent, waiting: %1").arg(outgoingChallenges.pendingCount)
        }
    }
}
