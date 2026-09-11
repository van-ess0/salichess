// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "challengesmodel.h"

#include "eventstream.h"
#include "core/lichessapi.h"
#include "core/services.h"
#include "core/session.h"

#include <QJsonArray>
#include <QJsonObject>

namespace {

QString str(const QVariantMap &map, const char *key)
{
    return map.value(QString::fromLatin1(key)).toString();
}

QVariantMap opponentOf(const QVariantMap &challenge)
{
    const bool incoming = str(challenge, "direction") == QLatin1String("in");
    return challenge.value(incoming ? QStringLiteral("challenger") : QStringLiteral("destUser")).toMap();
}

QString invertColor(const QString &color)
{
    if (color == QLatin1String("white"))
        return QStringLiteral("black");
    if (color == QLatin1String("black"))
        return QStringLiteral("white");
    return color;
}

} // namespace

ChallengesModel::ChallengesModel(LichessApi *api, EventStream *events, QObject *parent)
    : QAbstractListModel(parent)
    , m_api(api)
{
    connect(events, &EventStream::challengeReceived, this, &ChallengesModel::upsert);
    connect(events, &EventStream::challengeCanceled, this, [this](const QVariantMap &challenge) {
        remove(str(challenge, "id"));
    });
    connect(events, &EventStream::challengeDeclined, this, [this](const QVariantMap &challenge) {
        remove(str(challenge, "id"));
    });
    connect(events, &EventStream::gameStarted, this, [this](const QVariantMap &game) {
        // A challenge's game has the same id as the challenge.
        remove(game.value(QStringLiteral("gameId"), game.value(QStringLiteral("id"))).toString());
    });
}

int ChallengesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_challenges.size();
}

QVariant ChallengesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_challenges.size())
        return QVariant();
    const QVariantMap &c = m_challenges.at(index.row());
    const QVariantMap opponent = opponentOf(c);
    const bool incoming = str(c, "direction") == QLatin1String("in");

    switch (role) {
    case ChallengeIdRole: return str(c, "id");
    case DirectionRole: return str(c, "direction");
    case OpponentNameRole: {
        const QString name = str(opponent, "name");
        return name.isEmpty() ? tr("Open challenge") : name;
    }
    case OpponentRatingRole: return opponent.value(QStringLiteral("rating")).toInt();
    case OpponentTitleRole: return str(opponent, "title");
    case OpponentOnlineRole: return opponent.value(QStringLiteral("online")).toBool();
    case TimeControlRole: return describeTimeControl(c);
    case SpeedRole: return str(c, "speed");
    case PerfNameRole: return str(c.value(QStringLiteral("perf")).toMap(), "name");
    case RatedRole: return c.value(QStringLiteral("rated")).toBool();
    case VariantRole: return str(c.value(QStringLiteral("variant")).toMap(), "name");
    case MyColorRole: return incoming ? invertColor(str(c, "color")) : str(c, "color");
    case UrlRole: return str(c, "url");
    default: return QVariant();
    }
}

QHash<int, QByteArray> ChallengesModel::roleNames() const
{
    return {
        { ChallengeIdRole, "challengeId" },
        { DirectionRole, "direction" },
        { OpponentNameRole, "opponentName" },
        { OpponentRatingRole, "opponentRating" },
        { OpponentTitleRole, "opponentTitle" },
        { OpponentOnlineRole, "opponentOnline" },
        { TimeControlRole, "timeControl" },
        { SpeedRole, "speed" },
        { PerfNameRole, "perfName" },
        { RatedRole, "rated" },
        { VariantRole, "variant" },
        { MyColorRole, "myColor" },
        { UrlRole, "url" },
    };
}

int ChallengesModel::incomingCount() const
{
    int n = 0;
    for (const QVariantMap &c : m_challenges) {
        if (str(c, "direction") == QLatin1String("in"))
            ++n;
    }
    return n;
}

void ChallengesModel::refresh()
{
    if (!m_api->hasToken())
        return;
    m_api->get(QStringLiteral("/api/challenge"), QUrlQuery(), this, [this](const ApiResult &result) {
        if (!result.ok())
            return;
        const QJsonObject body = result.json.object();
        for (const char *direction : { "in", "out" }) {
            for (const QJsonValue &value : body.value(QLatin1String(direction)).toArray()) {
                QVariantMap challenge = value.toObject().toVariantMap();
                challenge.insert(QStringLiteral("direction"), QString::fromLatin1(direction));
                upsert(challenge);
            }
        }
    });
}

void ChallengesModel::accept(const QString &challengeId)
{
    m_api->postForm(QStringLiteral("/api/challenge/%1/accept").arg(challengeId), QUrlQuery(), this,
                    [this, challengeId](const ApiResult &result) {
        if (!result.ok()) {
            emit errorOccurred(result.errorString);
            refresh();
            return;
        }
        remove(challengeId);
        emit challengeAccepted(challengeId);
    });
}

void ChallengesModel::decline(const QString &challengeId, const QString &reason)
{
    QUrlQuery form;
    if (!reason.isEmpty())
        form.addQueryItem(QStringLiteral("reason"), reason);
    m_api->postForm(QStringLiteral("/api/challenge/%1/decline").arg(challengeId), form, this,
                    [this, challengeId](const ApiResult &result) {
        if (!result.ok())
            emit errorOccurred(result.errorString);
        remove(challengeId);
    });
}

void ChallengesModel::cancel(const QString &challengeId)
{
    m_api->postForm(QStringLiteral("/api/challenge/%1/cancel").arg(challengeId), QUrlQuery(), this,
                    [this, challengeId](const ApiResult &result) {
        if (!result.ok())
            emit errorOccurred(result.errorString);
        remove(challengeId);
    });
}

void ChallengesModel::clear()
{
    beginResetModel();
    m_challenges.clear();
    m_notified.clear();
    endResetModel();
    emit countChanged();
}

QString ChallengesModel::describeTimeControl(const QVariantMap &challenge)
{
    const QVariantMap tc = challenge.value(QStringLiteral("timeControl")).toMap();
    const QString type = str(tc, "type");
    if (type == QLatin1String("clock")) {
        const QString show = str(tc, "show");
        if (!show.isEmpty())
            return show;
        return QStringLiteral("%1+%2").arg(tc.value(QStringLiteral("limit")).toInt() / 60)
                .arg(tc.value(QStringLiteral("increment")).toInt());
    }
    if (type == QLatin1String("correspondence")) {
        const int days = tc.value(QStringLiteral("daysPerTurn")).toInt();
        return days == 1 ? tr("1 day") : tr("%1 days").arg(days);
    }
    return tr("Unlimited");
}

int ChallengesModel::indexOf(const QString &id) const
{
    for (int i = 0; i < m_challenges.size(); ++i) {
        if (str(m_challenges.at(i), "id") == id)
            return i;
    }
    return -1;
}

void ChallengesModel::upsert(const QVariantMap &data)
{
    const QString id = str(data, "id");
    if (id.isEmpty())
        return;
    QVariantMap challenge = data;
    if (str(challenge, "direction").isEmpty()) {
        const QString challenger = str(challenge.value(QStringLiteral("challenger")).toMap(), "id");
        const bool mine = Services::session() && challenger == Services::session()->userId();
        challenge.insert(QStringLiteral("direction"), mine ? QStringLiteral("out") : QStringLiteral("in"));
    }
    const QString status = str(challenge, "status");
    if (!status.isEmpty() && status != QLatin1String("created") && status != QLatin1String("offline")) {
        remove(id);
        return;
    }

    const int row = indexOf(id);
    if (row >= 0) {
        m_challenges[row] = challenge;
        const QModelIndex idx = index(row);
        emit dataChanged(idx, idx);
    } else {
        beginInsertRows(QModelIndex(), m_challenges.size(), m_challenges.size());
        m_challenges.append(challenge);
        endInsertRows();
        emit countChanged();
    }

    if (str(challenge, "direction") == QLatin1String("in") && !m_notified.contains(id)) {
        m_notified.insert(id);
        const QVariantMap opponent = opponentOf(challenge);
        const QString perf = str(challenge.value(QStringLiteral("perf")).toMap(), "name");
        const QString rated = challenge.value(QStringLiteral("rated")).toBool() ? tr("Rated") : tr("Casual");
        emit newIncomingChallenge(id, str(opponent, "name"),
                                  QStringLiteral("%1 • %2 • %3").arg(describeTimeControl(challenge), perf, rated));
    }
}

void ChallengesModel::remove(const QString &id)
{
    const int row = indexOf(id);
    if (row < 0)
        return;
    beginRemoveRows(QModelIndex(), row, row);
    m_challenges.remove(row);
    endRemoveRows();
    emit countChanged();
}
