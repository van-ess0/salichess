// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "userprofile.h"

#include "core/lichessapi.h"
#include "core/services.h"

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrlQuery>

namespace {

struct PerfName {
    const char *key;
    const char *name;
};

// The order Lichess lists them in; ratings that have never been played are
// left out. Variants are shown too: a profile is about the player.
const PerfName Perfs[] = {
    {"ultraBullet", "UltraBullet"}, {"bullet", "Bullet"}, {"blitz", "Blitz"}, {"rapid", "Rapid"},
    {"classical", "Classical"}, {"correspondence", "Correspondence"}, {"chess960", "Chess960"},
    {"crazyhouse", "Crazyhouse"}, {"antichess", "Antichess"}, {"atomic", "Atomic"},
    {"horde", "Horde"}, {"kingOfTheHill", "King of the Hill"}, {"racingKings", "Racing Kings"},
    {"threeCheck", "Three-check"}, {"puzzle", "Puzzles"},
};

} // namespace

UserProfile::UserProfile(QObject *parent)
    : QObject(parent)
{
}

void UserProfile::setUsername(const QString &username)
{
    const QString trimmed = username.trimmed();
    if (m_username == trimmed)
        return;
    m_username = trimmed;
    emit usernameChanged();
    reload();
}

void UserProfile::clear()
{
    m_loaded = false;
    m_name.clear();
    m_title.clear();
    m_online = m_playing = m_patron = m_closed = false;
    m_bio.clear();
    m_location.clear();
    m_createdAt = m_seenAt = 0;
    m_playTime = m_followers = 0;
    m_counts.clear();
    m_ratings.clear();
    m_historyNames.clear();
    m_historyPoints.clear();
    emit profileChanged();
    emit historyChanged();
}

void UserProfile::reload()
{
    ++m_generation;
    clear();
    setError(QString());
    static const QRegularExpression validName(QStringLiteral("^[A-Za-z0-9_-]{2,30}$"));
    if (m_username.isEmpty() || !Services::api()) {
        setLoading(false);
        return;
    }
    if (!validName.match(m_username).hasMatch()) {
        setLoading(false);
        setError(tr("\"%1\" is not a valid Lichess username.").arg(m_username));
        return;
    }

    const int generation = m_generation;
    setLoading(true);
    Services::api()->get(QStringLiteral("/api/user/") + m_username, QUrlQuery(), this,
                         [this, generation](const ApiResult &result) {
        if (generation != m_generation)
            return;
        setLoading(false);
        if (!result.ok()) {
            setError(result.status == 404 ? tr("No such player") : result.errorString);
            return;
        }
        applyProfile(result.json.object());
        // The graph is a bonus: failing to get it leaves the rest as it is.
        Services::api()->get(QStringLiteral("/api/user/%1/rating-history").arg(m_username), QUrlQuery(), this,
                             [this, generation](const ApiResult &history) {
            if (generation == m_generation && history.ok())
                applyHistory(history.json.array());
        });
    });
}

void UserProfile::applyProfile(const QJsonObject &user)
{
    m_name = user.value(QStringLiteral("username")).toString(m_username);
    m_title = user.value(QStringLiteral("title")).toString();
    m_online = user.value(QStringLiteral("online")).toBool();
    m_playing = user.value(QStringLiteral("playing")).toBool();
    m_patron = user.value(QStringLiteral("patron")).toBool();
    m_closed = user.value(QStringLiteral("disabled")).toBool() || user.value(QStringLiteral("tosViolation")).toBool();
    m_createdAt = qint64(user.value(QStringLiteral("createdAt")).toDouble());
    m_seenAt = qint64(user.value(QStringLiteral("seenAt")).toDouble());
    m_playTime = user.value(QStringLiteral("playTime")).toObject().value(QStringLiteral("total")).toInt();
    m_followers = user.value(QStringLiteral("nbFollowers")).toInt();

    const QJsonObject profile = user.value(QStringLiteral("profile")).toObject();
    m_bio = profile.value(QStringLiteral("bio")).toString();
    m_location = profile.value(QStringLiteral("location")).toString();

    const QJsonObject count = user.value(QStringLiteral("count")).toObject();
    m_counts.clear();
    for (const char *key : {"all", "rated", "win", "loss", "draw", "ai"})
        m_counts.insert(QString::fromLatin1(key), count.value(QString::fromLatin1(key)).toInt());

    m_ratings.clear();
    const QJsonObject perfs = user.value(QStringLiteral("perfs")).toObject();
    for (const PerfName &perf : Perfs) {
        const QJsonObject p = perfs.value(QString::fromLatin1(perf.key)).toObject();
        const int games = p.value(QStringLiteral("games")).toInt();
        // Puzzles count "runs", not games; rating alone says it was played.
        if (games <= 0 && QLatin1String(perf.key) != QLatin1String("puzzle"))
            continue;
        if (!p.contains(QStringLiteral("rating")) || (games <= 0 && p.value(QStringLiteral("rating")).toInt() == 0))
            continue;
        QVariantMap row;
        row.insert(QStringLiteral("key"), QString::fromLatin1(perf.key));
        row.insert(QStringLiteral("name"), QString::fromLatin1(perf.name));
        row.insert(QStringLiteral("rating"), p.value(QStringLiteral("rating")).toInt());
        row.insert(QStringLiteral("games"), games);
        row.insert(QStringLiteral("prog"), p.value(QStringLiteral("prog")).toInt());
        row.insert(QStringLiteral("provisional"), p.value(QStringLiteral("prov")).toBool());
        m_ratings.append(row);
    }
    m_loaded = true;
    emit profileChanged();
}

void UserProfile::applyHistory(const QJsonArray &history)
{
    m_historyNames.clear();
    m_historyPoints.clear();
    for (const QJsonValue &value : history) {
        const QJsonObject perf = value.toObject();
        const QJsonArray points = perf.value(QStringLiteral("points")).toArray();
        if (points.size() < 2)
            continue;
        QVariantList list;
        list.reserve(points.size());
        for (const QJsonValue &point : points) {
            const QJsonArray p = point.toArray();
            if (p.size() < 4)
                continue;
            // The month counts from 0, as in JavaScript.
            const QDateTime when(QDate(p.at(0).toInt(), p.at(1).toInt() + 1, p.at(2).toInt()), QTime(0, 0),
                                 Qt::UTC);
            list.append(QVariant(QVariantList{when.toMSecsSinceEpoch(), p.at(3).toInt()}));
        }
        if (list.size() < 2)
            continue;
        m_historyNames.append(perf.value(QStringLiteral("name")).toString());
        m_historyPoints.append(list);
    }
    emit historyChanged();
}

QVariantList UserProfile::historyPoints(const QString &name) const
{
    const int i = m_historyNames.indexOf(name);
    return i < 0 ? QVariantList() : m_historyPoints.at(i);
}

void UserProfile::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

void UserProfile::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorStringChanged();
}
