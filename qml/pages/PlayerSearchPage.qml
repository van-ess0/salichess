// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.salichess 1.0
import "../components"
import "../js/Util.js" as Util

// Finds a Lichess player by the start of their name.
Page {
    id: page

    // What was typed.
    property string query

    allowedOrientations: Orientation.All

    PlayerSearch {
        id: search
        term: page.query
    }

    // The field is outside the list: in the list's header it lost the focus,
    // and so the keyboard, every time the results changed.
    Column {
        id: searchColumn
        width: page.width

        PageHeader {
            title: qsTr("Find a player")
        }

        OfflineBanner {
            description: qsTr("Searching needs a connection")
        }

        SearchField {
            id: field
            width: parent.width
            placeholderText: qsTr("Username")
            inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
            EnterKey.iconSource: "image://theme/icon-m-enter-close"
            EnterKey.onClicked: focus = false
            onTextChanged: page.query = text
            Component.onCompleted: forceActiveFocus()
        }
    }

    SilicaListView {
        id: list
        anchors {
            top: searchColumn.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        clip: true
        model: search.results

        delegate: BackgroundItem {
            id: item
            height: Theme.itemSizeSmall
            onClicked: pageStack.push(Qt.resolvedUrl("ProfilePage.qml"), { username: modelData.id })

            Rectangle {
                id: dot
                x: Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.paddingMedium
                height: width
                radius: width / 2
                color: modelData.online ? Theme.presenceColor(Theme.PresenceAvailable)
                                        : Theme.presenceColor(Theme.PresenceOffline)
            }

            Label {
                anchors {
                    left: dot.right
                    leftMargin: Theme.paddingMedium
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                truncationMode: TruncationMode.Fade
                text: Util.playerName(modelData.name, modelData.title)
                color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0
            text: {
                if (search.errorString !== "")
                    return search.errorString
                if (search.term.length < 2)
                    return qsTr("Type a name")
                return search.loading ? "" : qsTr("No player found")
            }
            hintText: search.term.length < 2 ? qsTr("Two letters are enough") : ""
        }

        BusyIndicator {
            anchors.centerIn: parent
            size: BusyIndicatorSize.Large
            running: search.loading && list.count === 0
        }

        VerticalScrollDecorator {}
    }
}
