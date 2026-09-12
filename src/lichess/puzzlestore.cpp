// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "puzzlestore.h"

#include "core/appsettings.h"
#include "core/lichessapi.h"
#include "core/session.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QUrlQuery>

namespace {

// Lichess hands out at most this many puzzles per request.
const int MaxBatch = 50;

QString storePath()
{
    // Sailjail only lets the app write inside its own data directory.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/offline-puzzles.json");
}

QString puzzleId(const QJsonObject &puzzleAndGame)
{
    return puzzleAndGame.value(QStringLiteral("puzzle")).toObject()
            .value(QStringLiteral("id")).toString();
}

} // namespace

PuzzleStore::PuzzleStore(LichessApi *api, Session *session, AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_session(session)
    , m_settings(settings)
{
    load();

    connect(m_api, &LichessApi::offlineChanged, this, [this]() {
        if (m_api->offline())
            return;
        flushResults();
        refill();
    });
    connect(m_settings, &AppSettings::puzzleDifficultyChanged, this, &PuzzleStore::onDifficultyChanged);
    connect(m_settings, &AppSettings::offlinePuzzlesChanged, this, [this]() {
        if (m_puzzles.size() > target()) {
            m_puzzles.resize(target());
            save();
            emit changed();
        }
        refill();
    });
    connect(m_session, &Session::loggedInChanged, this, [this]() {
        if (!m_session->loggedIn()) {
            // Results belong to the account that played them.
            if (!m_results.isEmpty()) {
                m_results.clear();
                save();
                emit changed();
            }
            return;
        }
        flushResults();
        refill();
    });
}

QString PuzzleStore::angle()
{
    return QStringLiteral("mix");
}

int PuzzleStore::target() const
{
    return m_settings->offlinePuzzles();
}

QJsonObject PuzzleStore::take(const QString &angle, const QString &difficulty)
{
    if (angle != PuzzleStore::angle() || difficulty != m_difficulty || m_puzzles.isEmpty())
        return QJsonObject();
    const QJsonObject puzzle = m_puzzles.takeFirst();
    save();
    emit changed();
    refill();
    return puzzle;
}

void PuzzleStore::queueResult(const QString &angle, const QString &puzzleId, bool win, bool rated)
{
    if (puzzleId.isEmpty() || !m_session->loggedIn())
        return;
    QJsonObject result;
    result.insert(QStringLiteral("angle"), angle);
    result.insert(QStringLiteral("id"), puzzleId);
    result.insert(QStringLiteral("win"), win);
    result.insert(QStringLiteral("rated"), rated);
    m_results.append(result);
    save();
    emit changed();
    flushResults();
}

void PuzzleStore::refill()
{
    const int missing = target() - m_puzzles.size();
    if (m_filling || missing <= 0 || m_api->offline())
        return;

    // A pool for another difficulty is of no use: the puzzles would be too
    // easy or too hard for what the user asked for.
    if (m_difficulty != m_settings->puzzleDifficulty()) {
        m_difficulty = m_settings->puzzleDifficulty();
        m_puzzles.clear();
    }

    m_filling = true;
    emit changed();

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("nb"), QString::number(qMin(missing, MaxBatch)));
    if (!m_difficulty.isEmpty())
        query.addQueryItem(QStringLiteral("difficulty"), m_difficulty);
    const QString difficulty = m_difficulty;
    m_api->get(QStringLiteral("/api/puzzle/batch/") + angle(), query, this,
               [this, difficulty](const ApiResult &result) {
        m_filling = false;
        // The difficulty can change while a batch is on its way; those
        // puzzles are then of no use, and the pool has to be filled anew.
        const bool stale = difficulty != m_difficulty;
        if (!result.ok() || stale) {
            emit changed();
            if (stale)
                refill();
            return; // otherwise the next take() or connection tries again
        }
        QStringList known;
        for (const QJsonObject &puzzle : m_puzzles)
            known.append(puzzleId(puzzle));
        int added = 0;
        for (const QJsonValue &value : result.json.object().value(QStringLiteral("puzzles")).toArray()) {
            const QJsonObject puzzle = value.toObject();
            const QString id = puzzleId(puzzle);
            if (id.isEmpty() || known.contains(id))
                continue;
            known.append(id);
            m_puzzles.append(puzzle);
            ++added;
        }
        if (added > 0)
            save();
        emit changed();
        // Lichess caps a batch; keep going for a larger pool, but stop when
        // it has no more to give.
        if (added > 0)
            refill();
    });
}

void PuzzleStore::clear()
{
    if (m_puzzles.isEmpty())
        return;
    m_puzzles.clear();
    save();
    emit changed();
}

void PuzzleStore::flushResults()
{
    if (m_flushing || m_results.isEmpty() || m_api->offline() || !m_session->loggedIn())
        return;

    // Every result of one angle goes in a single request; in practice they
    // all come from the stored mix.
    const QString angle = m_results.first().value(QStringLiteral("angle")).toString();
    QJsonArray solutions;
    QStringList sent;
    for (const QJsonObject &result : m_results) {
        if (result.value(QStringLiteral("angle")).toString() != angle)
            continue;
        QJsonObject solution;
        solution.insert(QStringLiteral("id"), result.value(QStringLiteral("id")));
        solution.insert(QStringLiteral("win"), result.value(QStringLiteral("win")));
        solution.insert(QStringLiteral("rated"), result.value(QStringLiteral("rated")));
        solutions.append(solution);
        sent.append(result.value(QStringLiteral("id")).toString());
    }
    QJsonObject body;
    body.insert(QStringLiteral("solutions"), solutions);

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("nb"), QStringLiteral("0"));
    m_flushing = true;
    m_api->postJson(QStringLiteral("/api/puzzle/batch/") + angle, query, QJsonDocument(body), this,
                    [this, sent](const ApiResult &result) {
        m_flushing = false;
        if (!result.ok())
            return; // keep them for the next attempt
        for (int i = m_results.size() - 1; i >= 0; --i) {
            if (sent.contains(m_results.at(i).value(QStringLiteral("id")).toString()))
                m_results.remove(i);
        }
        save();
        emit changed();
        flushResults(); // another angle may be left
    });
}

void PuzzleStore::onDifficultyChanged()
{
    if (m_difficulty == m_settings->puzzleDifficulty())
        return;
    m_difficulty = m_settings->puzzleDifficulty();
    m_puzzles.clear();
    save();
    emit changed();
    refill();
}

void PuzzleStore::load()
{
    QFile file(storePath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject stored = QJsonDocument::fromJson(file.readAll()).object();
    m_difficulty = stored.value(QStringLiteral("difficulty")).toString();
    for (const QJsonValue &value : stored.value(QStringLiteral("puzzles")).toArray())
        m_puzzles.append(value.toObject());
    for (const QJsonValue &value : stored.value(QStringLiteral("results")).toArray())
        m_results.append(value.toObject());
    if (m_difficulty != m_settings->puzzleDifficulty())
        m_puzzles.clear();
}

void PuzzleStore::save()
{
    QJsonArray puzzles;
    for (const QJsonObject &puzzle : m_puzzles)
        puzzles.append(puzzle);
    QJsonArray results;
    for (const QJsonObject &result : m_results)
        results.append(result);

    QJsonObject stored;
    stored.insert(QStringLiteral("difficulty"), m_difficulty);
    stored.insert(QStringLiteral("puzzles"), puzzles);
    stored.insert(QStringLiteral("results"), results);

    QFile file(storePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning("Could not write %s", qPrintable(storePath()));
        return;
    }
    file.write(QJsonDocument(stored).toJson(QJsonDocument::Compact));
}
