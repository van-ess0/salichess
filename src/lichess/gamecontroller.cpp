// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gamecontroller.h"

#include "chatmodel.h"
#include "chess/chessgame.h"
#include "core/lichessapi.h"
#include "core/ndjsonstream.h"
#include "core/services.h"
#include "core/session.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>

namespace {

const int IdleTimeoutMs = 30 * 1000;
const int MaxBackoffMs = 30 * 1000;

QVariantMap playerInfo(const QJsonObject &player)
{
    QVariantMap info;
    info.insert(QStringLiteral("id"), player.value(QStringLiteral("id")).toString());
    QString name = player.value(QStringLiteral("name")).toString();
    const int aiLevel = player.value(QStringLiteral("aiLevel")).toInt();
    if (name.isEmpty() && aiLevel > 0)
        name = GameController::tr("Stockfish level %1").arg(aiLevel);
    else if (name.isEmpty())
        name = GameController::tr("Anonymous");
    info.insert(QStringLiteral("name"), name);
    info.insert(QStringLiteral("rating"), player.value(QStringLiteral("rating")).toInt());
    info.insert(QStringLiteral("title"), player.value(QStringLiteral("title")).toString());
    info.insert(QStringLiteral("provisional"), player.value(QStringLiteral("provisional")).toBool());
    return info;
}

QStringList splitMoves(const QString &moves)
{
    static const QRegularExpression separator(QStringLiteral("\\s+"));
    return moves.split(separator, QString::SkipEmptyParts);
}

} // namespace

GameController::GameController(QObject *parent)
    : QObject(parent)
    , m_game(new ChessGame(this))
    , m_chat(new ChatModel(this))
{
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &GameController::connectStream);

    connect(&m_clockTimer, &QTimer::timeout, this, &GameController::clockChanged);

    connect(m_game, &ChessGame::movesChanged, this, &GameController::stateChanged);
}

GameController::~GameController()
{
    closeStream();
}

void GameController::setGameId(const QString &id)
{
    if (m_gameId == id)
        return;
    closeStream();
    m_gameId = id;
    m_chatLoaded = false;
    m_chat->clear();
    m_serverMoves.clear();
    m_pendingMove.clear();
    m_status.clear();
    emit gameIdChanged();
    if (!m_gameId.isEmpty()) {
        m_failures = 0;
        setLoading(true);
        connectStream();
    }
}

int GameController::whiteTime() const
{
    if (runningClock() == QLatin1String("white"))
        return int(qMax<qint64>(0, m_whiteTime - m_clockStamp.elapsed()));
    return int(m_whiteTime);
}

int GameController::blackTime() const
{
    if (runningClock() == QLatin1String("black"))
        return int(qMax<qint64>(0, m_blackTime - m_clockStamp.elapsed()));
    return int(m_blackTime);
}

QString GameController::runningClock() const
{
    // Lichess starts the clocks once both players have moved.
    if ((!m_hasClock && !hasTurnTimer()) || gameOver() || m_game->ply() < 2 || !m_clockStamp.isValid())
        return QString();
    return whiteToMove() ? QStringLiteral("white") : QStringLiteral("black");
}

bool GameController::gameOver() const
{
    return !m_status.isEmpty() && m_status != QLatin1String("created")
            && m_status != QLatin1String("started");
}

QString GameController::resultText() const
{
    if (!gameOver())
        return QString();

    const QString whiteName = m_white.value(QStringLiteral("name")).toString();
    const QString blackName = m_black.value(QStringLiteral("name")).toString();
    const QString loser = m_winner == QLatin1String("white") ? blackName : whiteName;
    QString verdict;
    if (m_winner == QLatin1String("white"))
        verdict = tr("White is victorious");
    else if (m_winner == QLatin1String("black"))
        verdict = tr("Black is victorious");

    QString reason;
    if (m_status == QLatin1String("mate"))
        reason = tr("Checkmate");
    else if (m_status == QLatin1String("resign"))
        reason = tr("%1 resigned").arg(loser);
    else if (m_status == QLatin1String("stalemate"))
        reason = tr("Stalemate");
    else if (m_status == QLatin1String("timeout"))
        reason = tr("%1 left the game").arg(loser);
    else if (m_status == QLatin1String("draw"))
        reason = tr("Draw");
    else if (m_status == QLatin1String("outoftime"))
        reason = m_winner.isEmpty() ? tr("Time out") : tr("%1 ran out of time").arg(loser);
    else if (m_status == QLatin1String("aborted"))
        reason = tr("Game aborted");
    else if (m_status == QLatin1String("noStart"))
        reason = tr("%1 didn't move").arg(loser);
    else if (m_status == QLatin1String("cheat"))
        reason = tr("Cheat detected");
    else if (m_status == QLatin1String("insufficientMaterialClaim"))
        reason = tr("Insufficient material");
    else
        reason = tr("Game over");

    if (verdict.isEmpty() && m_status != QLatin1String("aborted"))
        verdict = tr("Draw");
    if (verdict.isEmpty() || reason == verdict)
        return reason;
    return reason + QStringLiteral(" • ") + verdict;
}

bool GameController::isMyTurn() const
{
    if (m_myColor.isEmpty() || gameOver() || m_status.isEmpty())
        return false;
    return (m_myColor == QLatin1String("white")) == whiteToMove();
}

bool GameController::canAbort() const
{
    return !m_myColor.isEmpty() && !gameOver() && m_game->ply() < 2;
}

bool GameController::canTakeback() const
{
    return !m_myColor.isEmpty() && !gameOver() && m_game->ply() > 0;
}

bool GameController::opponentOffersDraw() const
{
    return m_myColor == QLatin1String("white") ? m_bdraw : (m_myColor == QLatin1String("black") && m_wdraw);
}

bool GameController::opponentProposesTakeback() const
{
    return m_myColor == QLatin1String("white") ? m_btakeback : (m_myColor == QLatin1String("black") && m_wtakeback);
}

bool GameController::iOfferDraw() const
{
    return m_myColor == QLatin1String("white") ? m_wdraw : (m_myColor == QLatin1String("black") && m_bdraw);
}

bool GameController::iProposeTakeback() const
{
    return m_myColor == QLatin1String("white") ? m_wtakeback : (m_myColor == QLatin1String("black") && m_btakeback);
}

void GameController::move(const QString &uci)
{
    if (!isMyTurn() || !m_pendingMove.isEmpty() || !m_game->atLatest())
        return;
    const QString played = m_game->position().normalizeUci(uci);
    if (played.isEmpty())
        return;

    // Charge the thinking time to my clock before handing the move over.
    if (!runningClock().isEmpty()) {
        const qint64 elapsed = m_clockStamp.elapsed();
        if (whiteToMove())
            m_whiteTime = qMax<qint64>(0, m_whiteTime - elapsed);
        else
            m_blackTime = qMax<qint64>(0, m_blackTime - elapsed);
    }

    m_game->playUci(played);
    m_pendingMove = played;
    m_clockStamp.restart();
    updateClockTimer();
    emit clockChanged();

    Services::api()->postForm(QStringLiteral("/api/board/game/%1/move/%2").arg(m_gameId, m_pendingMove),
                              QUrlQuery(), this, [this](const ApiResult &result) {
        if (result.ok())
            return;
        // Roll back to what the server last told us.
        m_pendingMove.clear();
        m_game->setUciMoves(m_serverMoves);
        emit moveRejected(result.errorString);
    });
}

void GameController::resign()
{
    post(QStringLiteral("resign"));
}

void GameController::abort()
{
    post(QStringLiteral("abort"));
}

void GameController::offerDraw()
{
    post(QStringLiteral("draw/yes"));
}

void GameController::answerDraw(bool accept)
{
    post(accept ? QStringLiteral("draw/yes") : QStringLiteral("draw/no"));
}

void GameController::proposeTakeback()
{
    post(QStringLiteral("takeback/yes"));
}

void GameController::answerTakeback(bool accept)
{
    post(accept ? QStringLiteral("takeback/yes") : QStringLiteral("takeback/no"));
}

void GameController::claimVictory()
{
    post(QStringLiteral("claim-victory"));
}

void GameController::sendChat(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return;
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("room"), QStringLiteral("player"));
    form.addQueryItem(QStringLiteral("text"), trimmed.left(140));
    post(QStringLiteral("chat"), form);
}

void GameController::reconnect()
{
    if (m_gameId.isEmpty())
        return;
    closeStream();
    m_failures = 0;
    connectStream();
}

void GameController::connectStream()
{
    if (m_gameId.isEmpty() || m_stream || !Services::api())
        return;
    m_stream = Services::api()->openStream(QStringLiteral("/api/board/game/stream/%1").arg(m_gameId),
                                           QUrlQuery(), IdleTimeoutMs);
    m_stream->setParent(this);
    connect(m_stream.data(), &NdjsonStream::opened, this, [this]() {
        m_failures = 0;
        setConnected(true);
        setError(QString());
    });
    connect(m_stream.data(), &NdjsonStream::message, this, &GameController::onMessage);
    connect(m_stream.data(), &NdjsonStream::finished, this, &GameController::onStreamFinished);
}

void GameController::closeStream()
{
    m_reconnectTimer.stop();
    if (m_stream) {
        m_stream->abort();
        m_stream->deleteLater();
        m_stream.clear();
    }
    setConnected(false);
}

void GameController::onMessage(const QJsonObject &message)
{
    const QString type = message.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("gameFull")) {
        applyGameFull(message);
    } else if (type == QLatin1String("gameState")) {
        applyState(message);
    } else if (type == QLatin1String("chatLine")) {
        if (message.value(QStringLiteral("room")).toString() == QLatin1String("player"))
            m_chat->append(message.value(QStringLiteral("username")).toString(),
                           message.value(QStringLiteral("text")).toString());
    } else if (type == QLatin1String("opponentGone")) {
        m_opponentGone = message.value(QStringLiteral("gone")).toBool();
        m_claimWinInSeconds = message.value(QStringLiteral("claimWinInSeconds")).toInt();
        emit stateChanged();
    }
}

void GameController::onStreamFinished(int status, const QString &error)
{
    if (m_stream)
        m_stream->deleteLater();
    m_stream.clear();
    setConnected(false);
    setLoading(false);

    if (status >= 400 && status < 500 && status != 429) {
        // Not our game, not playable through the Board API, or gone.
        setError(error.isEmpty() ? tr("This game cannot be played in salichess.") : error);
        return;
    }
    if (gameOver())
        return;

    ++m_failures;
    const int delay = status == 429 ? 60 * 1000 : qMin(MaxBackoffMs, 1000 << qMin(m_failures, 5));
    if (m_failures > 2)
        setError(tr("Connection lost. Reconnecting…"));
    m_reconnectTimer.start(delay);
}

void GameController::applyGameFull(const QJsonObject &full)
{
    m_white = playerInfo(full.value(QStringLiteral("white")).toObject());
    m_black = playerInfo(full.value(QStringLiteral("black")).toObject());

    const QString me = Services::session() ? Services::session()->userId() : QString();
    if (!me.isEmpty() && m_white.value(QStringLiteral("id")).toString() == me)
        m_myColor = QStringLiteral("white");
    else if (!me.isEmpty() && m_black.value(QStringLiteral("id")).toString() == me)
        m_myColor = QStringLiteral("black");
    else
        m_myColor.clear();
    m_chat->setMyName(Services::session() ? Services::session()->username() : QString());

    m_speed = full.value(QStringLiteral("speed")).toString();
    m_perfName = full.value(QStringLiteral("perf")).toObject().value(QStringLiteral("name")).toString();
    m_rated = full.value(QStringLiteral("rated")).toBool();
    m_variantName = full.value(QStringLiteral("variant")).toObject().value(QStringLiteral("name")).toString();
    m_hasClock = full.value(QStringLiteral("clock")).isObject();
    m_daysPerTurn = full.value(QStringLiteral("daysPerTurn")).toInt();
    emit infoChanged();

    const QString initialFen = full.value(QStringLiteral("initialFen")).toString();
    m_pendingMove.clear();
    m_serverMoves.clear();
    m_game->reset(initialFen);

    applyState(full.value(QStringLiteral("state")).toObject());
    setLoading(false);
    loadChatHistory();
}

void GameController::applyState(const QJsonObject &state)
{
    const QStringList moves = splitMoves(state.value(QStringLiteral("moves")).toString());
    m_serverMoves = moves;

    const QStringList local = m_game->uciMoves();
    if (!m_pendingMove.isEmpty()) {
        if (moves == local) {
            m_pendingMove.clear(); // our move was confirmed
        } else if (moves.size() == local.size() - 1 && moves == local.mid(0, moves.size())) {
            // Stale state from before our move; keep the optimistic move.
        } else {
            m_pendingMove.clear();
            m_game->setUciMoves(moves);
        }
    } else if (moves != local) {
        m_game->setUciMoves(moves);
    }

    m_whiteTime = qint64(state.value(QStringLiteral("wtime")).toDouble());
    m_blackTime = qint64(state.value(QStringLiteral("btime")).toDouble());
    m_clockStamp.restart();

    m_status = state.value(QStringLiteral("status")).toString();
    m_winner = state.value(QStringLiteral("winner")).toString();
    m_wdraw = state.value(QStringLiteral("wdraw")).toBool();
    m_bdraw = state.value(QStringLiteral("bdraw")).toBool();
    m_wtakeback = state.value(QStringLiteral("wtakeback")).toBool();
    m_btakeback = state.value(QStringLiteral("btakeback")).toBool();
    if (gameOver())
        m_opponentGone = false;

    updateClockTimer();
    emit clockChanged();
    emit stateChanged();
}

void GameController::loadChatHistory()
{
    if (m_chatLoaded || m_myColor.isEmpty())
        return;
    m_chatLoaded = true;
    Services::api()->get(QStringLiteral("/api/board/game/%1/chat").arg(m_gameId), QUrlQuery(), this,
                         [this](const ApiResult &result) {
        if (!result.ok())
            return;
        const QJsonArray lines = result.json.array();
        if (lines.isEmpty())
            return;
        // History replaces anything the stream delivered in the meantime.
        m_chat->clear();
        for (const QJsonValue &value : lines) {
            const QJsonObject line = value.toObject();
            m_chat->append(line.value(QStringLiteral("user")).toString(),
                           line.value(QStringLiteral("text")).toString());
        }
    });
}

void GameController::post(const QString &action, const QUrlQuery &form)
{
    if (m_gameId.isEmpty())
        return;
    Services::api()->postForm(QStringLiteral("/api/board/game/%1/%2").arg(m_gameId, action), form, this,
                              [this](const ApiResult &result) {
        if (!result.ok())
            emit actionFailed(result.errorString);
    });
}

void GameController::updateClockTimer()
{
    if (runningClock().isEmpty()) {
        m_clockTimer.stop();
        return;
    }
    // A correspondence turn is counted in days: once a second is plenty.
    m_clockTimer.setInterval(m_hasClock ? 200 : 1000);
    if (!m_clockTimer.isActive())
        m_clockTimer.start();
}

void GameController::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

void GameController::setConnected(bool connected)
{
    if (m_connected == connected)
        return;
    m_connected = connected;
    emit connectedChanged();
}

void GameController::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorStringChanged();
}

bool GameController::whiteToMove() const
{
    return m_game->sideToMove() == QLatin1String("white");
}
