// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.salichess 1.0
import "../components"

// Lichess TV: the game Lichess features in a channel, move by move. It only
// follows the game while it is on the screen.
Page {
    id: page

    readonly property var channels: [
        { "key": "best", "name": qsTr("Top rated") },
        { "key": "bullet", "name": qsTr("Bullet") },
        { "key": "blitz", "name": qsTr("Blitz") },
        { "key": "rapid", "name": qsTr("Rapid") },
        { "key": "classical", "name": qsTr("Classical") },
        { "key": "ultraBullet", "name": qsTr("UltraBullet") },
        { "key": "computer", "name": qsTr("Computers") },
        { "key": "bot", "name": qsTr("Bots") }
    ]
    property bool userFlipped: false
    readonly property bool flipped: (tv.orientation === "black") !== userFlipped
    // Whose clock is shown where: the side shown at the bottom is the one
    // the featured game is seen from.
    readonly property string topColor: flipped ? "white" : "black"
    readonly property string bottomColor: flipped ? "black" : "white"

    allowedOrientations: Orientation.All

    TvFeed {
        id: tv
        channel: page.channels[channelBox.currentIndex].key
        // Nothing is followed while the page is off screen or the app is in
        // the background.
        active: page.status === PageStatus.Active && Qt.application.state === Qt.ApplicationActive
    }

    // The board is only worth keeping awake for while it is being watched.
    onStatusChanged: app.boardVisible = status === PageStatus.Active

    Component.onDestruction: app.boardVisible = false

    // A game ends up on a new board; start from the featured side again.
    Connections {
        target: tv
        onGameChanged: page.userFlipped = false
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: layout.height

        PullDownMenu {
            MenuItem {
                visible: tv.gameId !== ""
                text: qsTr("Open in browser")
                onClicked: Qt.openUrlExternally("https://lichess.org/" + tv.gameId)
            }
            MenuItem {
                visible: tv.gameId !== ""
                text: qsTr("Analyse this game")
                onClicked: pageStack.push(Qt.resolvedUrl("AnalysisPage.qml"), { gameId: tv.gameId })
            }
            MenuItem {
                text: qsTr("Turn the board round")
                onClicked: page.userFlipped = !page.userFlipped
            }
        }

        Item {
            id: layout
            width: page.width
            height: page.isPortrait ? header.height + boardColumn.height + infoColumn.height + Theme.paddingLarge
                                    : Math.max(page.height, infoColumn.height + Theme.paddingLarge)

            readonly property real boardSize: page.isPortrait
                                              ? page.width
                                              : Math.min(page.height - 2 * Theme.itemSizeSmall, page.width * 0.55)

            PageHeader {
                id: header
                visible: page.isPortrait
                height: visible ? implicitHeight : 0
                title: qsTr("Lichess TV")
                description: page.channels[channelBox.currentIndex].name
            }

            Column {
                id: boardColumn
                y: header.height
                width: layout.boardSize

                PlayerBar {
                    width: parent.width
                    player: page.topColor === "white" ? tv.white : tv.black
                    showClock: tv.gameId !== ""
                    timeMs: page.topColor === "white" ? tv.whiteTime : tv.blackTime
                    running: tv.runningClock === page.topColor
                    toMove: tv.gameId !== "" && tv.game.sideToMove === page.topColor
                    material: page.topColor === "white" ? tv.game.whiteMaterial : tv.game.blackMaterial
                    materialScore: page.topColor === "white" ? tv.game.materialScore : -tv.game.materialScore
                }

                ChessBoard {
                    width: parent.width
                    game: tv.game
                    flipped: page.flipped
                    interactive: false
                }

                PlayerBar {
                    width: parent.width
                    player: page.bottomColor === "white" ? tv.white : tv.black
                    showClock: tv.gameId !== ""
                    timeMs: page.bottomColor === "white" ? tv.whiteTime : tv.blackTime
                    running: tv.runningClock === page.bottomColor
                    toMove: tv.gameId !== "" && tv.game.sideToMove === page.bottomColor
                    material: page.bottomColor === "white" ? tv.game.whiteMaterial : tv.game.blackMaterial
                    materialScore: page.bottomColor === "white" ? tv.game.materialScore : -tv.game.materialScore
                }
            }

            Column {
                id: infoColumn
                x: page.isPortrait ? 0 : boardColumn.width
                y: page.isPortrait ? boardColumn.y + boardColumn.height + Theme.paddingSmall : Theme.paddingLarge
                width: page.isPortrait ? page.width : page.width - boardColumn.width

                OfflineBanner {
                    description: qsTr("Lichess TV needs a connection")
                }

                ComboBox {
                    id: channelBox
                    label: qsTr("Channel")
                    currentIndex: 0
                    menu: ContextMenu {
                        Repeater {
                            model: page.channels
                            MenuItem { text: modelData.name }
                        }
                    }
                }

                BusyLabel {
                    visible: running
                    running: tv.loading && tv.gameId === ""
                    text: qsTr("Waiting for a game…")
                }

                Label {
                    visible: tv.errorString !== ""
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    color: Theme.errorColor
                    text: tv.errorString
                }

                Label {
                    visible: tv.gameId !== "" && tv.errorString === ""
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryColor
                    text: qsTr("Swipe back to leave. The board follows the featured game and moves on by itself when it changes.")
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
