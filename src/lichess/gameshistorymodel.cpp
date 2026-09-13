// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gameshistorymodel.h"

#include "core/lichessapi.h"
#include "core/ndjsonstream.h"
#include "core/services.h"
#include "core/session.h"
#include "gameinfo.h"

#include <QJsonObject>
#include <QUrlQuery>

namespace {

// Games per load. Lichess streams them; a page this size keeps the list
// growing smoothly while scrolling without a long wait.
const int PageSize = 20;

} // namespace

GamesHistoryModel::GamesHistoryModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_reloadDebounce.setSingleShot(true);
    m_reloadDebounce.setInterval(0);
    connect(&m_reloadDebounce, &QTimer::timeout, this, &GamesHistoryModel::doReload);

    // Without a username of its own the model shows the logged-in user's
    // games, so it follows the account: logging in fills an empty list, and
    // logging out empties it again.
    if (Session *session = Services::session()) {
        connect(session, &Session::accountChanged, this, [this]() {
            if (m_username.isEmpty())
                reload();
        });
    }
    reload();
}

GamesHistoryModel::~GamesHistoryModel()
{
    closeStream();
}

int GamesHistoryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_games.size();
}

QVariant GamesHistoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_games.size())
        return QVariant();
    const Game &g = m_games.at(index.row());

    switch (role) {
    case GameIdRole: return g.id;
    case OpponentNameRole: return g.opponentName;
    case OpponentTitleRole: return g.opponentTitle;
    case OpponentRatingRole: return g.opponentRating;
    case ColorRole: return g.color;
    case ResultRole: return g.result;
    case StatusRole: return g.status;
    case ResultTextRole: return g.resultText;
    case RatingDiffRole: return g.ratingDiff;
    case FenRole: return g.fen;
    case SpeedRole: return g.speed;
    case PerfRole: return g.perf;
    case RatedRole: return g.rated;
    case VariantRole: return g.variant;
    case OpeningRole: return g.opening;
    case CreatedAtRole: return g.createdAt;
    default: return QVariant();
    }
}

QHash<int, QByteArray> GamesHistoryModel::roleNames() const
{
    return {
        { GameIdRole, "gameId" },
        { OpponentNameRole, "opponentName" },
        { OpponentTitleRole, "opponentTitle" },
        { OpponentRatingRole, "opponentRating" },
        { ColorRole, "color" },
        { ResultRole, "result" },
        { StatusRole, "status" },
        { ResultTextRole, "resultText" },
        { RatingDiffRole, "ratingDiff" },
        { FenRole, "fen" },
        { SpeedRole, "speed" },
        { PerfRole, "perf" },
        { RatedRole, "rated" },
        { VariantRole, "variant" },
        { OpeningRole, "opening" },
        { CreatedAtRole, "createdAt" },
    };
}

void GamesHistoryModel::setUsername(const QString &username)
{
    if (m_username == username)
        return;
    m_username = username;
    emit usernameChanged();
    reload();
}

void GamesHistoryModel::setPerfType(const QString &perfType)
{
    if (m_perfType == perfType)
        return;
    m_perfType = perfType;
    emit filterChanged();
    reload();
}

void GamesHistoryModel::setColor(const QString &color)
{
    if (m_color == color)
        return;
    m_color = color;
    emit filterChanged();
    reload();
}

void GamesHistoryModel::setRated(RatedFilter rated)
{
    if (m_rated == rated)
        return;
    m_rated = rated;
    emit filterChanged();
    reload();
}

void GamesHistoryModel::setAnalysedOnly(bool only)
{
    if (m_analysedOnly == only)
        return;
    m_analysedOnly = only;
    emit filterChanged();
    reload();
}

void GamesHistoryModel::setOpponent(const QString &opponent)
{
    if (m_opponent == opponent)
        return;
    m_opponent = opponent;
    emit filterChanged();
    reload();
}

bool GamesHistoryModel::filtered() const
{
    return !m_perfType.isEmpty() || !m_color.isEmpty() || m_rated != AnyGames
            || m_analysedOnly || !m_opponent.isEmpty();
}

void GamesHistoryModel::clearFilters()
{
    if (!filtered())
        return;
    m_perfType.clear();
    m_color.clear();
    m_rated = AnyGames;
    m_analysedOnly = false;
    m_opponent.clear();
    emit filterChanged();
    reload();
}

void GamesHistoryModel::refresh()
{
    reload();
}

void GamesHistoryModel::reload()
{
    m_reloadDebounce.start();
}

void GamesHistoryModel::doReload()
{
    closeStream();
    clear();
    load(0);
}

void GamesHistoryModel::loadMore()
{
    if (m_loading || !m_hasMore || m_games.isEmpty())
        return;
    // "until" is inclusive, so step past the oldest game held; games that
    // share the timestamp are caught by the id guard.
    load(m_games.last().createdAt - 1);
}

void GamesHistoryModel::clear()
{
    if (!m_games.isEmpty()) {
        beginResetModel();
        m_games.clear();
        endResetModel();
        emit countChanged();
    }
    m_ids.clear();
    m_pending.clear();
    setHasMore(true);
    setError(QString());
}

void GamesHistoryModel::load(qint64 until)
{
    LichessApi *api = Services::api();
    Session *session = Services::session();
    if (!api)
        return;
    const QString user = m_username.isEmpty() && session ? session->username() : m_username;
    if (user.isEmpty()) {
        setLoading(false);
        return;
    }

    closeStream();
    m_pending.clear();
    setError(QString());
    setLoading(true);

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("max"), QString::number(PageSize));
    query.addQueryItem(QStringLiteral("sort"), QStringLiteral("dateDesc"));
    query.addQueryItem(QStringLiteral("finished"), QStringLiteral("true"));
    // The final position draws the mini board; the moves themselves are only
    // needed once a game is opened.
    query.addQueryItem(QStringLiteral("lastFen"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("opening"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("moves"), QStringLiteral("false"));
    query.addQueryItem(QStringLiteral("tags"), QStringLiteral("false"));
    if (until > 0)
        query.addQueryItem(QStringLiteral("until"), QString::number(until));
    if (!m_perfType.isEmpty())
        query.addQueryItem(QStringLiteral("perfType"), m_perfType);
    if (!m_color.isEmpty())
        query.addQueryItem(QStringLiteral("color"), m_color);
    if (m_rated != AnyGames)
        query.addQueryItem(QStringLiteral("rated"), m_rated == RatedOnly ? QStringLiteral("true")
                                                                         : QStringLiteral("false"));
    if (m_analysedOnly)
        query.addQueryItem(QStringLiteral("analysed"), QStringLiteral("true"));
    if (!m_opponent.isEmpty())
        query.addQueryItem(QStringLiteral("vs"), m_opponent);

    m_stream = api->openStream(QStringLiteral("/api/games/user/") + user, query);
    m_stream->setParent(this);
    connect(m_stream.data(), &NdjsonStream::message, this, [this, user](const QJsonObject &game) {
        m_pending.append(parseGame(game, user));
    });
    connect(m_stream.data(), &NdjsonStream::finished, this, [this](int status, const QString &error) {
        const int received = m_pending.size();
        if (error.isEmpty() || received > 0) {
            appendPage();
            // A short page means Lichess had nothing more to send.
            setHasMore(received == PageSize);
        } else {
            setError(status == 404 ? tr("No such player") : error);
            setHasMore(false);
        }
        if (m_stream)
            m_stream->deleteLater();
        m_stream.clear();
        setLoading(false);
    });
}

void GamesHistoryModel::appendPage()
{
    QVector<Game> fresh;
    fresh.reserve(m_pending.size());
    for (const Game &game : m_pending) {
        if (game.id.isEmpty() || m_ids.contains(game.id))
            continue;
        m_ids.insert(game.id);
        fresh.append(game);
    }
    m_pending.clear();
    if (fresh.isEmpty())
        return;

    beginInsertRows(QModelIndex(), m_games.size(), m_games.size() + fresh.size() - 1);
    m_games += fresh;
    endInsertRows();
    emit countChanged();
}

void GamesHistoryModel::closeStream()
{
    if (!m_stream)
        return;
    NdjsonStream *stream = m_stream.data();
    m_stream.clear();
    stream->disconnect(this);
    stream->abort();
    stream->deleteLater();
}

GamesHistoryModel::Game GamesHistoryModel::parseGame(const QJsonObject &json, const QString &me)
{
    Game game;
    game.id = json.value(QStringLiteral("id")).toString();
    game.status = json.value(QStringLiteral("status")).toString();
    game.speed = json.value(QStringLiteral("speed")).toString();
    game.perf = json.value(QStringLiteral("perf")).toString();
    game.rated = json.value(QStringLiteral("rated")).toBool();
    game.variant = json.value(QStringLiteral("variant")).toString();
    game.fen = json.value(QStringLiteral("lastFen")).toString();
    game.createdAt = qint64(json.value(QStringLiteral("createdAt")).toDouble());
    game.opening = json.value(QStringLiteral("opening")).toObject()
            .value(QStringLiteral("name")).toString();

    const QJsonObject players = json.value(QStringLiteral("players")).toObject();
    const QJsonObject white = players.value(QStringLiteral("white")).toObject();
    const QJsonObject black = players.value(QStringLiteral("black")).toObject();
    const QString whiteId = white.value(QStringLiteral("user")).toObject()
            .value(QStringLiteral("id")).toString();
    // These are the games of |me|, so anything but a match is black.
    game.color = whiteId.compare(me, Qt::CaseInsensitive) == 0 ? QStringLiteral("white")
                                                               : QStringLiteral("black");
    const bool playedWhite = game.color == QLatin1String("white");
    const QVariantMap whiteInfo = GameInfo::exportedPlayer(white);
    const QVariantMap blackInfo = GameInfo::exportedPlayer(black);
    const QVariantMap mine = playedWhite ? whiteInfo : blackInfo;
    const QVariantMap theirs = playedWhite ? blackInfo : whiteInfo;

    game.opponentName = theirs.value(QStringLiteral("name")).toString();
    game.opponentTitle = theirs.value(QStringLiteral("title")).toString();
    game.opponentRating = theirs.value(QStringLiteral("rating")).toInt();
    game.ratingDiff = mine.value(QStringLiteral("ratingDiff")).toInt();

    const QString winner = json.value(QStringLiteral("winner")).toString();
    game.result = GameInfo::outcome(game.status, winner, game.color);
    game.resultText = GameInfo::resultText(game.status, winner,
                                           whiteInfo.value(QStringLiteral("name")).toString(),
                                           blackInfo.value(QStringLiteral("name")).toString());
    return game;
}

void GamesHistoryModel::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

void GamesHistoryModel::setHasMore(bool hasMore)
{
    if (m_hasMore == hasMore)
        return;
    m_hasMore = hasMore;
    emit hasMoreChanged();
}

void GamesHistoryModel::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorStringChanged();
}
