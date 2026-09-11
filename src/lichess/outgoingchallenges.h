// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OUTGOINGCHALLENGES_H
#define OUTGOINGCHALLENGES_H

#include <QList>
#include <QObject>
#include <QVariantMap>

class EventStream;
class LichessApi;
class OutgoingChallenge;

// Challenges sent from the app. Each one keeps its own keep-alive request
// open, so they stay valid while the user does something else; the manager
// reports their outcome app-wide. Exposed to QML as "outgoingChallenges".
class OutgoingChallenges : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int pendingCount READ pendingCount NOTIFY pendingCountChanged)
public:
    OutgoingChallenges(LichessApi *api, EventStream *events, QObject *parent = nullptr);

    int pendingCount() const;

    // Sends a challenge; see OutgoingChallenge::create() for |options|.
    Q_INVOKABLE OutgoingChallenge *create(const QString &username, const QVariantMap &options);
    // The pending challenge with this id, or null.
    Q_INVOKABLE OutgoingChallenge *find(const QString &challengeId) const;
    void cancelAll();

signals:
    void pendingCountChanged();
    void challengeAccepted(const QString &gameId, const QString &opponent);
    void challengeDeclined(const QString &challengeId, const QString &opponent, const QString &reason);
    void challengeFailed(const QString &opponent, const QString &error);
    // Any final outcome, so the challenge list can drop the entry.
    void challengeFinished(const QString &challengeId);

private:
    void onStateChanged(OutgoingChallenge *challenge);
    void prune();

    LichessApi *m_api;
    EventStream *m_events;
    QList<OutgoingChallenge *> m_challenges;
};

#endif // OUTGOINGCHALLENGES_H
