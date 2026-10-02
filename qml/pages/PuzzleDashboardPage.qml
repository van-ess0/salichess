// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.salichess 1.0
import "../components"
import "../js/PuzzleThemes.js" as Themes
import "../js/Util.js" as Util

// How the user has done at puzzles over the last days: totals, the themes
// they are best and worst at, and the puzzles played last.
Page {
    id: page

    readonly property var dayOptions: [7, 30, 90, 365]

    allowedOrientations: Orientation.All

    PuzzleDashboard {
        id: dashboard
        days: 30
    }

    Component.onCompleted: dashboard.reload()

    function percent(part, whole) {
        return whole > 0 ? Math.round(100 * part / whole) : 0
    }

    function playTheme(theme) {
        pageStack.push(Qt.resolvedUrl("PuzzlePage.qml"), { angle: theme })
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: qsTr("Refresh")
                onClicked: dashboard.reload()
            }
        }

        Column {
            id: column
            width: page.width

            PageHeader {
                title: qsTr("Puzzle dashboard")
                description: session.loggedIn && session.puzzleRating > 0
                             ? qsTr("Rating %1").arg(session.puzzleRating) : ""
            }

            OfflineBanner {
                description: qsTr("The dashboard needs a connection")
            }

            ComboBox {
                label: qsTr("Period")
                currentIndex: Math.max(0, page.dayOptions.indexOf(dashboard.days))
                menu: ContextMenu {
                    Repeater {
                        model: page.dayOptions
                        MenuItem {
                            text: modelData === 365 ? qsTr("Last year") : qsTr("Last %n day(s)", "", modelData)
                            onClicked: dashboard.days = modelData
                        }
                    }
                }
            }

            BusyLabel {
                visible: running
                running: dashboard.loading
                text: qsTr("Loading…")
            }

            Label {
                visible: dashboard.errorString !== ""
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                color: Theme.errorColor
                text: dashboard.errorString
            }

            // --- Totals ---

            Item {
                visible: dashboard.puzzleCount > 0
                width: 1
                height: Theme.paddingMedium
            }

            Row {
                visible: dashboard.puzzleCount > 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x

                Repeater {
                    model: [
                        { "value": dashboard.puzzleCount, "label": qsTr("Played") },
                        { "value": page.percent(dashboard.firstWins, dashboard.puzzleCount) + "%", "label": qsTr("First try") },
                        { "value": dashboard.performance, "label": qsTr("Performance") }
                    ]
                    Column {
                        width: parent.width / 3
                        Label {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            color: Theme.highlightColor
                            font.pixelSize: Theme.fontSizeExtraLarge
                            font.family: Theme.fontFamilyHeading
                            text: modelData.value
                        }
                        Label {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: Theme.secondaryColor
                            text: modelData.label
                        }
                    }
                }
            }

            Item {
                visible: dashboard.puzzleCount > 0
                width: 1
                height: Theme.paddingLarge
            }

            Label {
                visible: !dashboard.loading && dashboard.errorString === "" && dashboard.puzzleCount === 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                color: Theme.secondaryHighlightColor
                text: qsTr("No puzzles played in this period")
            }

            // --- Strengths and weaknesses ---

            SectionHeader {
                visible: dashboard.strongest.length > 0
                text: qsTr("Strongest themes")
            }

            Repeater {
                model: dashboard.strongest
                PuzzleThemeRow {
                    width: column.width
                    theme: modelData
                    onClicked: page.playTheme(modelData.theme)
                }
            }

            SectionHeader {
                visible: dashboard.weakest.length > 0
                text: qsTr("Themes to practise")
            }

            Repeater {
                model: dashboard.weakest
                PuzzleThemeRow {
                    width: column.width
                    theme: modelData
                    onClicked: page.playTheme(modelData.theme)
                }
            }

            // --- Recent ---

            SectionHeader {
                visible: dashboard.recent.length > 0
                text: qsTr("Recent puzzles")
            }

            Repeater {
                model: dashboard.recent

                BackgroundItem {
                    id: recentItem
                    height: Theme.itemSizeSmall
                    onClicked: pageStack.push(Qt.resolvedUrl("PuzzlePage.qml"), { puzzleId: modelData.id })

                    Rectangle {
                        id: stripe
                        anchors {
                            left: parent.left
                            top: parent.top
                            bottom: parent.bottom
                            topMargin: Theme.paddingSmall
                            bottomMargin: Theme.paddingSmall
                        }
                        width: Theme.paddingSmall / 2
                        color: modelData.win ? "#629924" : "#df5353"
                    }

                    Column {
                        anchors {
                            left: parent.left
                            leftMargin: Theme.horizontalPageMargin
                            right: parent.right
                            rightMargin: Theme.horizontalPageMargin
                            verticalCenter: parent.verticalCenter
                        }
                        Label {
                            width: parent.width
                            truncationMode: TruncationMode.Fade
                            color: recentItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                            text: (modelData.win ? qsTr("Solved") : qsTr("Failed")) + " • " + qsTr("rating %1").arg(modelData.rating)
                        }
                        Label {
                            width: parent.width
                            truncationMode: TruncationMode.Fade
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: Theme.secondaryColor
                            text: [Util.formatGameDate(modelData.date)]
                                  .concat(modelData.themes.slice(0, 3).map(function(key) { return Themes.name(key) }))
                                  .join(" • ")
                        }
                    }
                }
            }

            // --- All themes ---

            SectionHeader {
                visible: dashboard.themes.length > 0
                text: qsTr("By theme")
            }

            Repeater {
                model: dashboard.themes
                PuzzleThemeRow {
                    width: column.width
                    theme: modelData
                    onClicked: page.playTheme(modelData.theme)
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
