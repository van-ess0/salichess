// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PUZZLEDASHBOARD_H
#define PUZZLEDASHBOARD_H

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class NdjsonStream;

// How the user has done at puzzles lately: totals and the themes they are
// best and worst at over a number of days (/api/puzzle/dashboard/{days}),
// plus the puzzles they played last (/api/puzzle/activity). Needs a login.
// Created in QML:
//     PuzzleDashboard { days: 30 }
class PuzzleDashboard : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int days READ days WRITE setDays NOTIFY daysChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

    Q_PROPERTY(int puzzleCount READ puzzleCount NOTIFY dashboardChanged)
    Q_PROPERTY(int firstWins READ firstWins NOTIFY dashboardChanged) // solved at the first try
    Q_PROPERTY(int replayWins READ replayWins NOTIFY dashboardChanged)
    Q_PROPERTY(int performance READ performance NOTIFY dashboardChanged)
    // Maps of "theme", "puzzles", "firstWins", "replayWins", "performance"
    // and "solvedPercent", the most played first.
    Q_PROPERTY(QVariantList themes READ themes NOTIFY dashboardChanged)
    // The themes with the best and the worst first-try rates, among those
    // played often enough to mean something.
    Q_PROPERTY(QVariantList strongest READ strongest NOTIFY dashboardChanged)
    Q_PROPERTY(QVariantList weakest READ weakest NOTIFY dashboardChanged)
    // The puzzles played last, newest first: "id", "win", "rating", "date"
    // (ms), "themes".
    Q_PROPERTY(QVariantList recent READ recent NOTIFY recentChanged)

public:
    explicit PuzzleDashboard(QObject *parent = nullptr);
    ~PuzzleDashboard() override;

    int days() const { return m_days; }
    void setDays(int days);
    bool loading() const { return m_loading; }
    QString errorString() const { return m_error; }

    int puzzleCount() const { return m_puzzleCount; }
    int firstWins() const { return m_firstWins; }
    int replayWins() const { return m_replayWins; }
    int performance() const { return m_performance; }
    QVariantList themes() const { return m_themes; }
    QVariantList strongest() const { return m_strongest; }
    QVariantList weakest() const { return m_weakest; }
    QVariantList recent() const { return m_recent; }

    Q_INVOKABLE void reload();

    // Themes with fewer puzzles than this are left out of the lists of
    // strengths and weaknesses.
    static int minPuzzlesForRanking() { return 5; }

signals:
    void daysChanged();
    void loadingChanged();
    void errorStringChanged();
    void dashboardChanged();
    void recentChanged();

private:
    void applyDashboard(const QJsonObject &dashboard);
    void loadRecent();
    void setLoading(bool loading);
    void setError(const QString &error);

    int m_days = 30;
    bool m_loading = false;
    QString m_error;
    int m_generation = 0;
    int m_puzzleCount = 0;
    int m_firstWins = 0;
    int m_replayWins = 0;
    int m_performance = 0;
    QVariantList m_themes;
    QVariantList m_strongest;
    QVariantList m_weakest;
    QVariantList m_recent;
    QVariantList m_pendingRecent;
    QPointer<NdjsonStream> m_stream;
};

#endif // PUZZLEDASHBOARD_H
