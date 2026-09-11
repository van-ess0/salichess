// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components"
import "../js/Time.js" as Time
import "../js/Util.js" as Util

Page {
    id: page

    allowedOrientations: Orientation.All

    onStatusChanged: {
        if (status === PageStatus.Active && session.loggedIn)
            ongoingGames.refreshIfStale()
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: qsTr("About")
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
            }
            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
            }
            MenuItem {
                enabled: !session.busy
                text: session.loggedIn ? qsTr("Log out") : qsTr("Log in")
                onClicked: {
                    if (session.loggedIn)
                        Remorse.popupAction(page, qsTr("Logging out"), function() { session.logout() })
                    else
                        pageStack.push(Qt.resolvedUrl("LoginPage.qml"))
                }
            }
            MenuItem {
                visible: session.loggedIn
                text: qsTr("Refresh")
                onClicked: {
                    challenges.refresh()
                    ongoingGames.refresh()
                    session.refreshAccount()
                }
            }
        }

        Column {
            id: column
            width: page.width

            PageHeader {
                title: "salichess"
                description: session.loggedIn
                             ? Util.playerName(session.username, session.title)
                             : qsTr("Not logged in")
            }

            // Entry points
            BackgroundItem {
                visible: session.loggedIn
                height: Theme.itemSizeMedium
                onClicked: pageStack.push(Qt.resolvedUrl("NewChallengeDialog.qml"))
                MenuEntry {
                    icon: "image://theme/icon-m-people"
                    title: qsTr("Play with a friend")
                    subtitle: qsTr("Send a challenge to a Lichess player")
                }
            }

            BackgroundItem {
                height: Theme.itemSizeMedium
                onClicked: pageStack.push(Qt.resolvedUrl("PuzzlesPage.qml"))
                MenuEntry {
                    icon: "image://theme/icon-m-wizard"
                    title: qsTr("Puzzles")
                    subtitle: session.loggedIn && session.puzzleRating > 0
                              ? qsTr("Your puzzle rating: %1").arg(session.puzzleRating)
                              : qsTr("Train tactics by theme and difficulty")
                }
            }

            BackgroundItem {
                height: Theme.itemSizeMedium
                onClicked: pageStack.push(Qt.resolvedUrl("PuzzlePage.qml"), { daily: true })
                MenuEntry {
                    icon: "image://theme/icon-m-date"
                    title: qsTr("Daily puzzle")
                    subtitle: qsTr("The Lichess puzzle of the day")
                }
            }

            // Login prompt
            Item {
                visible: !session.loggedIn
                width: parent.width
                height: loginColumn.height + 2 * Theme.paddingLarge

                Column {
                    id: loginColumn
                    y: Theme.paddingLarge
                    width: parent.width
                    spacing: Theme.paddingLarge

                    Label {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * x
                        wrapMode: Text.Wrap
                        color: Theme.secondaryHighlightColor
                        font.pixelSize: Theme.fontSizeMedium
                        text: qsTr("Log in with your Lichess account to play games with friends and to keep track of your puzzle rating.")
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Log in")
                        onClicked: pageStack.push(Qt.resolvedUrl("LoginPage.qml"))
                    }
                }
            }

            // Challenges
            SectionHeader {
                visible: challenges.count > 0
                text: qsTr("Challenges")
            }

            Repeater {
                model: challenges
                ListItem {
                    id: challengeItem
                    width: column.width
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
                    width: column.width
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
                                // Correspondence: how long the current move may still take.
                                if (model.speed === "correspondence" && model.secondsLeft > 0)
                                    turn += " • " + qsTr("%1 left").arg(Time.formatTurnTime(model.secondsLeft * 1000))
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

        VerticalScrollDecorator {}
    }
}
