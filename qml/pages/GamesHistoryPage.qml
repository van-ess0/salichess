// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.salichess 1.0
import "../components"
import "../js/Util.js" as Util

// The games already played, newest first. Tapping one opens its analysis.
Page {
    id: page

    // Whose games; empty means the logged-in user.
    property string username

    allowedOrientations: Orientation.All

    GamesHistoryModel {
        id: history
        username: page.username
    }

    SilicaListView {
        id: listView
        anchors.fill: parent
        model: history
        // The last screenful is the cue to fetch the next page.
        onContentYChanged: {
            if (history.hasMore && !history.loading
                    && contentY + height > contentHeight - page.height)
                history.loadMore()
        }

        PullDownMenu {
            MenuItem {
                text: qsTr("Filter")
                onClicked: pageStack.push(Qt.resolvedUrl("GameFilterDialog.qml"), { model: history })
            }
            MenuItem {
                visible: history.filtered
                text: qsTr("Clear filters")
                onClicked: history.clearFilters()
            }
            MenuItem {
                text: qsTr("Refresh")
                onClicked: history.refresh()
            }
        }

        header: Column {
            width: listView.width

            PageHeader {
                title: qsTr("Games")
                description: page.username !== "" ? page.username
                                                  : (history.filtered ? qsTr("Filtered") : "")
            }

            OfflineBanner {
                description: qsTr("Past games need a connection")
            }
        }

        delegate: BackgroundItem {
            id: gameItem
            width: listView.width
            height: Theme.itemSizeExtraLarge
            onClicked: pageStack.push(Qt.resolvedUrl("AnalysisPage.qml"), { gameId: model.gameId })

            // Won, lost or drawn, as a stripe along the leading edge.
            Rectangle {
                anchors {
                    left: parent.left
                    top: parent.top
                    bottom: parent.bottom
                    topMargin: Theme.paddingSmall
                    bottomMargin: Theme.paddingSmall
                }
                width: Theme.paddingSmall / 2
                color: {
                    if (model.result === "win")
                        return "#629924"
                    if (model.result === "loss")
                        return "#df5353"
                    return Theme.secondaryColor
                }
            }

            MiniBoard {
                id: thumb
                x: Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                width: parent.height - Theme.paddingMedium
                fen: model.fen
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
                    color: gameItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                    text: Util.playerName(model.opponentName, model.opponentTitle)
                          + (model.opponentRating > 0 ? " (" + model.opponentRating + ")" : "")
                }

                Row {
                    width: parent.width
                    spacing: Theme.paddingSmall

                    Label {
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.secondaryColor
                        text: model.resultText !== "" ? model.resultText : qsTr("Game over")
                        width: Math.min(implicitWidth, parent.width - diffLabel.width - parent.spacing)
                        truncationMode: TruncationMode.Fade
                    }
                    Label {
                        id: diffLabel
                        font.pixelSize: Theme.fontSizeSmall
                        visible: model.rated && model.ratingDiff !== 0
                        color: model.ratingDiff > 0 ? "#629924" : "#df5353"
                        text: (model.ratingDiff > 0 ? "+" : "−") + Math.abs(model.ratingDiff)
                    }
                }

                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    text: [Util.formatGameDate(model.createdAt), model.perf,
                           model.rated ? qsTr("Rated") : qsTr("Casual"), model.opening]
                          .filter(function(s) { return s !== "" && s !== undefined }).join(" • ")
                }
            }
        }

        footer: Item {
            width: listView.width
            height: Theme.itemSizeMedium

            BusyIndicator {
                anchors.centerIn: parent
                running: history.loading
                size: BusyIndicatorSize.Medium
            }
        }

        ViewPlaceholder {
            enabled: history.count === 0 && !history.loading
            text: {
                if (!session.loggedIn && page.username === "")
                    return qsTr("Log in to see your games")
                if (history.errorString !== "")
                    return history.errorString
                if (history.filtered)
                    return qsTr("No games match the filter")
                return qsTr("No games played yet")
            }
            hintText: history.filtered ? qsTr("Change it in the pull-down menu") : ""
        }

        VerticalScrollDecorator {}
    }
}
