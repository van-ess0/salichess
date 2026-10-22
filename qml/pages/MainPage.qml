// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0

// The start page: tabs along the top, swiped sideways like in the Settings app,
// whose TabView and TabBar are in Sailfish.Silica.private, which Harbour does not
// allow. So this is a SlideshowView, which comes round, of full-height tabs with a
// tab bar floating over them, which slides down with a tab that is pulled down and
// gets a background once the tab scrolls beneath it.
Page {
    id: page

    allowedOrientations: Orientation.All

    onStatusChanged: {
        if (status === PageStatus.Active && session.loggedIn)
            ongoingGames.refreshIfStale()
    }

    Component { id: playTab; MainPlayTab { topInset: tabBar.height } }
    Component { id: exploreTab; MainExploreTab { topInset: tabBar.height } }
    Component { id: meTab; MainMeTab { topInset: tabBar.height } }

    // Swiping on from the last tab comes round to the first
    SlideshowView {
        id: tabs
        anchors.fill: parent
        itemWidth: width
        itemHeight: height
        // The neighbouring tabs wait just outside the page, and show while a page slides
        clip: true
        model: 3

        delegate: Item {
            property Item tab: loader.item
            width: tabs.itemWidth
            height: tabs.itemHeight

            Loader {
                id: loader
                anchors.fill: parent
                sourceComponent: [playTab, exploreTab, meTab][index]
            }
        }
    }

    Item {
        id: tabBar
        readonly property real scrolled: tabs.currentItem && tabs.currentItem.tab ? tabs.currentItem.tab.contentY : 0
        // A notch in portrait pushes the tab names down, as in Silica's own TabBar
        readonly property real notch: page.orientation === Orientation.Portrait && Screen.topCutout.height > 0
                                      ? Math.max(0, Screen.topCutout.height - Theme.paddingLarge) : 0
        width: parent.width
        height: Theme.itemSizeMedium + notch
        y: Math.max(0, -scrolled)
        z: 1

        Rectangle {
            anchors.fill: parent
            color: Theme.overlayBackgroundColor
            opacity: Math.max(0, Math.min(1, tabBar.scrolled / Theme.paddingLarge))
        }

        Row {
            y: tabBar.notch
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.paddingLarge

            Repeater {
                model: [qsTr("Play"), qsTr("Explore"), qsTr("Me")]

                // No background and no press highlight, as in the Settings app: the
                // name of the tab being shown, or pressed, takes the highlight colour.
                MouseArea {
                    id: tabItem
                    readonly property bool current: tabs.currentIndex === index
                    width: tabTitle.width + 2 * Theme.paddingLarge
                    height: Theme.itemSizeMedium
                    onClicked: tabs.currentIndex = index

                    Label {
                        id: tabTitle
                        anchors.centerIn: parent
                        text: modelData
                        font.pixelSize: Theme.fontSizeLarge
                        color: tabItem.current || tabItem.pressed ? Theme.highlightColor : Theme.primaryColor
                    }
                    Rectangle {
                        visible: tabItem.current
                        anchors {
                            horizontalCenter: parent.horizontalCenter
                            top: tabTitle.bottom
                            topMargin: Theme.paddingSmall
                        }
                        width: tabTitle.width
                        height: Math.round(Theme.paddingSmall / 2)
                        color: Theme.highlightColor
                    }
                }
            }
        }
    }
}
