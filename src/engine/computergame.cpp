// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "computergame.h"

#include "chess/chessgame.h"
#include "chess/chessposition.h"
#include "core/appsettings.h"
#include "core/services.h"
#include "lichess/gameinfo.h"
#include "nnueweights.h"
#include "stockfishengine.h"

#include <QDateTime>
#include <QThread>

#include <cstdlib>

namespace {

// Per level 1..8: Stockfish's skill level, the deepest it may search and the
// longest it may think. The weakest levels play at a glance; the strongest
// gets two seconds, which is a lot for a phone and still quick for a person.
struct Strength {
    int skill;
    int depth;
    int moveTimeMs;
};

const Strength Levels[] = {
    {0, 1, 50},
    {2, 2, 100},
    {4, 3, 150},
    {7, 5, 250},
    {10, 8, 400},
    {13, 11, 600},
    {17, 16, 1000},
    {20, 24, 2000},
};

const Strength &strength(int level)
{
    return Levels[qBound(1, level, 8) - 1];
}

QString other(const QString &color)
{
    return color == QLatin1String("white") ? QStringLiteral("black") : QStringLiteral("white");
}

} // namespace

int ComputerGame::skillForLevel(int level)
{
    return strength(level).skill;
}

int ComputerGame::depthForLevel(int level)
{
    return strength(level).depth;
}

int ComputerGame::moveTimeForLevel(int level)
{
    return strength(level).moveTimeMs;
}

ComputerGame::ComputerGame(QObject *parent)
    : QObject(parent)
    , m_game(new ChessGame(this))
    , m_weights(Services::weights())
{
    if (m_weights) {
        connect(m_weights, &NnueWeights::readyChanged, this, [this]() {
            emit engineAvailableChanged();
            // A game that was waiting for the networks goes on by itself.
            if (m_waiting && engineAvailable())
                requestEngineMove();
        });
    }
}

ComputerGame::~ComputerGame()
{
    if (m_thread) {
        // The engine must be taken down on its own thread, where it was made.
        QMetaObject::invokeMethod(m_engine, "unload", Qt::BlockingQueuedConnection);
        m_thread->quit();
        m_thread->wait();
    }
}

void ComputerGame::setLevel(int level)
{
    level = qBound(1, level, 8);
    if (m_level == level)
        return;
    m_level = level;
    if (m_engine)
        QMetaObject::invokeMethod(m_engine, "setSkillLevel", Qt::QueuedConnection,
                                  Q_ARG(int, skillForLevel(m_level)));
    emit levelChanged();
}

bool ComputerGame::engineAvailable() const
{
    return m_weights && m_weights->ready();
}

bool ComputerGame::isMyTurn() const
{
    return m_state == Playing && m_game->sideToMove() == m_playerColor;
}

QString ComputerGame::computerColor() const
{
    return other(m_playerColor);
}

QString ComputerGame::resultText() const
{
    if (m_state != Finished)
        return QString();
    const QString you = tr("You");
    const QString computer = tr("Stockfish");
    const bool white = m_playerColor == QLatin1String("white");
    return GameInfo::resultText(m_status, m_winner, white ? you : computer, white ? computer : you);
}

void ComputerGame::start(const QString &color)
{
    cancelRequest();
    if (color == QLatin1String("white") || color == QLatin1String("black")) {
        m_playerColor = color;
    } else {
        static bool seeded = false;
        if (!seeded) {
            qsrand(uint(QDateTime::currentMSecsSinceEpoch()) ^ uint(quintptr(QThread::currentThreadId())));
            seeded = true;
        }
        m_playerColor = qrand() % 2 ? QStringLiteral("white") : QStringLiteral("black");
    }
    m_game->reset();
    m_status.clear();
    m_winner.clear();
    m_error.clear();
    m_state = Playing;
    emit stateChanged();
    requestEngineMove();
}

void ComputerGame::move(const QString &uci)
{
    if (!isMyTurn() || thinking() || !m_game->atLatest())
        return;
    const QString played = m_game->position().normalizeUci(uci);
    if (played.isEmpty() || !m_game->playUci(played))
        return;
    m_error.clear();
    checkOutcome();
    if (m_state == Playing)
        requestEngineMove();
    emit stateChanged();
}

void ComputerGame::takeBack()
{
    if (m_state != Playing)
        return;
    const bool white = m_playerColor == QLatin1String("white");
    const bool myTurn = isMyTurn();
    // The reply goes with the move, so it is the player's turn again. The
    // first move of a game against white belongs to the computer alone.
    const int plies = myTurn ? 2 : 1;
    const int least = plies + (white ? 0 : 1);
    if (m_game->ply() < least)
        return;
    cancelRequest();
    for (int i = 0; i < plies; ++i)
        m_game->undo();
    m_game->viewLatest();
    m_error.clear();
    emit stateChanged();
}

void ComputerGame::resign()
{
    if (m_state != Playing)
        return;
    finish(QStringLiteral("resign"), computerColor());
}

void ComputerGame::retry()
{
    if (m_state != Playing || isMyTurn() || thinking())
        return;
    requestEngineMove();
}

void ComputerGame::downloadEngine()
{
    if (m_weights && !m_weights->ready() && !m_weights->downloading())
        m_weights->download();
}

void ComputerGame::ensureEngine()
{
    if (m_thread)
        return;
    m_thread = new QThread(this);
    m_thread->setObjectName(QStringLiteral("stockfish-play"));
    m_engine = new StockfishEngine;
    m_engine->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_engine, &QObject::deleteLater);
    connect(m_engine, &StockfishEngine::bestMove, this, &ComputerGame::onBestMove);
    connect(m_engine, &StockfishEngine::failed, this, &ComputerGame::onEngineFailed);
    connect(m_engine, &StockfishEngine::readyChanged, this, [this](bool ready) {
        m_engineReady = ready;
        m_loading = false;
        if (ready && m_waiting && engineAvailable())
            requestEngineMove();
    });
    m_thread->start();

    AppSettings *settings = Services::settings();
    QMetaObject::invokeMethod(m_engine, "setSkillLevel", Qt::QueuedConnection,
                              Q_ARG(int, skillForLevel(m_level)));
    QMetaObject::invokeMethod(m_engine, "setThreads", Qt::QueuedConnection,
                              Q_ARG(int, settings ? settings->engineThreads() : 1));
    // A move every few hundred milliseconds does not need a big table.
    QMetaObject::invokeMethod(m_engine, "setHashSize", Qt::QueuedConnection, Q_ARG(int, 16));
}

void ComputerGame::requestEngineMove()
{
    if (m_state != Playing || isMyTurn() || thinking())
        return;
    m_error.clear();

    // No networks, no engine: Stockfish would end the whole app. Wait, and
    // say so; downloading them (or leaving) is up to the player.
    if (!engineAvailable()) {
        m_waiting = true;
        emit stateChanged();
        return;
    }

    ensureEngine();
    if (!m_engineReady) {
        // Reading the networks takes a moment; the move follows when the
        // engine says it is ready.
        m_waiting = true;
        if (!m_loading) {
            m_loading = true;
            QMetaObject::invokeMethod(m_engine, "load", Qt::QueuedConnection,
                                      Q_ARG(QString, m_weights->bigPath()),
                                      Q_ARG(QString, m_weights->smallPath()));
        }
        emit stateChanged();
        return;
    }

    m_waiting = false;
    m_requestId = ++m_lastRequestId;
    QMetaObject::invokeMethod(m_engine, "play", Qt::QueuedConnection,
                              Q_ARG(QString, m_game->position().fen()), Q_ARG(QStringList, QStringList()),
                              Q_ARG(int, depthForLevel(m_level)), Q_ARG(int, moveTimeForLevel(m_level)),
                              Q_ARG(int, m_requestId));
    emit stateChanged();
}

void ComputerGame::onBestMove(int requestId, const QString &uci)
{
    // The answer for a position the game has left (a move taken back, a new
    // game) is of no use.
    if (requestId != m_requestId || m_requestId == 0 || m_state != Playing)
        return;
    m_requestId = 0;
    if (uci.isEmpty()) {
        checkOutcome();
        emit stateChanged();
        return;
    }
    const QString played = m_game->position().normalizeUci(uci);
    if (played.isEmpty() || !m_game->playUci(played)) {
        m_error = tr("The engine suggested a move that is not legal.");
        emit stateChanged();
        return;
    }
    checkOutcome();
    emit stateChanged();
}

void ComputerGame::onEngineFailed(const QString &error)
{
    if (m_state != Playing || (m_requestId == 0 && !m_loading && !m_waiting))
        return;
    m_requestId = 0;
    m_loading = false;
    m_waiting = false;
    setError(error);
}

void ComputerGame::cancelRequest()
{
    if (m_requestId != 0 && m_engine)
        QMetaObject::invokeMethod(m_engine, "stop", Qt::QueuedConnection);
    m_requestId = 0;
    m_waiting = false;
}

void ComputerGame::checkOutcome()
{
    const QString outcome = m_game->outcome();
    if (outcome.isEmpty())
        return;
    if (outcome == QLatin1String("checkmate"))
        // After the mating move it is the mated side's turn.
        finish(QStringLiteral("mate"), other(m_game->sideToMove()));
    else if (outcome == QLatin1String("stalemate"))
        finish(QStringLiteral("stalemate"), QString());
    else
        finish(QStringLiteral("draw"), QString());
}

void ComputerGame::finish(const QString &status, const QString &winner)
{
    cancelRequest();
    m_status = status;
    m_winner = winner;
    m_state = Finished;
    emit stateChanged();
}

void ComputerGame::setError(const QString &error)
{
    m_error = error;
    emit stateChanged();
}
