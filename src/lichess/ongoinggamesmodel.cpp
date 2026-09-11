// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ongoinggamesmodel.h"

#include "eventstream.h"
#include "core/lichessapi.h"

#include <QJsonArray>
#include <QJsonObject>

namespace {

// Polling while a game waits for the opponent. After a few loads without any
// change the interval doubles, up to the maximum: correspondence moves take
// hours.
const int PollIntervalMs = 60 * 1000;
const int MaxPollIntervalMs = 5 * 60 * 1000;
const int UnchangedLoadsBeforeBackoff = 5;
// refreshIfStale() leaves a list alone that is younger than this.
const int StaleAfterMs = 30 * 1000;

QString str(const QVariantMap &map, const char *key)
{
    return map.value(QString::fromLatin1(key)).toString();
}

int indexOfGame(const QVector<QVariantMap> &games, const QString &id, int from)
{
    for (int i = from; i < games.size(); ++i) {
        if (str(games.at(i), "gameId") == id)
            return i;
    }
    return -1;
}

} // namespace

OngoingGamesModel::OngoingGamesModel(LichessApi *api, EventStream *events, QObject *parent)
    : QAbstractListModel(parent)
    , m_api(api)
    , m_events(events)
    , m_pollIntervalMs(PollIntervalMs)
{
    m_pollTimer.setSingleShot(true);
    connect(&m_pollTimer, &QTimer::timeout, this, &OngoingGamesModel::refresh);

    // Event bursts (e.g. all games sent when the stream opens) cause a
    // single refresh. So does the stream opening: games may have ended while
    // it was down.
    m_refreshDebounce.setSingleShot(true);
    m_refreshDebounce.setInterval(1500);
    connect(&m_refreshDebounce, &QTimer::timeout, this, &OngoingGamesModel::refresh);
    connect(events, &EventStream::gameStarted, &m_refreshDebounce, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(events, &EventStream::gameFinished, &m_refreshDebounce, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(events, &EventStream::connectedChanged, this, [this]() {
        if (m_events->connected())
            m_refreshDebounce.start();
    });
}

int OngoingGamesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_games.size();
}

QVariant OngoingGamesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_games.size())
        return QVariant();
    const QVariantMap &g = m_games.at(index.row());
    const QVariantMap opponent = g.value(QStringLiteral("opponent")).toMap();

    switch (role) {
    case GameIdRole: return str(g, "gameId");
    case OpponentNameRole: {
        const QString name = str(opponent, "username");
        return name.isEmpty() ? tr("Anonymous") : name;
    }
    case OpponentRatingRole: return opponent.value(QStringLiteral("rating")).toInt();
    case OpponentTitleRole: return str(opponent, "title");
    case ColorRole: return str(g, "color");
    case FenRole: return str(g, "fen");
    case LastMoveRole: return str(g, "lastMove");
    case IsMyTurnRole: return g.value(QStringLiteral("isMyTurn")).toBool();
    case SecondsLeftRole: return g.value(QStringLiteral("secondsLeft")).toInt();
    case SpeedRole: return str(g, "speed");
    case PerfRole: return str(g, "perf");
    case RatedRole: return g.value(QStringLiteral("rated")).toBool();
    case BoardCompatibleRole: {
        const QVariantMap compat = g.value(QStringLiteral("compat")).toMap();
        return compat.isEmpty() || compat.value(QStringLiteral("board")).toBool();
    }
    default: return QVariant();
    }
}

QHash<int, QByteArray> OngoingGamesModel::roleNames() const
{
    return {
        { GameIdRole, "gameId" },
        { OpponentNameRole, "opponentName" },
        { OpponentRatingRole, "opponentRating" },
        { OpponentTitleRole, "opponentTitle" },
        { ColorRole, "color" },
        { FenRole, "fen" },
        { LastMoveRole, "lastMove" },
        { IsMyTurnRole, "isMyTurn" },
        { SecondsLeftRole, "secondsLeft" },
        { SpeedRole, "speed" },
        { PerfRole, "perf" },
        { RatedRole, "rated" },
        { BoardCompatibleRole, "boardCompatible" },
    };
}

int OngoingGamesModel::myTurnCount() const
{
    int n = 0;
    for (const QVariantMap &g : m_games) {
        if (g.value(QStringLiteral("isMyTurn")).toBool())
            ++n;
    }
    return n;
}

QStringList OngoingGamesModel::myTurnOpponents() const
{
    QStringList names;
    for (const QVariantMap &g : m_games) {
        if (!g.value(QStringLiteral("isMyTurn")).toBool())
            continue;
        const QString name = str(g.value(QStringLiteral("opponent")).toMap(), "username");
        names.append(name.isEmpty() ? tr("Anonymous") : name);
    }
    return names;
}

void OngoingGamesModel::refresh()
{
    if (!m_api->hasToken() || m_loading)
        return;
    m_refreshDebounce.stop(); // this refresh covers it
    m_loading = true;
    emit loadingChanged();

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("nb"), QStringLiteral("50"));
    m_api->get(QStringLiteral("/api/account/playing"), query, this, [this](const ApiResult &result) {
        m_loading = false;
        emit loadingChanged();
        if (result.ok()) {
            QVector<QVariantMap> games;
            for (const QJsonValue &value : result.json.object().value(QStringLiteral("nowPlaying")).toArray())
                games.append(value.toObject().toVariantMap());
            m_lastLoad.start();
            if (setGames(games)) {
                m_unchangedLoads = 0;
                m_pollIntervalMs = PollIntervalMs;
            } else if (++m_unchangedLoads >= UnchangedLoadsBeforeBackoff) {
                m_pollIntervalMs = qMin(2 * m_pollIntervalMs, MaxPollIntervalMs);
            }
        }
        schedulePoll();
    });
}

void OngoingGamesModel::refreshIfStale()
{
    m_unchangedLoads = 0;
    if (m_pollIntervalMs != PollIntervalMs) {
        m_pollIntervalMs = PollIntervalMs;
        schedulePoll();
    }
    // The event stream triggers a refresh whenever it (re)connects.
    if (m_loading || m_refreshDebounce.isActive() || !m_events->connected())
        return;
    if (m_lastLoad.isValid() && m_lastLoad.elapsed() < StaleAfterMs)
        return;
    refresh();
}

void OngoingGamesModel::setPolling(bool enabled)
{
    m_polling = enabled;
    m_unchangedLoads = 0;
    m_pollIntervalMs = PollIntervalMs;
    schedulePoll();
}

void OngoingGamesModel::clear()
{
    beginResetModel();
    m_games.clear();
    endResetModel();
    m_lastMyTurn.clear();
    m_loadedOnce = false;
    m_lastLoad.invalidate();
    emit countChanged();
}

bool OngoingGamesModel::waitingForOpponent() const
{
    for (const QVariantMap &g : m_games) {
        if (!g.value(QStringLiteral("isMyTurn")).toBool())
            return true;
    }
    return false;
}

void OngoingGamesModel::schedulePoll()
{
    // New and finished games come from the event stream: without games
    // there is nothing to poll for.
    if (!m_polling || (m_loadedOnce && m_games.isEmpty())) {
        m_pollTimer.stop();
        return;
    }
    // With every game waiting for me, a poll can only notice moves I made
    // elsewhere (on the website, or in a game page I left): rarely will do.
    m_pollTimer.start(!m_loadedOnce || waitingForOpponent() ? m_pollIntervalMs : MaxPollIntervalMs);
}

bool OngoingGamesModel::setGames(const QVector<QVariantMap> &games)
{
    QHash<QString, bool> turns;
    for (const QVariantMap &g : games) {
        const QString id = str(g, "gameId");
        const bool mine = g.value(QStringLiteral("isMyTurn")).toBool();
        turns.insert(id, mine);
        if (!m_loadedOnce)
            continue;
        const QString opponent = str(g.value(QStringLiteral("opponent")).toMap(), "username");
        if (!m_lastMyTurn.contains(id) && str(g, "source") == QLatin1String("lobby")
                && str(g, "speed") == QLatin1String("correspondence"))
            emit newLobbyGame(id, opponent);
        else if (mine && !m_lastMyTurn.value(id, false))
            emit myTurn(id, opponent);
    }
    m_lastMyTurn = turns;
    m_loadedOnce = true;

    if (games == m_games)
        return false;

    // Update the rows in place rather than resetting the model, so that the
    // delegates (each draws a mini board) survive: games that ended are
    // removed, the others moved into place and new ones inserted.
    for (int row = m_games.size() - 1; row >= 0; --row) {
        if (!turns.contains(str(m_games.at(row), "gameId"))) {
            beginRemoveRows(QModelIndex(), row, row);
            m_games.remove(row);
            endRemoveRows();
        }
    }
    for (int row = 0; row < games.size(); ++row) {
        const int from = indexOfGame(m_games, str(games.at(row), "gameId"), row);
        if (from < 0) {
            beginInsertRows(QModelIndex(), row, row);
            m_games.insert(row, games.at(row));
            endInsertRows();
            continue;
        }
        if (from != row) {
            beginMoveRows(QModelIndex(), from, from, QModelIndex(), row);
            m_games.move(from, row);
            endMoveRows();
        }
        if (m_games.at(row) != games.at(row)) {
            m_games[row] = games.at(row);
            emit dataChanged(index(row), index(row));
        }
    }
    // Only left over if Lichess listed a game twice.
    if (m_games.size() > games.size()) {
        beginRemoveRows(QModelIndex(), games.size(), m_games.size() - 1);
        m_games.resize(games.size());
        endRemoveRows();
    }
    emit countChanged();
    return true;
}
