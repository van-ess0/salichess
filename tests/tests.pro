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

# Stockfish, so the engine wrapper can be tested on the host too.
include($$PWD/../3rdparty/stockfish.pri)

HEADERS += \
    fakelichess.h \
    testdata.h \
    $$SRC/chess/chessgame.h \
    $$SRC/chess/chessposition.h \
    $$SRC/chess/hotseatcontroller.h \
    $$SRC/chess/movetree.h \
    $$SRC/chess/piecesmodel.h \
    $$SRC/core/appsettings.h \
    $$SRC/engine/enginecontroller.h \
    $$SRC/engine/nnueweights.h \
    $$SRC/engine/stockfishengine.h \
    $$SRC/core/lichessapi.h \
    $$SRC/core/ndjsonstream.h \
    $$SRC/core/services.h \
    $$SRC/core/session.h \
    $$SRC/core/tokenstore.h \
    $$SRC/lichess/challengesmodel.h \
    $$SRC/lichess/chatmodel.h \
    $$SRC/lichess/eventstream.h \
    $$SRC/lichess/friendsmodel.h \
    $$SRC/lichess/gameanalysis.h \
    $$SRC/lichess/gamecontroller.h \
    $$SRC/lichess/gameinfo.h \
    $$SRC/lichess/gameshistorymodel.h \
    $$SRC/lichess/lobbyseek.h \
    $$SRC/lichess/ongoinggamesmodel.h \
    $$SRC/lichess/outgoingchallenge.h \
    $$SRC/lichess/outgoingchallenges.h \
    $$SRC/lichess/puzzlecontroller.h \
    $$SRC/lichess/puzzlestore.h \
    $$SRC/lichess/puzzlelogic.h

SOURCES += \
    main.cpp \
    fakelichess.cpp \
    tst_chess.cpp \
    tst_engine.cpp \
    tst_lichess.cpp \
    tst_live.cpp \
    $$SRC/chess/chessgame.cpp \
    $$SRC/chess/chessposition.cpp \
    $$SRC/chess/hotseatcontroller.cpp \
    $$SRC/chess/movetree.cpp \
    $$SRC/chess/piecesmodel.cpp \
    $$SRC/core/appsettings.cpp \
    $$SRC/engine/enginecontroller.cpp \
    $$SRC/engine/nnueweights.cpp \
    $$SRC/engine/stockfishengine.cpp \
    $$SRC/core/lichessapi.cpp \
    $$SRC/core/ndjsonstream.cpp \
    $$SRC/core/services.cpp \
    $$SRC/core/session.cpp \
    $$SRC/lichess/challengesmodel.cpp \
    $$SRC/lichess/chatmodel.cpp \
    $$SRC/lichess/eventstream.cpp \
    $$SRC/lichess/friendsmodel.cpp \
    $$SRC/lichess/gameanalysis.cpp \
    $$SRC/lichess/gamecontroller.cpp \
    $$SRC/lichess/gameinfo.cpp \
    $$SRC/lichess/gameshistorymodel.cpp \
    $$SRC/lichess/lobbyseek.cpp \
    $$SRC/lichess/ongoinggamesmodel.cpp \
    $$SRC/lichess/outgoingchallenge.cpp \
    $$SRC/lichess/outgoingchallenges.cpp \
    $$SRC/lichess/puzzlecontroller.cpp \
    $$SRC/lichess/puzzlestore.cpp \
    $$SRC/lichess/puzzlelogic.cpp
