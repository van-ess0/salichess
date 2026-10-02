// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "puzzledashboard.h"

#include "core/lichessapi.h"
#include "core/ndjsonstream.h"
#include "core/services.h"
#include "core/session.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUrlQuery>

#include <algorithm>

namespace {

const int RecentPuzzles = 20;

double solvedRate(const QVariant &theme)
{
    const QVariantMap map = theme.toMap();
    const int puzzles = map.value(QStringLiteral("puzzles")).toInt();
    return puzzles > 0 ? double(map.value(QStringLiteral("firstWins")).toInt()) / puzzles : 0;
}

} // namespace

PuzzleDashboard::PuzzleDashboard(QObject *parent)
    : QObject(parent)
{
}

PuzzleDashboard::~PuzzleDashboard()
{
    if (m_stream)
        m_stream->abort();
}

void PuzzleDashboard::setDays(int days)
{
    days = qBound(1, days, 365);
    if (m_days == days)
        return;
    m_days = days;
    emit daysChanged();
    reload();
}

void PuzzleDashboard::reload()
{
    ++m_generation;
    if (m_stream) {
        m_stream->abort();
        m_stream->deleteLater();
        m_stream.clear();
    }
    setError(QString());
    if (!Services::api() || !Services::session() || !Services::session()->loggedIn()) {
        setLoading(false);
        setError(tr("Log in to see your puzzle dashboard"));
        return;
    }

    const int generation = m_generation;
    setLoading(true);
    Services::api()->get(QStringLiteral("/api/puzzle/dashboard/%1").arg(m_days), QUrlQuery(), this,
                         [this, generation](const ApiResult &result) {
        if (generation != m_generation)
            return;
        setLoading(false);
        if (!result.ok()) {
            setError(result.errorString);
            return;
        }
        applyDashboard(result.json.object());
    });
    loadRecent();
}

void PuzzleDashboard::applyDashboard(const QJsonObject &dashboard)
{
    const QJsonObject global = dashboard.value(QStringLiteral("global")).toObject();
    m_puzzleCount = global.value(QStringLiteral("nb")).toInt();
    m_firstWins = global.value(QStringLiteral("firstWins")).toInt();
    m_replayWins = global.value(QStringLiteral("replayWins")).toInt();
    m_performance = global.value(QStringLiteral("performance")).toInt();

    m_themes.clear();
    const QJsonObject themes = dashboard.value(QStringLiteral("themes")).toObject();
    for (auto it = themes.begin(); it != themes.end(); ++it) {
        const QJsonObject entry = it.value().toObject();
        const QJsonObject results = entry.value(QStringLiteral("results")).toObject();
        const int puzzles = results.value(QStringLiteral("nb")).toInt();
        if (puzzles <= 0)
            continue;
        QVariantMap row;
        row.insert(QStringLiteral("theme"), entry.value(QStringLiteral("theme")).toString(it.key()));
        row.insert(QStringLiteral("puzzles"), puzzles);
        row.insert(QStringLiteral("firstWins"), results.value(QStringLiteral("firstWins")).toInt());
        row.insert(QStringLiteral("replayWins"), results.value(QStringLiteral("replayWins")).toInt());
        row.insert(QStringLiteral("performance"), results.value(QStringLiteral("performance")).toInt());
        row.insert(QStringLiteral("solvedPercent"),
                   qRound(100.0 * results.value(QStringLiteral("firstWins")).toInt() / puzzles));
        m_themes.append(row);
    }
    std::stable_sort(m_themes.begin(), m_themes.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("puzzles")).toInt() > b.toMap().value(QStringLiteral("puzzles")).toInt();
    });

    QVariantList ranked;
    for (const QVariant &theme : m_themes) {
        if (theme.toMap().value(QStringLiteral("puzzles")).toInt() >= minPuzzlesForRanking())
            ranked.append(theme);
    }
    std::stable_sort(ranked.begin(), ranked.end(), [](const QVariant &a, const QVariant &b) {
        return solvedRate(a) > solvedRate(b);
    });
    // A theme is not a strength and a weakness at once: with few themes, the
    // best half and the worst half are kept apart.
    const int shown = qMin(3, ranked.size() / 2);
    m_strongest = ranked.mid(0, shown);
    m_weakest.clear();
    for (int i = 0; i < shown; ++i)
        m_weakest.append(ranked.at(ranked.size() - 1 - i));

    emit dashboardChanged();
}

void PuzzleDashboard::loadRecent()
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("max"), QString::number(RecentPuzzles));
    m_pendingRecent.clear();
    const int generation = m_generation;
    m_stream = Services::api()->openStream(QStringLiteral("/api/puzzle/activity"), query);
    m_stream->setParent(this);
    connect(m_stream.data(), &NdjsonStream::message, this, [this, generation](const QJsonObject &entry) {
        if (generation != m_generation)
            return;
        const QJsonObject puzzle = entry.value(QStringLiteral("puzzle")).toObject();
        QVariantMap row;
        row.insert(QStringLiteral("id"), puzzle.value(QStringLiteral("id")).toString());
        row.insert(QStringLiteral("win"), entry.value(QStringLiteral("win")).toBool());
        row.insert(QStringLiteral("rating"), puzzle.value(QStringLiteral("rating")).toInt());
        row.insert(QStringLiteral("date"), qint64(entry.value(QStringLiteral("date")).toDouble()));
        row.insert(QStringLiteral("themes"), puzzle.value(QStringLiteral("themes")).toVariant());
        m_pendingRecent.append(row);
    });
    connect(m_stream.data(), &NdjsonStream::finished, this, [this, generation](int, const QString &) {
        if (m_stream)
            m_stream->deleteLater();
        m_stream.clear();
        if (generation != m_generation)
            return;
        // Failing to get the list leaves the dashboard as it is.
        m_recent = m_pendingRecent;
        emit recentChanged();
    });
}

void PuzzleDashboard::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

void PuzzleDashboard::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorStringChanged();
}
