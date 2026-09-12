// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PUZZLESTORE_H
#define PUZZLESTORE_H

#include <QJsonObject>
#include <QObject>
#include <QVector>

class AppSettings;
class LichessApi;
class Session;

// Puzzles kept on the phone so they can be played without a connection, and
// the results that still have to be sent to Lichess. Exposed to QML as
// "puzzleStore".
//
// The pool holds the healthy mix at the difficulty from the settings, which
// is what "Puzzles" plays; a pool for another difficulty is dropped. It is
// topped up to appSettings.offlinePuzzles whenever the app is online, so the
// puzzles that are played come out of it and the pool stays full.
class PuzzleStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(int target READ target NOTIFY changed)
    Q_PROPERTY(bool filling READ filling NOTIFY changed)
    // Results waiting for a connection.
    Q_PROPERTY(int pendingResults READ pendingResults NOTIFY changed)
public:
    PuzzleStore(LichessApi *api, Session *session, AppSettings *settings, QObject *parent = nullptr);

    int count() const { return m_puzzles.size(); }
    int target() const;
    bool filling() const { return m_filling; }
    int pendingResults() const { return m_results.size(); }

    // The angle the pool is filled for; the only one that can be played
    // without a connection.
    static QString angle();

    // The next stored puzzle, or an empty object if |angle|/|difficulty| are
    // not what the pool holds, or it is empty. Tops the pool up again.
    QJsonObject take(const QString &angle, const QString &difficulty);
    // Keeps a played puzzle until Lichess can be told about it.
    void queueResult(const QString &angle, const QString &puzzleId, bool win, bool rated);

    // Downloads what is missing, if the app is online. Called on its own
    // whenever a puzzle is taken or the settings change.
    Q_INVOKABLE void refill();
    // Forgets the stored puzzles (not the results).
    Q_INVOKABLE void clear();

signals:
    void changed();

private:
    void load();
    void save();
    void flushResults();
    void onDifficultyChanged();

    LichessApi *m_api;
    Session *m_session;
    AppSettings *m_settings;
    QString m_difficulty;  // what the pool was downloaded for
    QVector<QJsonObject> m_puzzles;
    QVector<QJsonObject> m_results;
    bool m_filling = false;
    bool m_flushing = false;
};

#endif // PUZZLESTORE_H
