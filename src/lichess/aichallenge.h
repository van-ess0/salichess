// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AICHALLENGE_H
#define AICHALLENGE_H

#include <QObject>
#include <QString>

// Starts a game against the Lichess AI (/api/challenge/ai). The game is then
// played like any other Board API game, through GameController. Created in
// QML:
//     AiChallenge { onStarted: ... }
class AiChallenge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

public:
    explicit AiChallenge(QObject *parent = nullptr);

    bool busy() const { return m_busy; }
    QString errorString() const { return m_error; }

    // |level| is 1 to 8, |color| "white", "black" or "random", |seconds| the
    // initial time and |increment| the Fischer increment, both in seconds.
    Q_INVOKABLE void start(int level, const QString &color, int seconds, int increment);

signals:
    void busyChanged();
    void errorStringChanged();
    void started(const QString &gameId);
    void failed(const QString &error);

private:
    void setBusy(bool busy);

    bool m_busy = false;
    QString m_error;
};

#endif // AICHALLENGE_H
