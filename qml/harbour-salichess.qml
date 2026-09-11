// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import Nemo.KeepAlive 1.2
import Nemo.Notifications 1.0
import harbour.salichess 1.0
import "pages"

ApplicationWindow {
    id: app

    // The GameController shown on screen, if any (used by the cover).
    property QtObject activeGame: null
    // Set by game and puzzle pages while they are visible.
    property bool boardVisible: false

    initialPage: Component { MainPage { } }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations

    function notice(text) {
        if (text)
            Notices.show(text, Notice.Short, Notice.Bottom)
    }

    function openGame(gameId) {
        if (!gameId)
            return
        var page = pageStack.currentPage
        if (page && page.gameId === gameId)
            return
        pageStack.push(Qt.resolvedUrl("pages/GamePage.qml"), { gameId: gameId })
    }

    function notify(summary, body, method, args) {
        if (!appSettings.notifications)
            return
        var n = notificationComponent.createObject(app, {
            summary: summary,
            body: body,
            previewSummary: summary,
            previewBody: body,
            remoteActions: [{
                "name": "default",
                "displayName": qsTr("Show"),
                "service": appActivation.serviceName,
                "path": appActivation.objectPath,
                "iface": appActivation.interfaceName,
                "method": method,
                "arguments": args
            }]
        })
        n.publish()
        notifications.push(n)
    }

    // Published notifications; their actions need the running app, so they
    // are withdrawn when it quits.
    property var notifications: []

    Component.onDestruction: {
        for (var i = 0; i < notifications.length; ++i)
            notifications[i].close()
    }

    DisplayBlanking {
        preventBlanking: app.boardVisible && appSettings.keepScreenOn
                         && Qt.application.state === Qt.ApplicationActive
    }

    Component {
        id: notificationComponent
        Notification {
            id: note
            appName: "salichess"
            appIcon: "harbour-salichess"
            onClosed: {
                var i = app.notifications.indexOf(note)
                if (i >= 0)
                    app.notifications.splice(i, 1)
            }
        }
    }

    Connections {
        target: challenges
        onNewIncomingChallenge: {
            if (Qt.application.state !== Qt.ApplicationActive)
                app.notify(qsTr("Challenge from %1").arg(opponentName), description, "openChallenges", [])
            else
                app.notice(qsTr("%1 challenges you").arg(opponentName))
        }
        onChallengeAccepted: app.openGame(gameId)
        onErrorOccurred: app.notice(message)
    }

    // Outcomes of challenges sent from the app, wherever the user is now.
    Connections {
        target: outgoingChallenges

        onChallengeAccepted: {
            var page = pageStack.currentPage
            if (page && page.challenge && page.challenge.challengeId === gameId)
                pageStack.replace(Qt.resolvedUrl("pages/GamePage.qml"), { gameId: gameId })
            else
                app.openGame(gameId)
            if (Qt.application.state !== Qt.ApplicationActive)
                app.notify(qsTr("Challenge accepted"), qsTr("%1 accepted your challenge. The game has started.").arg(opponent),
                           "openGame", [gameId])
        }
        onChallengeDeclined: {
            var page = pageStack.currentPage
            var shown = page && page.challenge && page.challenge.challengeId === challengeId
            var text = qsTr("%1 declined your challenge").arg(opponent) + (reason ? ": " + reason : "")
            if (Qt.application.state !== Qt.ApplicationActive)
                app.notify(qsTr("Challenge declined"), text, "activate", [])
            else if (!shown)
                app.notice(text)
        }
        onChallengeFailed: {
            var page = pageStack.currentPage
            if (!(page && page.challenge && page.challenge.opponent === opponent))
                app.notice(qsTr("Challenge to %1 failed: %2").arg(opponent).arg(error))
        }
    }

    Connections {
        target: ongoingGames
        onMyTurn: {
            if (app.activeGame && app.activeGame.gameId === gameId
                    && Qt.application.state === Qt.ApplicationActive)
                return
            app.notify(qsTr("Your turn"), qsTr("Your move against %1").arg(opponentName), "openGame", [gameId])
        }
    }

    Connections {
        target: session
        onLoginFailed: app.notice(error)
    }

    Connections {
        target: appActivation
        onActivateRequested: app.activate()
        onOpenGameRequested: {
            app.activate()
            app.openGame(gameId)
        }
        onOpenChallengesRequested: {
            app.activate()
            pageStack.pop(null, PageStackAction.Immediate)
        }
    }
}
