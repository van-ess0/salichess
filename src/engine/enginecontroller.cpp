// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "enginecontroller.h"

#include "chess/chessgame.h"
#include "chess/chessposition.h"
#include "core/appsettings.h"
#include "nnueweights.h"
#include "stockfishengine.h"

#include <QThread>
#include <QtMath>

namespace {

// The position on the board settles before the engine is pointed at it:
// stepping through a game would otherwise start a search per move.
const int RestartDelayMs = 250;

// Longer lines are unreadable on a phone.
const int PvMovesShown = 8;

// The moves of a line in SAN, worked out by replaying them.
QString pvInSan(const QString &fen, const QString &pvUci, QString *firstMove)
{
    ChessPosition position(fen);
    QStringList san;
    const QStringList moves = pvUci.split(QLatin1Char(' '), QString::SkipEmptyParts);
    for (const QString &uci : moves) {
        const QString normalized = position.normalizeUci(uci);
        if (normalized.isEmpty())
            break;
        if (firstMove && firstMove->isEmpty())
            *firstMove = normalized;
        san.append(position.sanForUci(normalized));
        position.playUci(normalized);
        if (san.size() >= PvMovesShown)
            break;
    }
    return san.join(QLatin1Char(' '));
}

} // namespace

EngineController::EngineController(AppSettings *settings, NnueWeights *weights, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_weights(weights)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(RestartDelayMs);
    connect(&m_debounce, &QTimer::timeout, this, &EngineController::restart);

    connect(m_weights, &NnueWeights::readyChanged, this, [this]() {
        emit availableChanged();
        // Switching the engine on before the networks arrived leaves it
        // waiting for them; this is when it can finally start.
        if (available())
            restart();
        else
            stop();
    });

    connect(m_settings, &AppSettings::engineEnabledChanged, this, [this]() {
        emit enabledChanged();
        if (wantsEngine())
            restart();
        else
            stop();
    });
    connect(m_settings, &AppSettings::engineLinesChanged, this, &EngineController::applySettings);
    connect(m_settings, &AppSettings::engineDepthChanged, this, &EngineController::applySettings);
    connect(m_settings, &AppSettings::engineThreadsChanged, this, &EngineController::applySettings);
    connect(m_settings, &AppSettings::engineHashChanged, this, &EngineController::applySettings);
}

EngineController::~EngineController()
{
    if (m_thread) {
        // The engine must be taken down on its own thread, where it was made.
        QMetaObject::invokeMethod(m_engine, "unload", Qt::BlockingQueuedConnection);
        m_thread->quit();
        m_thread->wait();
    }
}

bool EngineController::enabled() const
{
    return m_settings->engineEnabled();
}

void EngineController::setEnabled(bool enabled)
{
    m_settings->setEngineEnabled(enabled);
}

bool EngineController::available() const
{
    return m_weights->ready();
}

void EngineController::enableWithDownload()
{
    setEnabled(true);
    if (!available())
        m_weights->download();
}

void EngineController::setGame(ChessGame *game)
{
    if (m_game == game)
        return;
    if (m_game)
        m_game->disconnect(this);
    m_game = game;
    if (m_game) {
        connect(m_game, &ChessGame::positionChanged, this, &EngineController::onPositionChanged);
        connect(m_game, &QObject::destroyed, this, [this]() { setGame(nullptr); });
    }
    emit gameChanged();
    clearLines();
    if (wantsEngine())
        onPositionChanged();
    else
        stop();
}

void EngineController::setApplicationActive(bool active)
{
    if (m_appActive == active)
        return;
    m_appActive = active;
    if (wantsEngine())
        restart();
    else
        stop();
}

bool EngineController::wantsEngine() const
{
    return m_game && enabled() && m_appActive;
}

bool EngineController::shouldSearch() const
{
    return wantsEngine() && available();
}

void EngineController::onPositionChanged()
{
    if (!wantsEngine()) {
        stop();
        return;
    }
    const QString fen = m_game->fen();
    if (fen == m_searchFen && m_searching)
        return;
    m_pendingFen = fen;
    // What is on screen belongs to the old position until the new lines
    // arrive; showing the old evaluation next to a new position would be a
    // lie, so it goes at once.
    clearLines();
    m_debounce.start();
}

void EngineController::restart()
{
    if (!wantsEngine()) {
        stop();
        return;
    }
    if (!available()) {
        // Switched on without the networks on the phone: fetch them. The
        // search follows when they arrive.
        if (!m_weights->downloading())
            m_weights->download();
        stop();
        return;
    }
    if (!m_thread) {
        m_thread = new QThread(this);
        m_thread->setObjectName(QStringLiteral("stockfish"));
        m_engine = new StockfishEngine;
        m_engine->moveToThread(m_thread);
        connect(m_thread, &QThread::finished, m_engine, &QObject::deleteLater);
        connect(m_engine, &StockfishEngine::info, this, &EngineController::onInfo);
        connect(m_engine, &StockfishEngine::failed, this, &EngineController::setError);
        connect(m_engine, &StockfishEngine::readyChanged, this, [this](bool ready) {
            m_engineReady = ready;
            if (ready)
                restart();
        });
        connect(m_engine, &StockfishEngine::searchFinished, this, [this]() {
            m_searching = false;
            emit stateChanged();
        });
        m_thread->start();
        applySettings();
    }

    if (!m_engineReady) {
        // Reading the networks takes a moment; the search follows once the
        // engine says it is ready.
        QMetaObject::invokeMethod(m_engine, "load", Qt::QueuedConnection,
                                  Q_ARG(QString, m_weights->bigPath()),
                                  Q_ARG(QString, m_weights->smallPath()));
        return;
    }

    const QString fen = m_pendingFen.isEmpty() ? m_game->fen() : m_pendingFen;
    m_searchFen = fen;
    m_whiteToMove = m_game->viewSideToMove() == QLatin1String("white");
    m_pendingFen.clear();
    m_depth = 0;
    m_searching = true;
    setError(QString());
    emit stateChanged();
    QMetaObject::invokeMethod(m_engine, "search", Qt::QueuedConnection,
                              Q_ARG(QString, fen), Q_ARG(QStringList, QStringList()),
                              Q_ARG(int, m_settings->engineDepth()));
}

void EngineController::stop()
{
    m_debounce.stop();
    m_pendingFen.clear();
    m_searchFen.clear();
    clearLines();
    if (!m_engine)
        return;
    QMetaObject::invokeMethod(m_engine, "stop", Qt::QueuedConnection);
    if (m_searching) {
        m_searching = false;
        emit stateChanged();
    }
}

void EngineController::applySettings()
{
    if (!m_engine)
        return;
    QMetaObject::invokeMethod(m_engine, "setThreads", Qt::QueuedConnection,
                              Q_ARG(int, m_settings->engineThreads()));
    QMetaObject::invokeMethod(m_engine, "setHashSize", Qt::QueuedConnection,
                              Q_ARG(int, m_settings->engineHash()));
    QMetaObject::invokeMethod(m_engine, "setMultiPv", Qt::QueuedConnection,
                              Q_ARG(int, m_settings->engineLines()));
    // Depth and the number of lines only take effect on the next search.
    if (m_searching)
        restart();
}

void EngineController::onInfo(int depth, int multiPv, int scoreCp, int mateIn, const QString &pvUci)
{
    // A line from the search that was running before the board moved on.
    if (m_searchFen.isEmpty() || !m_game || m_searchFen != m_game->fen())
        return;

    QString firstMove;
    QVariantMap line;
    line.insert(QStringLiteral("depth"), depth);
    // Stockfish scores from the side to move's point of view; everything
    // else in the app reads evaluations from white's.
    line.insert(QStringLiteral("cp"), m_whiteToMove ? scoreCp : -scoreCp);
    line.insert(QStringLiteral("mate"), m_whiteToMove ? mateIn : -mateIn);
    line.insert(QStringLiteral("pv"), pvInSan(m_searchFen, pvUci, &firstMove));
    line.insert(QStringLiteral("uci"), firstMove);

    const int index = multiPv - 1;
    while (m_lines.size() <= index)
        m_lines.append(QVariantMap());
    m_lines[index] = line;
    m_depth = qMax(m_depth, depth);
    emit linesChanged();
}

void EngineController::clearLines()
{
    if (m_lines.isEmpty() && m_depth == 0)
        return;
    m_lines.clear();
    m_depth = 0;
    emit linesChanged();
}

int EngineController::evalCp() const
{
    return m_lines.isEmpty() ? 0 : m_lines.first().toMap().value(QStringLiteral("cp")).toInt();
}

int EngineController::evalMate() const
{
    return m_lines.isEmpty() ? 0 : m_lines.first().toMap().value(QStringLiteral("mate")).toInt();
}

qreal EngineController::winPercent() const
{
    if (m_lines.isEmpty())
        return 50;
    const int mate = evalMate();
    if (mate != 0)
        return mate > 0 ? 100 : 0;
    // The same curve the evaluation graph is drawn with.
    const qreal win = 2 / (1 + qExp(-0.00368208 * evalCp())) - 1;
    return qBound(qreal(0), 50 + 50 * win, qreal(100));
}

int EngineController::bestMoveFrom() const
{
    if (m_lines.isEmpty())
        return -1;
    const QString uci = m_lines.first().toMap().value(QStringLiteral("uci")).toString();
    return uci.size() >= 4 ? ChessPosition::squareFromName(uci.mid(0, 2)) : -1;
}

int EngineController::bestMoveTo() const
{
    if (m_lines.isEmpty())
        return -1;
    const QString uci = m_lines.first().toMap().value(QStringLiteral("uci")).toString();
    return uci.size() >= 4 ? ChessPosition::squareFromName(uci.mid(2, 2)) : -1;
}

void EngineController::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit stateChanged();
}
