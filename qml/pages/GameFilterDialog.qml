// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.salichess 1.0

// Narrows the games history down. The model is only changed when the dialog
// is accepted, so a cancelled dialog costs no request.
Dialog {
    id: dialog

    // The GamesHistoryModel being filtered.
    property QtObject model

    readonly property var perfs: ["", "ultraBullet", "bullet", "blitz", "rapid", "classical",
                                  "correspondence"]

    allowedOrientations: Orientation.All
    canAccept: true

    function perfName(perf) {
        switch (perf) {
        case "": return qsTr("Any")
        case "ultraBullet": return qsTr("UltraBullet")
        case "bullet": return qsTr("Bullet")
        case "blitz": return qsTr("Blitz")
        case "rapid": return qsTr("Rapid")
        case "classical": return qsTr("Classical")
        case "correspondence": return qsTr("Correspondence")
        }
        return perf
    }

    onAccepted: {
        model.perfType = dialog.perfs[perfBox.currentIndex]
        model.color = colorBox.currentIndex === 0 ? "" : (colorBox.currentIndex === 1 ? "white" : "black")
        model.rated = ratedBox.currentIndex === 0 ? GamesHistoryModel.AnyGames
                    : (ratedBox.currentIndex === 1 ? GamesHistoryModel.RatedOnly
                                                   : GamesHistoryModel.CasualOnly)
        model.analysedOnly = analysedSwitch.checked
        model.opponent = opponentField.text.trim()
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            width: parent.width

            DialogHeader {
                title: qsTr("Filter games")
                acceptText: qsTr("Apply")
            }

            ComboBox {
                id: perfBox
                label: qsTr("Time control")
                currentIndex: Math.max(0, dialog.perfs.indexOf(dialog.model.perfType))
                menu: ContextMenu {
                    Repeater {
                        model: dialog.perfs
                        MenuItem { text: dialog.perfName(modelData) }
                    }
                }
            }

            ComboBox {
                id: colorBox
                label: qsTr("Colour")
                currentIndex: dialog.model.color === "white" ? 1
                            : (dialog.model.color === "black" ? 2 : 0)
                menu: ContextMenu {
                    MenuItem { text: qsTr("Any") }
                    MenuItem { text: qsTr("White") }
                    MenuItem { text: qsTr("Black") }
                }
            }

            ComboBox {
                id: ratedBox
                label: qsTr("Kind")
                currentIndex: dialog.model.rated === GamesHistoryModel.RatedOnly ? 1
                            : (dialog.model.rated === GamesHistoryModel.CasualOnly ? 2 : 0)
                menu: ContextMenu {
                    MenuItem { text: qsTr("Any") }
                    MenuItem { text: qsTr("Rated") }
                    MenuItem { text: qsTr("Casual") }
                }
            }

            TextSwitch {
                id: analysedSwitch
                text: qsTr("Analysed games only")
                description: qsTr("Games Lichess has already looked at with a computer")
                checked: dialog.model.analysedOnly
            }

            TextField {
                id: opponentField
                width: parent.width
                label: qsTr("Opponent")
                placeholderText: qsTr("Any opponent")
                text: dialog.model.opponent
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                EnterKey.iconSource: "image://theme/icon-m-enter-close"
                EnterKey.onClicked: focus = false
            }
        }
    }
}
