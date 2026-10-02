// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "playersearch.h"

#include "core/lichessapi.h"
#include "core/services.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUrlQuery>

namespace {

// Lichess answers from two characters on.
const int MinTermLength = 2;
// Not a request per keystroke.
const int TypingDelayMs = 350;

} // namespace

PlayerSearch::PlayerSearch(QObject *parent)
    : QObject(parent)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(TypingDelayMs);
    connect(&m_debounce, &QTimer::timeout, this, &PlayerSearch::search);
}

void PlayerSearch::setTerm(const QString &term)
{
    const QString trimmed = term.trimmed();
    if (m_term == trimmed)
        return;
    m_term = trimmed;
    emit termChanged();

    // Answers to an earlier term are no longer wanted.
    ++m_generation;
    if (m_term.size() < MinTermLength) {
        m_debounce.stop();
        setLoading(false);
        if (!m_results.isEmpty()) {
            m_results.clear();
            emit resultsChanged();
        }
        if (!m_error.isEmpty()) {
            m_error.clear();
            emit errorStringChanged();
        }
        return;
    }
    m_debounce.start();
}

void PlayerSearch::search()
{
    if (!Services::api())
        return;
    const int generation = m_generation;
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("term"), m_term);
    query.addQueryItem(QStringLiteral("object"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("names"), QStringLiteral("10"));
    setLoading(true);
    Services::api()->get(QStringLiteral("/api/player/autocomplete"), query, this,
                         [this, generation](const ApiResult &result) {
        if (generation != m_generation)
            return;
        setLoading(false);
        const QString error = result.ok() ? QString() : result.errorString;
        if (error != m_error) {
            m_error = error;
            emit errorStringChanged();
        }
        m_results.clear();
        if (result.ok()) {
            const QJsonArray found = result.json.object().value(QStringLiteral("result")).toArray();
            for (const QJsonValue &value : found) {
                const QJsonObject user = value.toObject();
                QVariantMap row;
                row.insert(QStringLiteral("id"), user.value(QStringLiteral("id")).toString());
                row.insert(QStringLiteral("name"), user.value(QStringLiteral("name")).toString());
                row.insert(QStringLiteral("title"), user.value(QStringLiteral("title")).toString());
                row.insert(QStringLiteral("online"), user.value(QStringLiteral("online")).toBool());
                row.insert(QStringLiteral("patron"), user.value(QStringLiteral("patron")).toBool());
                m_results.append(row);
            }
        }
        emit resultsChanged();
    });
}

void PlayerSearch::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}
