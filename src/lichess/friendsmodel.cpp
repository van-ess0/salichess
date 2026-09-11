// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "friendsmodel.h"

#include "core/lichessapi.h"
#include "core/ndjsonstream.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>

namespace {
const int StatusBatch = 100; // max ids per /api/users/status call

int perfRating(const QJsonObject &user, const char *perf)
{
    return user.value(QStringLiteral("perfs")).toObject().value(QLatin1String(perf)).toObject()
            .value(QStringLiteral("rating")).toInt();
}
}

FriendsModel::FriendsModel(LichessApi *api, QObject *parent)
    : QAbstractListModel(parent)
    , m_api(api)
{
}

int FriendsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_friends.size();
}

QVariant FriendsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_friends.size())
        return QVariant();
    const Friend &f = m_friends.at(index.row());
    switch (role) {
    case UserIdRole: return f.id;
    case UsernameRole: return f.name;
    case TitleRole: return f.title;
    case OnlineRole: return f.online;
    case PlayingRole: return f.playing;
    case BlitzRatingRole: return f.blitz;
    case RapidRatingRole: return f.rapid;
    case ClassicalRatingRole: return f.classical;
    default: return QVariant();
    }
}

QHash<int, QByteArray> FriendsModel::roleNames() const
{
    return {
        { UserIdRole, "userId" },
        { UsernameRole, "username" },
        { TitleRole, "title" },
        { OnlineRole, "online" },
        { PlayingRole, "playing" },
        { BlitzRatingRole, "blitzRating" },
        { RapidRatingRole, "rapidRating" },
        { ClassicalRatingRole, "classicalRating" },
    };
}

void FriendsModel::refresh()
{
    if (!m_api->hasToken() || m_loading)
        return;
    setLoading(true);
    m_pending.clear();

    m_stream = m_api->openStream(QStringLiteral("/api/rel/following"));
    m_stream->setParent(this);
    connect(m_stream.data(), &NdjsonStream::message, this, [this](const QJsonObject &user) {
        Friend f;
        f.id = user.value(QStringLiteral("id")).toString();
        f.name = user.value(QStringLiteral("username")).toString();
        f.title = user.value(QStringLiteral("title")).toString();
        f.blitz = perfRating(user, "blitz");
        f.rapid = perfRating(user, "rapid");
        f.classical = perfRating(user, "classical");
        if (!f.id.isEmpty())
            m_pending.append(f);
    });
    connect(m_stream.data(), &NdjsonStream::finished, this, [this](int status, const QString &) {
        m_stream->deleteLater();
        m_stream.clear();
        if (status < 200 || status >= 300) {
            setLoading(false);
            return;
        }
        fetchStatuses(0);
    });
}

void FriendsModel::clear()
{
    if (m_stream) {
        m_stream->abort();
        m_stream->deleteLater();
        m_stream.clear();
    }
    m_pending.clear();
    beginResetModel();
    m_friends.clear();
    endResetModel();
    setLoading(false);
    emit countChanged();
}

void FriendsModel::fetchStatuses(int from)
{
    if (from >= m_pending.size()) {
        publish();
        return;
    }
    QStringList ids;
    for (int i = from; i < m_pending.size() && i < from + StatusBatch; ++i)
        ids.append(m_pending.at(i).id);

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("ids"), ids.join(QLatin1Char(',')));
    m_api->get(QStringLiteral("/api/users/status"), query, this, [this, from](const ApiResult &result) {
        if (result.ok()) {
            for (const QJsonValue &value : result.json.array()) {
                const QJsonObject status = value.toObject();
                const QString id = status.value(QStringLiteral("id")).toString();
                for (Friend &f : m_pending) {
                    if (f.id == id) {
                        f.online = status.value(QStringLiteral("online")).toBool();
                        f.playing = status.value(QStringLiteral("playing")).toBool();
                        break;
                    }
                }
            }
        }
        fetchStatuses(from + StatusBatch);
    });
}

void FriendsModel::publish()
{
    std::sort(m_pending.begin(), m_pending.end(), [](const Friend &a, const Friend &b) {
        if (a.online != b.online)
            return a.online;
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    beginResetModel();
    m_friends = m_pending;
    endResetModel();
    m_pending.clear();
    setLoading(false);
    emit countChanged();
}

void FriendsModel::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}
