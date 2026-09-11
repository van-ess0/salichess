// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: page

    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: page.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("About")
            }

            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                source: "/usr/share/icons/hicolor/172x172/apps/harbour-salichess.png"
                width: Theme.iconSizeExtraLarge
                height: width
                sourceSize.width: width
                sourceSize.height: height
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "salichess " + Qt.application.version
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeLarge
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                color: Theme.secondaryHighlightColor
                text: qsTr("An unofficial Lichess client for Sailfish OS. It is not affiliated with or endorsed by lichess.org.")
            }

            SectionHeader {
                text: qsTr("License")
            }

            LinkedLabel {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                plainText: qsTr("salichess is free software, licensed under the GNU General Public License, version 3 or later: https://www.gnu.org/licenses/gpl-3.0.html\n\n"
                                + "Source code, bug reports and translations: https://github.com/van-ess0/salichess")
            }

            SectionHeader {
                text: qsTr("Credits")
            }

            LinkedLabel {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                plainText: qsTr("Lichess, the free and open-source chess server: https://lichess.org\n\n"
                                + "chess-library by Disservin (MIT license): https://github.com/Disservin/chess-library\n\n"
                                + "Chess pieces by Colin M.L. Burnett (GPLv2+), as used on Lichess. The app icon combines his knight with the Sailfish OS icon template shape.")
            }
        }

        VerticalScrollDecorator {}
    }
}
