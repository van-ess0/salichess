# SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Host-side tests for everything below the QML layer. Not part of the RPM.
# Needs a desktop Qt 5 (qmake-qt5); network code runs against a local fake
# Lichess server, so no internet is needed:
#   mkdir build-tests && cd build-tests && qmake-qt5 ../tests && make && ./tst_salichess
# SALICHESS_NETWORK_TESTS=1 additionally runs a few tests against lichess.org.

TEMPLATE = app
TARGET = tst_salichess
QT += testlib network qml
QT -= gui
CONFIG += console testcase
CONFIG -= app_bundle
QMAKE_CXXFLAGS += -std=gnu++17

SRC = $$PWD/../src
INCLUDEPATH += $$SRC $$PWD/../3rdparty/chess-library

HEADERS += \
    fakelichess.h \
    testdata.h \
    $$SRC/chess/chessgame.h \
    $$SRC/chess/chessposition.h \
    $$SRC/chess/piecesmodel.h \
    $$SRC/core/appsettings.h \
    $$SRC/core/lichessapi.h \
    $$SRC/core/ndjsonstream.h \
    $$SRC/core/services.h \
    $$SRC/core/session.h \
    $$SRC/core/tokenstore.h \
    $$SRC/lichess/challengesmodel.h \
    $$SRC/lichess/chatmodel.h \
    $$SRC/lichess/eventstream.h \
    $$SRC/lichess/friendsmodel.h \
    $$SRC/lichess/gamecontroller.h \
    $$SRC/lichess/ongoinggamesmodel.h \
    $$SRC/lichess/outgoingchallenge.h \
    $$SRC/lichess/outgoingchallenges.h \
    $$SRC/lichess/puzzlecontroller.h \
    $$SRC/lichess/puzzlelogic.h

SOURCES += \
    main.cpp \
    fakelichess.cpp \
    tst_chess.cpp \
    tst_lichess.cpp \
    tst_live.cpp \
    $$SRC/chess/chessgame.cpp \
    $$SRC/chess/chessposition.cpp \
    $$SRC/chess/piecesmodel.cpp \
    $$SRC/core/appsettings.cpp \
    $$SRC/core/lichessapi.cpp \
    $$SRC/core/ndjsonstream.cpp \
    $$SRC/core/services.cpp \
    $$SRC/core/session.cpp \
    $$SRC/lichess/challengesmodel.cpp \
    $$SRC/lichess/chatmodel.cpp \
    $$SRC/lichess/eventstream.cpp \
    $$SRC/lichess/friendsmodel.cpp \
    $$SRC/lichess/gamecontroller.cpp \
    $$SRC/lichess/ongoinggamesmodel.cpp \
    $$SRC/lichess/outgoingchallenge.cpp \
    $$SRC/lichess/outgoingchallenges.cpp \
    $$SRC/lichess/puzzlecontroller.cpp \
    $$SRC/lichess/puzzlelogic.cpp
