// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import Amber.Web.Authorization 1.0

// "Log in with Lichess": OAuth2 authorization code flow with PKCE. The
// browser opens the Lichess consent page and redirects back to a local
// listener run by Amber.Web.Authorization.
Page {
    id: page

    property bool waiting: false

    allowedOrientations: Orientation.All

    OAuth2AcPkce {
        id: oauth
        clientId: session.oauthClientId
        scopes: session.oauthScopes
        authorizationEndpoint: session.oauthAuthorizationEndpoint
        tokenEndpoint: session.oauthTokenEndpoint
        timeout: 5 * 60

        // The browser has handed the code back: leave it behind and return
        // to the app.
        onReceivedAuthorizationCode: app.activate()
        onReceivedAccessToken: {
            page.waiting = false
            session.login(token["access_token"])
        }
        onErrorOccurred: {
            page.waiting = false
            app.notice(qsTr("Login failed: %1").arg(error.message))
        }
    }

    Connections {
        target: session
        onLoggedInChanged: {
            if (session.loggedIn)
                pageStack.pop()
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Log in")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                text: qsTr("salichess uses your Lichess account to play games and record puzzle results. Your password is never seen by this app: you log in on lichess.org in the browser and grant access there.")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("The app asks for permission to play games, send and accept challenges, read and record puzzles, and see who you follow. You can revoke access at any time in your Lichess account settings.")
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                enabled: !page.waiting && !session.busy
                text: qsTr("Log in with Lichess")
                onClicked: {
                    page.waiting = true
                    oauth.authorizeInBrowser()
                }
            }

            BusyLabel {
                running: page.waiting || session.busy
                text: session.busy ? qsTr("Logging in…") : qsTr("Waiting for authorization in the browser…")
            }

            Button {
                visible: page.waiting
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Cancel")
                onClicked: {
                    page.waiting = false
                    oauth.redirectListener.stopListening()
                }
            }
        }
    }
}
