// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components"
import "../js/Time.js" as Time
import "../js/Util.js" as Util

MainTab {
    id: playTab

    // Correspondence countdowns are drawn against this.
    property double now: Date.now()


    Timer {
        running: playTab.visible && Qt.application.active && ongoingGames.count > 0
        interval: 30 * 1000
        repeat: true
        triggeredOnStart: true
        onTriggered: playTab.now = Date.now()
    }

    TileGrid {
        MenuTile {
            readonly property bool waiting: lobbySeek.seeking && !lobbySeek.correspondence
            visible: session.loggedIn
            onClicked: pageStack.push(Qt.resolvedUrl(waiting ? "SeekWaitingPage.qml" : "SeekPage.qml"))
            icon: "image://theme/icon-m-shuffle"
            title: qsTr("Play with a random opponent")
            subtitle: waiting ? qsTr("Looking for an opponent…")
                                     : qsTr("Rapid, classical or correspondence")
        }

        MenuTile {
            visible: session.loggedIn
            onClicked: pageStack.push(Qt.resolvedUrl("NewChallengeDialog.qml"))
            icon: "image://theme/icon-m-people"
            title: qsTr("Play with a friend")
            subtitle: qsTr("Send a challenge to a Lichess player")
        }

        MenuTile {
            onClicked: pageStack.push(Qt.resolvedUrl("ComputerPage.qml"))
            icon: "image://theme/icon-m-computer"
            title: qsTr("Play with the computer")
            subtitle: session.loggedIn ? qsTr("The Lichess AI, or Stockfish on this phone")
                                       : qsTr("Stockfish on this phone")
        }

        MenuTile {
            onClicked: pageStack.push(Qt.resolvedUrl("HotseatDialog.qml"))
            icon: "image://theme/icon-m-device"
            title: qsTr("Pass and play")
            subtitle: qsTr("Two players sharing this phone")
        }

        MenuTile {
            visible: session.loggedIn
            onClicked: pageStack.push(Qt.resolvedUrl("PuzzlesPage.qml"))
            icon: "image://theme/icon-m-wizard"
            title: qsTr("Puzzles")
            subtitle: session.loggedIn && session.puzzleRating > 0
                      ? qsTr("Your puzzle rating: %1").arg(session.puzzleRating)
                      : qsTr("Train tactics by theme and difficulty")
        }

        MenuTile {
            visible: session.loggedIn
            onClicked: pageStack.push(Qt.resolvedUrl("PlayerSearchPage.qml"))
            icon: "image://theme/icon-m-search"
            title: qsTr("Find a player")
            subtitle: qsTr("Look up any Lichess player")
        }
    }

    LoginPrompt {}

    // Challenges
    SectionHeader {
        visible: challenges.count > 0
        text: qsTr("Challenges")
    }

    Repeater {
        model: challenges
        ListItem {
            id: challengeItem
            width: playTab.width
            contentHeight: Theme.itemSizeMedium
            menu: challengeMenu
            onClicked: {
                // Challenges sent from the app can be reopened.
                var sent = model.direction === "out" ? outgoingChallenges.find(model.challengeId) : null
                if (sent)
                    pageStack.push(Qt.resolvedUrl("ChallengeWaitingPage.qml"), { challenge: sent })
                else
                    openMenu()
            }

            Column {
                anchors {
                    left: parent.left
                    leftMargin: Theme.horizontalPageMargin
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    color: challengeItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                    text: model.direction === "in"
                          ? qsTr("From %1").arg(Util.playerName(model.opponentName, model.opponentTitle))
                          : qsTr("To %1").arg(Util.playerName(model.opponentName, model.opponentTitle))
                }
                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryColor
                    text: [model.timeControl, model.perfName,
                           model.rated ? qsTr("Rated") : qsTr("Casual"),
                           model.direction === "out" ? qsTr("Waiting…") : ""]
                          .filter(function(s) { return s !== "" }).join(" • ")
                }
            }

            Component {
                id: challengeMenu
                ContextMenu {
                    MenuItem {
                        visible: model.direction === "in"
                        text: qsTr("Accept")
                        onClicked: challenges.accept(model.challengeId)
                    }
                    MenuItem {
                        visible: model.direction === "in"
                        text: qsTr("Decline")
                        onClicked: challenges.decline(model.challengeId, "generic")
                    }
                    MenuItem {
                        visible: model.direction === "in"
                        text: qsTr("Decline: not now")
                        onClicked: challenges.decline(model.challengeId, "later")
                    }
                    MenuItem {
                        visible: model.direction === "out"
                        text: qsTr("Cancel challenge")
                        onClicked: {
                            var sent = outgoingChallenges.find(model.challengeId)
                            if (sent)
                                sent.cancel()
                            else
                                challenges.cancel(model.challengeId)
                        }
                    }
                }
            }
        }
    }

    // Ongoing games
    SectionHeader {
        visible: ongoingGames.count > 0
        text: ongoingGames.myTurnCount > 0
              ? qsTr("Ongoing games (your turn: %1)").arg(ongoingGames.myTurnCount)
              : qsTr("Ongoing games")
    }

    Repeater {
        model: ongoingGames
        BackgroundItem {
            id: gameItem
            width: playTab.width
            height: Theme.itemSizeExtraLarge
            onClicked: {
                if (model.boardCompatible)
                    app.openGame(model.gameId)
                else
                    app.notice(qsTr("This game's time control is too fast to be played in salichess."))
            }

            MiniBoard {
                id: thumb
                x: Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                width: parent.height - Theme.paddingMedium
                fen: model.fen
                lastMove: model.lastMove
                flipped: model.color === "black"
            }

            Column {
                anchors {
                    left: thumb.right
                    leftMargin: Theme.paddingLarge
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    text: Util.playerName(model.opponentName, model.opponentTitle)
                          + (model.opponentRating > 0 ? " (" + model.opponentRating + ")" : "")
                    color: gameItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                }
                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    font.pixelSize: Theme.fontSizeSmall
                    color: model.isMyTurn ? Theme.highlightColor : Theme.secondaryColor
                    text: {
                        var turn = model.isMyTurn ? qsTr("Your turn") : qsTr("Waiting for opponent")
                        // Correspondence: how long the side to move
                        // still has for it.
                        var left = model.turnDeadline > 0 ? model.turnDeadline - playTab.now : 0
                        if (left > 0)
                            turn += " • " + qsTr("%1 left").arg(Time.formatTurnTime(left))
                        return turn
                    }
                }
                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    text: [model.perf, model.rated ? qsTr("Rated") : qsTr("Casual"),
                           model.color === "white" ? qsTr("You play white") : qsTr("You play black")]
                          .join(" • ")
                }
            }
        }
    }

    Label {
        visible: session.loggedIn && ongoingGames.count === 0 && challenges.count === 0
                 && !ongoingGames.loading
        x: Theme.horizontalPageMargin
        width: parent.width - 2 * x
        topPadding: Theme.paddingLarge * 2
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        color: Theme.secondaryHighlightColor
        font.pixelSize: Theme.fontSizeLarge
        text: qsTr("No games or challenges right now")
    }
}
