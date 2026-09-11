# SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
# SPDX-License-Identifier: GPL-3.0-or-later

# TARGET must match qml/<TARGET>.qml, the .desktop file, the icon and
# translation file names, and Name: in rpm/harbour-salichess.spec.
TARGET = harbour-salichess

# The RPM build passes VERSION from the spec (%qmake5 VERSION=%{version}).
isEmpty(VERSION): VERSION = 0.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

CONFIG += sailfishapp

QT += network svg dbus

# Sailfish Secrets keeps the Lichess access token. (No explicit
# link_pkgconfig: sailfishapp.prf adds it after its own PKGCONFIG entry.)
PKGCONFIG += sailfishsecrets
INCLUDEPATH += /usr/include/Sailfish

# chess-library (3rdparty/chess-library/chess.hpp) needs C++17.
QMAKE_CXXFLAGS += -std=gnu++17

INCLUDEPATH += src 3rdparty/chess-library

HEADERS += \
    src/core/appsettings.h \
    src/core/appactivation.h \
    src/core/lichessapi.h \
    src/core/ndjsonstream.h \
    src/core/pieceimageprovider.h \
    src/core/secretstokenstore.h \
    src/core/services.h \
    src/core/session.h \
    src/core/tokenstore.h \
    src/chess/chessgame.h \
    src/chess/chessposition.h \
    src/chess/piecesmodel.h \
    src/lichess/challengesmodel.h \
    src/lichess/chatmodel.h \
    src/lichess/eventstream.h \
    src/lichess/friendsmodel.h \
    src/lichess/gamecontroller.h \
    src/lichess/lobbyseek.h \
    src/lichess/ongoinggamesmodel.h \
    src/lichess/outgoingchallenge.h \
    src/lichess/outgoingchallenges.h \
    src/lichess/puzzlecontroller.h \
    src/lichess/puzzlelogic.h

SOURCES += \
    src/main.cpp \
    src/core/appsettings.cpp \
    src/core/appactivation.cpp \
    src/core/lichessapi.cpp \
    src/core/ndjsonstream.cpp \
    src/core/pieceimageprovider.cpp \
    src/core/secretstokenstore.cpp \
    src/core/services.cpp \
    src/core/session.cpp \
    src/chess/chessgame.cpp \
    src/chess/chessposition.cpp \
    src/chess/piecesmodel.cpp \
    src/lichess/challengesmodel.cpp \
    src/lichess/chatmodel.cpp \
    src/lichess/eventstream.cpp \
    src/lichess/friendsmodel.cpp \
    src/lichess/gamecontroller.cpp \
    src/lichess/lobbyseek.cpp \
    src/lichess/ongoinggamesmodel.cpp \
    src/lichess/outgoingchallenge.cpp \
    src/lichess/outgoingchallenges.cpp \
    src/lichess/puzzlecontroller.cpp \
    src/lichess/puzzlelogic.cpp

DISTFILES += \
    qml/harbour-salichess.qml \
    qml/cover/*.qml \
    qml/pages/*.qml \
    qml/components/*.qml \
    qml/js/*.js \
    rpm/harbour-salichess.changes \
    rpm/harbour-salichess.spec \
    translations/*.ts \
    harbour-salichess.desktop

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

# Regenerates translations/*.ts from qsTr()/tr() on every build.
CONFIG += sailfishapp_i18n

TRANSLATIONS += translations/harbour-salichess-de.ts

SUBDIRS += \
    tests/tests.pro
