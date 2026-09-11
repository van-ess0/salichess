// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../js/Util.js" as Util

// Player chat of a game.
Page {
    id: page

    property QtObject controller

    allowedOrientations: Orientation.All

    SilicaListView {
        id: list
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            bottom: input.top
        }
        clip: true
        model: page.controller ? page.controller.chat : null

        header: PageHeader {
            title: qsTr("Chat")
        }

        onCountChanged: positionViewAtEnd()
        Component.onCompleted: positionViewAtEnd()

        delegate: Item {
            width: list.width
            height: bubble.height + (timeLabel.visible ? timeLabel.height : 0) + Theme.paddingMedium

            Label {
                id: bubble
                x: model.mine ? parent.width - width - Theme.horizontalPageMargin : Theme.horizontalPageMargin
                width: Math.min(implicitWidth, parent.width * 0.8)
                wrapMode: Text.Wrap
                horizontalAlignment: model.mine ? Text.AlignRight : Text.AlignLeft
                color: model.system ? Theme.secondaryColor
                                    : (model.mine ? Theme.highlightColor : Theme.primaryColor)
                font.italic: model.system
                // Chat text comes from other players: escape it so it cannot inject markup.
                text: (model.mine || model.system ? "" : "<b>" + Util.escapeHtml(model.username) + "</b>: ")
                      + Util.escapeHtml(model.text)
                textFormat: Text.StyledText
            }

            // Lichess sends no timestamps: this is when the line arrived, so
            // lines from the history (loaded when the game opens) have none.
            Label {
                id: timeLabel
                visible: model.time !== undefined
                anchors {
                    top: bubble.bottom
                    left: model.mine ? undefined : bubble.left
                    right: model.mine ? bubble.right : undefined
                }
                font.pixelSize: Theme.fontSizeTiny
                color: Theme.secondaryColor
                text: visible ? Format.formatDate(model.time, Formatter.TimeValue) : ""
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0
            text: qsTr("No messages yet")
            hintText: qsTr("Say hello to your opponent")
        }

        VerticalScrollDecorator {}
    }

    TextField {
        id: input
        anchors.bottom: parent.bottom
        width: parent.width
        placeholderText: qsTr("Message")
        label: qsTr("Message to your opponent")
        EnterKey.enabled: text.trim().length > 0
        EnterKey.iconSource: "image://theme/icon-m-enter-accept"
        EnterKey.onClicked: {
            page.controller.sendChat(text)
            text = ""
        }
    }
}
