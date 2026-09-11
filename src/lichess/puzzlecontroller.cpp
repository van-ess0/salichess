// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "puzzlecontroller.h"

#include "chess/chessgame.h"
#include "chess/chessposition.h"
#include "core/appsettings.h"
#include "core/lichessapi.h"
#include "core/services.h"
#include "core/session.h"

#include <QJsonArray>
#include <QRegularExpression>

namespace {

const int BatchSize = 15;
const int LeadInDelayMs = 600;
const int ReplyDelayMs = 450;
const int UndoDelayMs = 700;
const int SolutionStepMs = 800;

} // namespace

PuzzleController::PuzzleController(QObject *parent)
    : QObject(parent)
    , m_game(new ChessGame(this))
{
    m_moveTimer.setSingleShot(true);
    connect(&m_moveTimer, &QTimer::timeout, this, [this]() {
        const Pending pending = m_pending;
        m_pending = Pending::None;
        switch (pending) {
        case Pending::LeadIn: playLeadInMove(); break;
        case Pending::Reply: playReply(); break;
        case Pending::Undo:
            m_game->undo();
            if (m_feedback == WrongMove)
                setFeedback(NoFeedback);
            break;
        case Pending::Solution: playNextSolutionMove(); break;
        case Pending::None: break;
        }
    });
}

void PuzzleController::setAngle(const QString &angle)
{
    const QString value = angle.isEmpty() ? QStringLiteral("mix") : angle;
    if (m_angle == value)
        return;
    m_angle = value;
    emit angleChanged();
}

void PuzzleController::setDifficulty(const QString &difficulty)
{
    if (m_difficulty == difficulty)
        return;
    m_difficulty = difficulty;
    emit difficultyChanged();
}

QString PuzzleController::puzzleUrl() const
{
    return m_puzzleId.isEmpty() ? QString()
                                : LichessApi::siteUrl() + QStringLiteral("/training/") + m_puzzleId;
}

QString PuzzleController::gameUrl() const
{
    if (m_gameId.isEmpty())
        return QString();
    return QStringLiteral("%1/%2/%3#%4").arg(LichessApi::siteUrl(), m_gameId, m_playerColor)
            .arg(m_initialPly + 1);
}

void PuzzleController::loadDaily()
{
    fetchSingle(QStringLiteral("/api/puzzle/daily"), true);
}

void PuzzleController::loadNext()
{
    const QString key = m_angle + QLatin1Char('|') + effectiveDifficulty();
    if (key != m_queueKey) {
        m_queue.clear();
        m_queueKey = key;
    }
    if (!m_queue.isEmpty()) {
        startPuzzle(m_queue.dequeue(), false);
        return;
    }
    fetchBatch();
}

void PuzzleController::loadPuzzle(const QString &id)
{
    static const QRegularExpression validId(QStringLiteral("^[A-Za-z0-9]{3,12}$"));
    if (!validId.match(id).hasMatch()) {
        setError(tr("Invalid puzzle id."));
        return;
    }
    fetchSingle(QStringLiteral("/api/puzzle/") + id, false);
}

void PuzzleController::move(const QString &uci)
{
    if (m_state != Playing || m_pending != Pending::None || !m_game->atLatest())
        return;

    const ChessPosition before = m_game->position();
    const QString played = before.normalizeUci(uci);
    if (played.isEmpty())
        return;

    const PuzzleLogic::Verdict verdict = m_logic.playerMove(before, played);
    setHint(-1);
    m_game->playUci(played);

    switch (verdict) {
    case PuzzleLogic::Correct:
        setFeedback(GoodMove);
        m_pending = Pending::Reply;
        m_moveTimer.start(ReplyDelayMs);
        break;
    case PuzzleLogic::Solved:
        setFeedback(Success);
        finish();
        break;
    case PuzzleLogic::Wrong:
        emit stateChanged(); // hadMistake
        setFeedback(WrongMove);
        m_pending = Pending::Undo;
        m_moveTimer.start(UndoDelayMs);
        break;
    }
}

void PuzzleController::showHint()
{
    if (m_state != Playing || m_pending != Pending::None)
        return;
    const QString expected = m_logic.expectedMove();
    if (expected.isEmpty())
        return;
    m_hintUsed = true;
    const int from = ChessPosition::squareFromName(expected.left(2));
    if (m_hintSquare != from)
        setHint(from);
    else
        setHint(from, ChessPosition::squareFromName(expected.mid(2, 2)));
}

void PuzzleController::viewSolution()
{
    if (m_state != Playing)
        return;
    m_moveTimer.stop();
    if (m_pending == Pending::LeadIn)
        playLeadInMove();
    else if (m_pending == Pending::Undo)
        m_game->undo();
    else if (m_pending == Pending::Reply)
        playReply();
    m_pending = Pending::None;
    if (m_state != Playing)
        return; // the reply finished the puzzle

    m_logic.markFailed();
    m_game->viewLatest();
    setHint(-1);
    setState(ShowingSolution);
    setFeedback(SolutionShown);
    m_pending = Pending::Solution;
    m_moveTimer.start(SolutionStepMs / 2);
}

bool PuzzleController::startPuzzle(const QJsonObject &data, bool daily)
{
    m_moveTimer.stop();
    m_pending = Pending::None;

    const QJsonObject gameData = data.value(QStringLiteral("game")).toObject();
    const QJsonObject puzzle = data.value(QStringLiteral("puzzle")).toObject();

    QStringList solution;
    for (const QJsonValue &move : puzzle.value(QStringLiteral("solution")).toArray())
        solution.append(move.toString());

    static const QRegularExpression separator(QStringLiteral("\\s+"));
    QStringList sanMoves = gameData.value(QStringLiteral("pgn")).toString()
            .split(separator, QString::SkipEmptyParts);

    // Show the position before the opponent's last move, then play that move
    // so the player sees what just happened.
    m_game->reset();
    m_leadInMove.clear();
    bool ok = !solution.isEmpty() && !sanMoves.isEmpty();
    if (ok) {
        const QString lastSan = sanMoves.takeLast();
        ok = m_game->playSanMoves(sanMoves.join(QLatin1Char(' ')));
        if (ok) {
            m_leadInMove = m_game->position().uciForSan(lastSan);
            ok = !m_leadInMove.isEmpty();
        }
    }
    if (ok) {
        ChessPosition start = m_game->position();
        start.playUci(m_leadInMove);
        ok = !start.normalizeUci(solution.first()).isEmpty();
        m_playerColor = start.sideToMove() == ChessPosition::White ? QStringLiteral("white")
                                                                   : QStringLiteral("black");
    }
    if (!ok) {
        setError(tr("This puzzle could not be loaded."));
        return false;
    }

    m_game->setFirstViewablePly(m_game->ply());
    m_logic.start(solution);

    m_puzzleId = puzzle.value(QStringLiteral("id")).toString();
    m_gameId = gameData.value(QStringLiteral("id")).toString();
    m_initialPly = puzzle.value(QStringLiteral("initialPly")).toInt();
    m_puzzleRating = puzzle.value(QStringLiteral("rating")).toInt();
    m_plays = puzzle.value(QStringLiteral("plays")).toInt();
    m_themes.clear();
    for (const QJsonValue &theme : puzzle.value(QStringLiteral("themes")).toArray())
        m_themes.append(theme.toString());
    m_isDaily = daily;
    m_hintUsed = false;
    m_ratingDiff = 0;
    m_resultSubmitted = false;
    m_error.clear();
    setHint(-1);
    emit puzzleChanged();
    emit resultChanged();

    setFeedback(NoFeedback);
    setState(Playing);
    m_pending = Pending::LeadIn;
    m_moveTimer.start(LeadInDelayMs);
    return true;
}

void PuzzleController::fetchSingle(const QString &path, bool daily)
{
    m_moveTimer.stop();
    m_pending = Pending::None;
    setState(Loading);
    Services::api()->get(path, QUrlQuery(), this, [this, daily](const ApiResult &result) {
        if (!result.ok()) {
            setError(result.errorString);
            return;
        }
        startPuzzle(result.json.object(), daily);
    });
}

void PuzzleController::fetchBatch()
{
    m_moveTimer.stop();
    m_pending = Pending::None;
    setState(Loading);

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("nb"), QString::number(BatchSize));
    const QString difficulty = effectiveDifficulty();
    if (!difficulty.isEmpty())
        query.addQueryItem(QStringLiteral("difficulty"), difficulty);
    const QString key = m_queueKey;
    Services::api()->get(QStringLiteral("/api/puzzle/batch/") + m_angle, query, this,
                         [this, key](const ApiResult &result) {
        if (key != m_queueKey)
            return; // theme or difficulty changed meanwhile
        if (!result.ok()) {
            setError(result.errorString);
            return;
        }
        for (const QJsonValue &value : result.json.object().value(QStringLiteral("puzzles")).toArray())
            m_queue.enqueue(value.toObject());
        if (m_queue.isEmpty()) {
            setError(tr("No more puzzles available for this theme."));
            return;
        }
        startPuzzle(m_queue.dequeue(), false);
    });
}

void PuzzleController::playLeadInMove()
{
    m_game->playUci(m_leadInMove);
}

void PuzzleController::playReply()
{
    const QString reply = m_logic.nextReply();
    if (!reply.isEmpty()) {
        m_game->playUci(reply);
        m_logic.replyPlayed();
    }
    if (m_logic.finished()) {
        setFeedback(Success);
        finish();
    }
}

void PuzzleController::playNextSolutionMove()
{
    const QString move = m_logic.expectedMove();
    if (move.isEmpty()) {
        finish();
        return;
    }
    m_game->playUci(move);
    m_logic.replyPlayed();
    if (m_logic.finished()) {
        finish();
        return;
    }
    m_pending = Pending::Solution;
    m_moveTimer.start(SolutionStepMs);
}

void PuzzleController::finish()
{
    m_moveTimer.stop();
    m_pending = Pending::None;
    setState(Finished);
    submitResult(!m_logic.failed());
}

void PuzzleController::submitResult(bool win)
{
    Session *session = Services::session();
    if (!session || !session->loggedIn() || m_puzzleId.isEmpty())
        return;

    QJsonObject solution;
    solution.insert(QStringLiteral("id"), m_puzzleId);
    solution.insert(QStringLiteral("win"), win);
    // A hint makes the attempt casual, so the rating stays untouched.
    solution.insert(QStringLiteral("rated"), !m_hintUsed);
    QJsonObject body;
    body.insert(QStringLiteral("solutions"), QJsonArray { solution });

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("nb"), QStringLiteral("0"));
    const QString angle = m_isDaily ? QStringLiteral("mix") : m_angle;
    const QString puzzleId = m_puzzleId;
    Services::api()->postJson(QStringLiteral("/api/puzzle/batch/") + angle, query, QJsonDocument(body), this,
                              [this, puzzleId](const ApiResult &result) {
        if (!result.ok() || puzzleId != m_puzzleId)
            return;
        const QJsonObject response = result.json.object();
        for (const QJsonValue &value : response.value(QStringLiteral("rounds")).toArray()) {
            const QJsonObject round = value.toObject();
            if (round.value(QStringLiteral("id")).toString() == puzzleId)
                m_ratingDiff = round.value(QStringLiteral("ratingDiff")).toInt();
        }
        const QJsonObject glicko = response.value(QStringLiteral("glicko")).toObject();
        if (glicko.contains(QStringLiteral("rating")))
            m_userRating = qRound(glicko.value(QStringLiteral("rating")).toDouble());
        m_resultSubmitted = true;
        emit resultChanged();
    });
}

QString PuzzleController::effectiveDifficulty() const
{
    if (!m_difficulty.isEmpty())
        return m_difficulty;
    return Services::settings() ? Services::settings()->puzzleDifficulty() : QString();
}

void PuzzleController::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
}

void PuzzleController::setFeedback(Feedback feedback)
{
    if (m_feedback == feedback)
        return;
    m_feedback = feedback;
    emit feedbackChanged();
}

void PuzzleController::setHint(int square, int target)
{
    if (m_hintSquare == square && m_hintTarget == target)
        return;
    m_hintSquare = square;
    m_hintTarget = target;
    emit hintSquareChanged();
}

void PuzzleController::setError(const QString &error)
{
    m_error = error;
    setState(Error);
    emit stateChanged();
}
