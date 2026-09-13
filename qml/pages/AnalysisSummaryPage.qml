// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components"
import "../js/Util.js" as Util

// What Lichess made of a game as a whole: how the evaluation moved, and how
// well each player did. Kept off the analysis board so that it can stay on
// one screen.
Page {
    id: page

    // The GameAnalysis of the game being looked at.
    property QtObject analysis

    allowedOrientations: Orientation.All

    function accuracyText(player) {
        if (!player || !player.accuracy)
            return ""
        return qsTr("%1% accuracy").arg(player.accuracy)
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: page.width

            PageHeader {
                title: qsTr("Computer analysis")
                description: page.analysis.openingName
            }

            EvalGraph {
                width: parent.width
                height: Theme.itemSizeExtraLarge
                analysis: page.analysis
                game: page.analysis.game
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                topPadding: Theme.paddingMedium
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                color: Theme.highlightColor
                text: page.analysis.resultText
            }

            Repeater {
                model: ["white", "black"]

                Item {
                    id: playerSummary
                    readonly property var player: modelData === "white" ? page.analysis.white
                                                                        : page.analysis.black
                    width: column.width
                    height: playerColumn.height + Theme.paddingLarge

                    Column {
                        id: playerColumn
                        x: Theme.horizontalPageMargin
                        y: Theme.paddingLarge
                        width: parent.width - 2 * x

                        Label {
                            width: parent.width
                            truncationMode: TruncationMode.Fade
                            text: Util.playerName(playerSummary.player.name, playerSummary.player.title)
                                  + " • "
                                  + (modelData === "white" ? qsTr("White") : qsTr("Black"))
                                  + (playerSummary.player.rating
                                     ? " (" + playerSummary.player.rating + ")" : "")
                        }
                        Label {
                            width: parent.width
                            wrapMode: Text.Wrap
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.secondaryColor
                            visible: page.analysis.hasServerAnalysis
                            text: [page.accuracyText(playerSummary.player),
                                   qsTr("%1 average centipawn loss").arg(playerSummary.player.acpl)]
                                  .filter(function(s) { return s !== "" }).join(" • ")
                        }
                        Label {
                            width: parent.width
                            wrapMode: Text.Wrap
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.secondaryColor
                            visible: page.analysis.hasServerAnalysis
                            text: qsTr("%1 inaccuracies, %2 mistakes, %3 blunders")
                                  .arg(playerSummary.player.inaccuracy)
                                  .arg(playerSummary.player.mistake)
                                  .arg(playerSummary.player.blunder)
                        }
                    }
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: !page.analysis.hasServerAnalysis
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryHighlightColor
                text: page.analysis.requestingAnalysis
                      ? qsTr("Lichess is analysing the game. This takes a minute or two.")
                      : qsTr("Lichess has not analysed this game.")
            }
        }

        VerticalScrollDecorator {}
    }
}
