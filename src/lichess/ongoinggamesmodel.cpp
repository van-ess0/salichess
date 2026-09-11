// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ongoinggamesmodel.h"

#include "eventstream.h"
#include "core/lichessapi.h"

#include <QJsonArray>
#include <QJsonObject>

namespace {

const int PollIntervalMs = 60 * 1000;

QString str(const QVariantMap &map, const char *key)
{
    return map.value(QString::fromLatin1(key)).toString();
}

} // namespace

OngoingGamesModel::OngoingGamesModel(LichessApi *api, EventStream *events, QObject *parent)
    : QAbstractListModel(parent)
    , m_api(api)
{
    m_pollTimer.setInterval(PollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &OngoingGamesModel::refresh);

    // Event bursts (e.g. all games sent when the stream opens) cause a
    // single refresh.
    m_refreshDebounce.setSingleShot(true);
    m_refreshDebounce.setInterval(1500);
    connect(&m_refreshDebounce, &QTimer::timeout, this, &OngoingGamesModel::refresh);
    connect(events, &EventStream::gameStarted, &m_refreshDebounce, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(events, &EventStream::gameFinished, &m_refreshDebounce, static_cast<void (QTimer::*)()>(&QTimer::start));
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
    m_loading = true;
    emit loadingChanged();

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("nb"), QStringLiteral("50"));
    m_api->get(QStringLiteral("/api/account/playing"), query, this, [this](const ApiResult &result) {
        m_loading = false;
        emit loadingChanged();
        if (!result.ok())
            return;
        QVector<QVariantMap> games;
        for (const QJsonValue &value : result.json.object().value(QStringLiteral("nowPlaying")).toArray())
            games.append(value.toObject().toVariantMap());
        setGames(games);
    });
}

void OngoingGamesModel::setPolling(bool enabled)
{
    if (enabled)
        m_pollTimer.start();
    else
        m_pollTimer.stop();
}

void OngoingGamesModel::clear()
{
    beginResetModel();
    m_games.clear();
    endResetModel();
    m_lastMyTurn.clear();
    m_loadedOnce = false;
    emit countChanged();
}

void OngoingGamesModel::setGames(const QVector<QVariantMap> &games)
{
    QHash<QString, bool> turns;
    for (const QVariantMap &g : games) {
        const QString id = str(g, "gameId");
        const bool mine = g.value(QStringLiteral("isMyTurn")).toBool();
        turns.insert(id, mine);
        if (m_loadedOnce && mine && !m_lastMyTurn.value(id, false)) {
            const QString opponent = str(g.value(QStringLiteral("opponent")).toMap(), "username");
            emit myTurn(id, opponent);
        }
    }
    m_lastMyTurn = turns;
    m_loadedOnce = true;

    beginResetModel();
    m_games = games;
    endResetModel();
    emit countChanged();
}
