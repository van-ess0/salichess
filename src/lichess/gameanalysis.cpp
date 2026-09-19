// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gameanalysis.h"

#include "chess/chessgame.h"
#include "core/lichessapi.h"
#include "core/services.h"
#include "gameinfo.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUrlQuery>
#include <QtMath>

namespace {

// The position looked at settles before its cloud evaluation is asked for:
// stepping through a game would otherwise send a request per move.
const int CloudEvalDelayMs = 400;

// A mate is drawn at the end of the graph, not off it.
const int MateCp = 10000;

// Lichess takes a while to analyse a game; it is asked about it now and then
// rather than over a websocket, which would be a protocol of its own just to
// draw a progress bar.
const int AnalysisPollMs = 4000;
const int AnalysisPolls = 75; // five minutes' worth

} // namespace

GameAnalysis::GameAnalysis(QObject *parent)
    : QObject(parent)
    , m_game(new ChessGame(this))
{
    connect(m_game, &ChessGame::positionChanged, this, [this]() {
        emit viewChanged();
        if (!viewInfo())
            m_cloudTimer.start();
    });

    m_cloudTimer.setSingleShot(true);
    m_cloudTimer.setInterval(CloudEvalDelayMs);
    connect(&m_cloudTimer, &QTimer::timeout, this, &GameAnalysis::requestCloudEval);

    m_pollTimer.setInterval(AnalysisPollMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &GameAnalysis::pollForAnalysis);
}

void GameAnalysis::requestAnalysis()
{
    LichessApi *api = Services::api();
    if (!api || m_gameId.isEmpty() || m_requesting || m_hasServerAnalysis)
        return;
    m_requesting = true;
    m_requestError.clear();
    emit requestChanged();

    const QString id = m_gameId;
    api->postForm(QLatin1Char('/') + id + QStringLiteral("/request-analysis"), QUrlQuery(), this,
                  [this, id](const ApiResult &result) {
        if (id != m_gameId)
            return;
        // 400 is what Lichess answers when the game is already queued, which
        // is not a reason to stop waiting for it.
        if (!result.ok() && result.status != 400) {
            endRequest(result.status == 401 || result.status == 403
                       ? tr("Lichess would not take the request. The game can be analysed on the website.")
                       : (result.errorString.isEmpty() ? tr("Could not ask for an analysis")
                                                       : result.errorString));
            return;
        }
        m_pollsLeft = AnalysisPolls;
        m_pollTimer.start();
    });
}

void GameAnalysis::pollForAnalysis()
{
    if (--m_pollsLeft <= 0) {
        endRequest(tr("Lichess is taking a long time. Try again later."));
        return;
    }
    if (!m_loading)
        fetch();
}

void GameAnalysis::endRequest(const QString &error)
{
    m_pollTimer.stop();
    m_pollsLeft = 0;
    m_requesting = false;
    m_requestError = error;
    emit requestChanged();
}

void GameAnalysis::setGameId(const QString &id)
{
    if (m_gameId == id)
        return;
    m_gameId = id;
    emit gameIdChanged();
    resetGameData();
    if (!m_gameId.isEmpty())
        fetch();
}

void GameAnalysis::setStartFen(const QString &fen)
{
    if (m_startFen == fen)
        return;
    m_startFen = fen;
    emit startFenChanged();
    if (m_gameId.isEmpty()) {
        resetGameData();
        m_cloudTimer.start();
    }
}

void GameAnalysis::reload()
{
    if (m_gameId.isEmpty() || m_loading)
        return;
    fetch();
}

void GameAnalysis::resetGameData()
{
    m_white.clear();
    m_black.clear();
    m_speed.clear();
    m_perf.clear();
    m_variant.clear();
    m_status.clear();
    m_winner.clear();
    m_openingName.clear();
    m_openingEco.clear();
    m_createdAt = 0;
    m_rated = false;
    m_hasServerAnalysis = false;
    m_plies.clear();
    m_clocks.clear();
    m_cloudEvals.clear();
    endRequest(QString());
    m_game->reset(m_gameId.isEmpty() ? m_startFen : QString());
    // After the reset: it moves the view, which is what arms the timer.
    m_cloudTimer.stop();
    setLoading(false);
    setError(QString());
    emit infoChanged();
    emit viewChanged();
}

void GameAnalysis::fetch()
{
    LichessApi *api = Services::api();
    if (!api)
        return;
    setLoading(true);
    setError(QString());

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("evals"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("accuracy"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("opening"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("clocks"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("moves"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("tags"), QStringLiteral("false"));
    const QString id = m_gameId;
    api->get(QStringLiteral("/game/export/") + id, query, this, [this, id](const ApiResult &result) {
        if (id != m_gameId)
            return; // another game was opened while this was on its way
        setLoading(false);
        if (!result.ok()) {
            setError(result.status == 404 ? tr("This game does not exist")
                                          : (result.errorString.isEmpty()
                                             ? tr("Could not load the game") : result.errorString));
            return;
        }
        applyGame(result.json.object());
    });
}

void GameAnalysis::applyGame(const QJsonObject &json)
{
    const QJsonObject players = json.value(QStringLiteral("players")).toObject();
    m_white = GameInfo::exportedPlayer(players.value(QStringLiteral("white")).toObject());
    m_black = GameInfo::exportedPlayer(players.value(QStringLiteral("black")).toObject());
    m_speed = json.value(QStringLiteral("speed")).toString();
    m_perf = json.value(QStringLiteral("perf")).toString();
    m_variant = json.value(QStringLiteral("variant")).toString();
    m_status = json.value(QStringLiteral("status")).toString();
    m_winner = json.value(QStringLiteral("winner")).toString();
    m_rated = json.value(QStringLiteral("rated")).toBool();
    m_createdAt = qint64(json.value(QStringLiteral("createdAt")).toDouble());

    const QJsonObject opening = json.value(QStringLiteral("opening")).toObject();
    m_openingName = opening.value(QStringLiteral("name")).toString();
    m_openingEco = opening.value(QStringLiteral("eco")).toString();

    m_clocks.clear();
    for (const QJsonValue &value : json.value(QStringLiteral("clocks")).toArray())
        m_clocks.append(value.toInt() * 10); // centiseconds on the wire

    // The rules engine plays standard chess: Chess960 is the same game from
    // another starting position, but the rest is not.
    const bool standard = m_variant.isEmpty() || m_variant == QLatin1String("standard")
            || m_variant == QLatin1String("chess960") || m_variant == QLatin1String("fromPosition");
    m_game->reset(json.value(QStringLiteral("initialFen")).toString());
    if (!standard) {
        setError(tr("salichess cannot show %1 games").arg(m_variant));
        emit infoChanged();
        emit viewChanged();
        return;
    }
    const QString moves = json.value(QStringLiteral("moves")).toString();
    if (!moves.isEmpty() && !m_game->playSanMoves(moves))
        setError(tr("This game could not be replayed"));

    // analysis[i] describes the position after move i + 1: its evaluation,
    // and whether that move deserved a judgment.
    m_plies.clear();
    const QJsonArray analysis = json.value(QStringLiteral("analysis")).toArray();
    m_plies.reserve(analysis.size());
    for (const QJsonValue &value : analysis) {
        const QJsonObject entry = value.toObject();
        PlyInfo info;
        info.mate = entry.value(QStringLiteral("mate")).toInt();
        if (entry.contains(QStringLiteral("eval")) || info.mate != 0) {
            info.hasEval = true;
            info.cp = entry.value(QStringLiteral("eval")).toInt();
        }
        info.best = entry.value(QStringLiteral("best")).toString();
        info.variation = entry.value(QStringLiteral("variation")).toString();
        const QJsonObject judgment = entry.value(QStringLiteral("judgment")).toObject();
        info.judgment = judgment.value(QStringLiteral("name")).toString();
        info.comment = judgment.value(QStringLiteral("comment")).toString();
        m_plies.append(info);
    }
    m_hasServerAnalysis = !m_plies.isEmpty();
    if (m_hasServerAnalysis && m_requesting)
        endRequest(QString()); // it arrived

    emit infoChanged();
    emit viewChanged();
    if (!viewInfo())
        m_cloudTimer.start();
}

QString GameAnalysis::resultText() const
{
    return GameInfo::resultText(m_status, m_winner,
                                m_white.value(QStringLiteral("name")).toString(),
                                m_black.value(QStringLiteral("name")).toString());
}

int GameAnalysis::viewPly() const
{
    return m_game->viewPly();
}

const GameAnalysis::PlyInfo *GameAnalysis::viewInfo() const
{
    const int ply = viewPly();
    if (ply <= 0 || ply > m_plies.size())
        return nullptr;
    // Lichess analysed the game as it was played; a side line the user tried
    // out is not in it, however far along it the board has gone.
    const int variation = m_game->variationStartPly();
    if (variation > 0 && ply >= variation)
        return nullptr;
    return &m_plies.at(ply - 1);
}

const GameAnalysis::PlyInfo *GameAnalysis::nextInfo() const
{
    const int ply = viewPly();
    if (ply < 0 || ply >= m_plies.size())
        return nullptr;
    // In a side line the position is no longer the game's, so what Lichess
    // said about the game does not describe it. The ply the line branches
    // off from is still the game's own position.
    const int variation = m_game->variationStartPly();
    if (variation > 0 && ply >= variation)
        return nullptr;
    return &m_plies.at(ply);
}

QString GameAnalysis::nextBestMove() const
{
    const PlyInfo *info = nextInfo();
    return info ? info->best : QString();
}

QString GameAnalysis::nextBestVariation() const
{
    const PlyInfo *info = nextInfo();
    return info ? info->variation : QString();
}

bool GameAnalysis::hasEval() const
{
    if (const PlyInfo *info = viewInfo())
        return info->hasEval;
    return m_cloudEvals.value(m_game->fen()).hasEval;
}

int GameAnalysis::evalCp() const
{
    if (const PlyInfo *info = viewInfo())
        return info->cp;
    return m_cloudEvals.value(m_game->fen()).cp;
}

int GameAnalysis::evalMate() const
{
    if (const PlyInfo *info = viewInfo())
        return info->mate;
    return m_cloudEvals.value(m_game->fen()).mate;
}

qreal GameAnalysis::winPercent() const
{
    if (!hasEval())
        return 50;
    const int mate = evalMate();
    if (mate != 0)
        return mate > 0 ? 100 : 0;
    return winPercentFromCp(evalCp());
}

QString GameAnalysis::evalSource() const
{
    if (const PlyInfo *info = viewInfo())
        return info->hasEval ? QStringLiteral("server") : QString();
    if (m_cloudEvals.value(m_game->fen()).hasEval)
        return QStringLiteral("cloud");
    return QString();
}

QString GameAnalysis::judgment() const
{
    const PlyInfo *info = viewInfo();
    return info ? info->judgment : QString();
}

QString GameAnalysis::judgmentComment() const
{
    const PlyInfo *info = viewInfo();
    return info ? info->comment : QString();
}

QString GameAnalysis::bestMove() const
{
    const PlyInfo *info = viewInfo();
    return info ? info->best : QString();
}

QString GameAnalysis::bestVariation() const
{
    const PlyInfo *info = viewInfo();
    return info ? info->variation : QString();
}

int GameAnalysis::clockMs() const
{
    if (m_game->variationStartPly() > 0)
        return -1;
    const int ply = viewPly();
    if (ply <= 0 || ply > m_clocks.size())
        return -1;
    return m_clocks.at(ply - 1);
}

int GameAnalysis::clockFor(bool white) const
{
    // Like the evaluations, the clocks are the game's own; a side line has
    // none.
    const int variation = m_game->variationStartPly();
    if (variation > 0)
        return -1;
    // White moves on the odd plies, black on the even ones: the clock shown
    // is the one left after that side's last move up to here.
    const int wanted = white ? 1 : 0;
    int ply = viewPly();
    if (ply % 2 != wanted)
        --ply;
    if (ply <= 0 || ply > m_clocks.size())
        return -1;
    return m_clocks.at(ply - 1);
}

QVariantList GameAnalysis::evalPoints() const
{
    QVariantList points;
    points.reserve(m_plies.size());
    for (int i = 0; i < m_plies.size(); ++i) {
        const PlyInfo &info = m_plies.at(i);
        if (!info.hasEval)
            continue;
        QVariantMap point;
        point.insert(QStringLiteral("ply"), i + 1);
        point.insert(QStringLiteral("cp"), info.mate != 0 ? (info.mate > 0 ? MateCp : -MateCp)
                                                          : info.cp);
        point.insert(QStringLiteral("mate"), info.mate);
        point.insert(QStringLiteral("win"), info.mate != 0 ? (info.mate > 0 ? 100.0 : 0.0)
                                                           : winPercentFromCp(info.cp));
        point.insert(QStringLiteral("judgment"), info.judgment);
        points.append(point);
    }
    return points;
}

QStringList GameAnalysis::judgments() const
{
    QStringList names;
    names.reserve(m_plies.size());
    for (const PlyInfo &info : m_plies)
        names.append(info.judgment);
    return names;
}

qreal GameAnalysis::winPercentFromCp(int centipawns)
{
    // The formula lichess.org draws its evaluation bar with.
    const qreal win = 2 / (1 + qExp(-0.00368208 * centipawns)) - 1;
    return qBound(qreal(0), 50 + 50 * win, qreal(100));
}

void GameAnalysis::requestCloudEval()
{
    LichessApi *api = Services::api();
    if (!api || viewInfo())
        return;
    const QString fen = m_game->fen();
    if (fen.isEmpty() || m_cloudEvals.contains(fen))
        return;

    m_cloudEvals.insert(fen, CloudEval()); // asked for; not known yet
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("fen"), fen);
    api->get(QStringLiteral("/api/cloud-eval"), query, this, [this, fen](const ApiResult &result) {
        if (result.status == 0) {
            // The request never reached Lichess: ask again when the position
            // is looked at once there is a connection.
            m_cloudEvals.remove(fen);
            return;
        }
        CloudEval &eval = m_cloudEvals[fen];
        eval.known = true;
        // 404 is the usual answer: most positions are not in the cloud.
        if (result.ok()) {
            const QJsonArray pvs = result.json.object().value(QStringLiteral("pvs")).toArray();
            if (!pvs.isEmpty()) {
                const QJsonObject pv = pvs.first().toObject();
                eval.hasEval = true;
                eval.cp = pv.value(QStringLiteral("cp")).toInt();
                eval.mate = pv.value(QStringLiteral("mate")).toInt();
            }
        }
        if (fen == m_game->fen())
            emit viewChanged();
    });
}

void GameAnalysis::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

void GameAnalysis::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorStringChanged();
}
