// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../js/Util.js" as Util

// A followed player in a TileGrid: who they are, whether they are on Lichess
// right now, their ratings, how many games the user has played against them,
// when the latest of those was, and when they were last on Lichess.
BackgroundItem {
    id: tile

    property string username
    property string title
    property bool online
    property bool playing
    property int games
    property double lastGameAt      // the latest game against the user; ms since the epoch, 0 while unknown
    property double seenAt          // ms since the epoch, 0 while unknown
    property var ratings: []        // [{ name, rating }], only those that exist

    width: parent.tileWidth
    // The lines have slots of fixed size, so every tile comes out the same.
    height: content.height + 2 * Theme.paddingLarge

    function dayText(ms) {
        var at = new Date(ms)
        var now = new Date()
        if (at.toDateString() === now.toDateString())
            return qsTr("today")
        return Qt.formatDate(at, at.getFullYear() === now.getFullYear() ? "d MMM" : "d MMM yyyy")
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.paddingSmall
        color: Theme.rgba(tile.highlighted ? Theme.highlightBackgroundColor : Theme.primaryColor,
                          tile.highlighted ? Theme.highlightBackgroundOpacity : 0.15)
    }

    Column {
        id: content
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            margins: Theme.paddingMedium
            topMargin: Theme.paddingLarge
        }
        spacing: Theme.paddingSmall

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            truncationMode: TruncationMode.Fade
            font.pixelSize: Theme.fontSizeSmall
            color: tile.highlighted ? Theme.highlightColor : Theme.primaryColor
            text: Util.playerName(tile.username, tile.title)
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.paddingSmall

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.paddingMedium
                height: width
                radius: width / 2
                color: tile.online ? Theme.presenceColor(Theme.PresenceAvailable)
                                   : Theme.presenceColor(Theme.PresenceOffline)
            }
            Label {
                font.pixelSize: Theme.fontSizeExtraSmall
                color: tile.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                text: tile.playing ? qsTr("Playing now") : (tile.online ? qsTr("Online") : qsTr("Offline"))
            }
        }

        Item {
            width: parent.width
            height: Math.ceil(Theme.fontSizeMedium * 1.3) + Math.ceil(Theme.fontSizeExtraSmall * 1.3)

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.paddingMedium

                Repeater {
                    model: tile.ratings
                    Column {
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            font.pixelSize: Theme.fontSizeMedium
                            font.family: Theme.fontFamilyHeading
                            color: tile.highlighted ? Theme.highlightColor : Theme.primaryColor
                            text: modelData.rating
                        }
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: tile.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                            text: modelData.name
                        }
                    }
                }
            }
        }

        Label {
            width: parent.width
            height: Math.ceil(Theme.fontSizeExtraSmall * 1.3)
            horizontalAlignment: Text.AlignHCenter
            truncationMode: TruncationMode.Fade
            font.pixelSize: Theme.fontSizeExtraSmall
            color: tile.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
            text: tile.games > 0 ? qsTr("%n game(s) with you", "", tile.games)
                  : (tile.games === 0 ? qsTr("No games with you yet") : "")
        }

        Label {
            width: parent.width
            height: Math.ceil(Theme.fontSizeExtraSmall * 1.3)
            horizontalAlignment: Text.AlignHCenter
            truncationMode: TruncationMode.Fade
            font.pixelSize: Theme.fontSizeExtraSmall
            color: tile.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
            text: tile.games > 0 && tile.lastGameAt > 0 ? qsTr("Last game %1").arg(tile.dayText(tile.lastGameAt)) : ""
        }

        Label {
            width: parent.width
            height: Math.ceil(Theme.fontSizeExtraSmall * 1.3)
            horizontalAlignment: Text.AlignHCenter
            truncationMode: TruncationMode.Fade
            font.pixelSize: Theme.fontSizeExtraSmall
            color: tile.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
            text: tile.seenAt > 0 ? qsTr("Last seen %1").arg(tile.dayText(tile.seenAt)) : ""
        }
    }
}
