// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components"
import "../js/PuzzleThemes.js" as Themes

// Puzzle hub: daily puzzle, healthy mix, difficulty and themes.
Page {
    id: page

    allowedOrientations: Orientation.All

    function start(angle) {
        pageStack.push(Qt.resolvedUrl("PuzzlePage.qml"), { angle: angle })
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: page.width

            PageHeader {
                title: qsTr("Puzzles")
                description: session.loggedIn && session.puzzleRating > 0
                             ? qsTr("Rating %1").arg(session.puzzleRating)
                             : (session.loggedIn ? "" : qsTr("Log in to record your progress"))
            }

            OfflineBanner {
                description: puzzleStore.count > 0
                             ? qsTr("%n puzzle(s) are ready to play offline", "", puzzleStore.count)
                             : qsTr("Keep puzzles on the phone in the settings")
            }

            BackgroundItem {
                height: Theme.itemSizeMedium
                onClicked: page.start("mix")
                MenuEntry {
                    icon: "image://theme/icon-m-wizard"
                    title: Themes.name("mix")
                    subtitle: puzzleStore.count > 0
                              ? qsTr("A bit of everything • %n stored", "", puzzleStore.count)
                              : qsTr("A bit of everything")
                }
            }

            BackgroundItem {
                height: Theme.itemSizeMedium
                onClicked: pageStack.push(Qt.resolvedUrl("PuzzlePage.qml"), { daily: true })
                MenuEntry {
                    icon: "image://theme/icon-m-date"
                    title: qsTr("Daily puzzle")
                    subtitle: qsTr("The Lichess puzzle of the day")
                }
            }

            ComboBox {
                id: difficultyBox
                label: qsTr("Difficulty")
                description: qsTr("Relative to your puzzle rating")
                currentIndex: Math.max(0, Themes.difficultyKeys().indexOf(appSettings.puzzleDifficulty))
                menu: ContextMenu {
                    Repeater {
                        model: Themes.difficultyKeys()
                        MenuItem {
                            text: Themes.difficultyName(modelData)
                            onClicked: appSettings.puzzleDifficulty = modelData
                        }
                    }
                }
            }

            Repeater {
                model: Themes.categories()

                Column {
                    width: column.width

                    SectionHeader {
                        text: modelData.title
                    }

                    Repeater {
                        model: modelData.themes
                        BackgroundItem {
                            id: themeItem
                            height: Theme.itemSizeExtraSmall
                            onClicked: page.start(modelData)
                            Label {
                                x: Theme.horizontalPageMargin
                                width: parent.width - 2 * x
                                anchors.verticalCenter: parent.verticalCenter
                                truncationMode: TruncationMode.Fade
                                text: Themes.name(modelData)
                                color: themeItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                            }
                        }
                    }
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
