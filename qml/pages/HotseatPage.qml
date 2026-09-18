// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.salichess 1.0
import "../components"

// Two players, one phone. The board sits in the middle of the screen and
// each player's clock fills the space on their own side of it, upside-down
// for the player sitting opposite. There is no page header: the screen
// belongs to both players equally, so nothing is drawn only one way up.
Page {
    id: page

    property int initialSeconds: appSettings.hotseatSeconds
    property int increment: appSettings.hotseatIncrement
    property bool autoClock: appSettings.hotseatAutoClock
    // Which player sits at the bottom edge, nearest whoever opened the app.
    property bool whiteAtBottom: true
    property bool started: false

    readonly property string bottomColor: whiteAtBottom ? "white" : "black"
    readonly property string topColor: whiteAtBottom ? "black" : "white"
    readonly property bool inProgress: hotseat.state !== HotseatController.Finished
                                       && hotseat.game.ply > 0
    readonly property string toMoveName: hotseat.sideToMove === "white" ? qsTr("White") : qsTr("Black")

    // The clocks may not be squeezed below this, so the board is only as big
    // as what is left over.
    readonly property real minClockSize: Theme.itemSizeLarge
    readonly property real boardSize: page.isPortrait
                                      ? Math.min(page.width, page.height - 2 * minClockSize)
                                      : Math.min(page.height, page.width - 2 * minClockSize)

    allowedOrientations: Orientation.All
    // While a game is on, leaving is done from the pull-down menu, so that a
    // swipe meant for the board cannot throw the game away.
    backNavigation: !inProgress

    HotseatController {
        id: hotseat
        initialSeconds: page.initialSeconds
        increment: page.increment
        autoSwitch: page.autoClock
    }

    onStatusChanged: {
        if (status === PageStatus.Active) {
            app.boardVisible = true
            // Not in Component.onCompleted: Silica creates this page as soon
            // as the dialog before it opens, long before the players are here.
            if (!page.started) {
                page.started = true
                hotseat.start()
            }
        } else if (status === PageStatus.Deactivating) {
            app.boardVisible = false
            hotseat.pause()
        }
    }

    Component.onDestruction: app.boardVisible = false

    // Nobody is watching a clock that is not on the screen.
    Connections {
        target: Qt.application
        onStateChanged: {
            if (Qt.application.state !== Qt.ApplicationActive)
                hotseat.pause()
        }
    }

    function newGame() {
        page.whiteAtBottom = page.whiteAtBottom
        hotseat.start()
    }

    SilicaFlickable {
        id: flickable
        anchors.fill: parent
        // Everything is sized to fit the screen, so there is nothing to
        // flick; the SilicaFlickable is here for the pull-down menu.
        contentHeight: height

        // The bottom item is reached first when pulling, so the irreversible
        // actions sit at the top and each gets a remorse timer.
        PullDownMenu {
            MenuItem {
                text: qsTr("Leave game")
                onClicked: {
                    if (page.inProgress)
                        Remorse.popupAction(page, qsTr("Leaving"), function() { pageStack.pop() })
                    else
                        pageStack.pop()
                }
            }
            MenuItem {
                visible: hotseat.state === HotseatController.Running && hotseat.game.ply > 0
                text: qsTr("%1 resigns").arg(page.toMoveName)
                onClicked: {
                    var color = hotseat.sideToMove
                    Remorse.popupAction(page, qsTr("Resigning"), function() { hotseat.resign(color) })
                }
            }
            MenuItem {
                text: qsTr("New game")
                onClicked: {
                    if (page.inProgress)
                        Remorse.popupAction(page, qsTr("Starting a new game"), function() { page.newGame() })
                    else
                        page.newGame()
                }
            }
            MenuItem {
                text: qsTr("Turn the board round")
                onClicked: page.whiteAtBottom = !page.whiteAtBottom
            }
            MenuItem {
                visible: hotseat.game.ply > 0 && hotseat.state !== HotseatController.Setup
                text: qsTr("Take back move")
                onClicked: hotseat.undoMove()
            }
            MenuItem {
                visible: !hotseat.gameOver && !hotseat.untimed
                text: hotseat.state === HotseatController.Paused ? qsTr("Go on") : qsTr("Pause")
                onClicked: {
                    if (hotseat.state === HotseatController.Paused)
                        hotseat.resume()
                    else
                        hotseat.pause()
                }
            }
        }

        Item {
            id: layout
            anchors.fill: parent

            HotseatClock {
                id: topClock
                controller: hotseat
                color: page.topColor
                inverted: true
                x: 0
                y: 0
                width: page.isPortrait ? layout.width : (layout.width - board.width) / 2
                height: page.isPortrait ? (layout.height - board.height) / 2 : layout.height
                onNewGameRequested: page.newGame()
            }

            ChessBoard {
                id: board
                anchors.centerIn: parent
                width: page.boardSize
                game: hotseat.game
                flipped: !page.whiteAtBottom
                // Each side's men face the player they belong to.
                facePlayers: true
                interactive: hotseat.acceptsMoves
                // Whoever is to move may move: that is what hotseat means.
                movableColor: "both"
                onMoveRequested: hotseat.move(uci)
            }

            HotseatClock {
                id: bottomClock
                controller: hotseat
                color: page.bottomColor
                inverted: false
                x: page.isPortrait ? 0 : layout.width - width
                y: page.isPortrait ? layout.height - height : 0
                width: topClock.width
                height: topClock.height
                onNewGameRequested: page.newGame()
            }

            // A paused clock covers the board, so that a player cannot study
            // the position on the other's time.
            Rectangle {
                anchors.fill: board
                visible: hotseat.state === HotseatController.Paused
                color: Theme.rgba(Theme.overlayBackgroundColor, 0.92)

                Label {
                    anchors.centerIn: parent
                    font.pixelSize: Theme.fontSizeExtraLarge
                    color: Theme.highlightColor
                    text: qsTr("Paused")
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: hotseat.resume()
                }
            }

            // How the game ended, in words, for whoever is reading the board
            // rather than their own button.
            Label {
                anchors {
                    horizontalCenter: board.horizontalCenter
                    bottom: board.bottom
                    bottomMargin: Theme.paddingLarge
                }
                width: board.width - 2 * Theme.paddingLarge
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                visible: hotseat.gameOver
                font.pixelSize: Theme.fontSizeMedium
                color: Theme.primaryColor
                text: hotseat.resultText

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -Theme.paddingMedium
                    z: -1
                    radius: Theme.paddingSmall
                    color: Theme.rgba(Theme.overlayBackgroundColor, 0.85)
                }
            }
        }
    }
}
