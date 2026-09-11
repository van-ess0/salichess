// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.salichess 1.0

// Shows one outgoing challenge. Either pass |username| and |options| to send
// a new challenge, or |challenge| to look at one that is already pending.
// Leaving the page keeps the challenge alive; the app opens the game when it
// is accepted (see harbour-salichess.qml).
Page {
    id: page

    property string username
    property var options
    property QtObject challenge: null

    readonly property int challengeState: challenge ? challenge.state : OutgoingChallenge.Idle
    readonly property bool waiting: challengeState === OutgoingChallenge.Creating
                                    || challengeState === OutgoingChallenge.Waiting
    readonly property string opponent: challenge ? challenge.opponent : username

    allowedOrientations: Orientation.All

    // Not on creation: a dialog creates this page before the user accepts.
    onStatusChanged: {
        if (status === PageStatus.Active && !challenge && username !== "")
            challenge = outgoingChallenges.create(username, options || {})
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        PullDownMenu {
            visible: page.challenge !== null && page.challenge.url !== ""
            MenuItem {
                text: qsTr("Copy challenge link")
                onClicked: {
                    Clipboard.text = page.challenge.url
                    app.notice(qsTr("Link copied"))
                }
            }
        }

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Challenge")
                description: page.opponent
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                size: BusyIndicatorSize.Large
                running: page.waiting
                visible: running
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeLarge
                color: Theme.highlightColor
                text: {
                    switch (page.challengeState) {
                    case OutgoingChallenge.Creating:
                        return qsTr("Sending challenge…")
                    case OutgoingChallenge.Waiting:
                        return qsTr("Waiting for %1 to accept").arg(page.opponent)
                    case OutgoingChallenge.Accepted:
                        return qsTr("%1 accepted the challenge").arg(page.opponent)
                    case OutgoingChallenge.Declined:
                        return qsTr("%1 declined the challenge").arg(page.opponent)
                    case OutgoingChallenge.Canceled:
                        return qsTr("The challenge was canceled")
                    case OutgoingChallenge.Failed:
                        return qsTr("The challenge could not be sent")
                    default:
                        return ""
                    }
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
                visible: text !== ""
                text: {
                    if (!page.challenge)
                        return ""
                    if (page.challengeState === OutgoingChallenge.Declined)
                        return page.challenge.declineReason
                    if (page.challengeState === OutgoingChallenge.Failed)
                        return page.challenge.errorString
                    if (page.challengeState === OutgoingChallenge.Waiting)
                        return qsTr("Your friend can accept it in the Lichess app, on lichess.org, or by opening the challenge link.")
                    return ""
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                visible: page.waiting
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
                text: qsTr("You can leave this page: the challenge stays open while salichess runs, and the game opens as soon as it is accepted.")
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: page.waiting ? qsTr("Cancel challenge") : qsTr("Back")
                onClicked: {
                    if (page.waiting)
                        page.challenge.cancel()
                    else
                        pageStack.pop()
                }
            }
        }
    }
}
