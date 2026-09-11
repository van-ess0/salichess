// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CHALLENGESMODEL_H
#define CHALLENGESMODEL_H

#include <QAbstractListModel>
#include <QSet>
#include <QVariantMap>
#include <QVector>

class EventStream;
class LichessApi;

// Pending challenges, incoming and outgoing. Exposed to QML as "challenges".
class ChallengesModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int incomingCount READ incomingCount NOTIFY countChanged)
public:
    enum Roles {
        ChallengeIdRole = Qt::UserRole + 1,
        DirectionRole,        // "in" or "out"
        OpponentNameRole,
        OpponentRatingRole,
        OpponentTitleRole,
        OpponentOnlineRole,
        TimeControlRole,      // "10+5", "3 days", ...
        SpeedRole,            // "blitz", "rapid", "classical", "correspondence"
        PerfNameRole,
        RatedRole,
        VariantRole,
        MyColorRole,          // "white", "black" or "random"
        UrlRole
    };

    ChallengesModel(LichessApi *api, EventStream *events, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_challenges.size(); }
    int incomingCount() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void accept(const QString &challengeId);
    // |reason| is a Lichess decline reason key ("generic", "later", ...).
    Q_INVOKABLE void decline(const QString &challengeId, const QString &reason = QString());
    Q_INVOKABLE void cancel(const QString &challengeId);
    void clear();
    // Drops a challenge that ended elsewhere (e.g. sent from the app).
    void removeChallenge(const QString &challengeId) { remove(challengeId); }

    static QString describeTimeControl(const QVariantMap &challenge);

signals:
    void countChanged();
    // A challenge we accepted in the app became a game.
    void challengeAccepted(const QString &gameId);
    // A new incoming challenge (each id is reported once per session).
    void newIncomingChallenge(const QString &challengeId, const QString &opponentName,
                              const QString &description);
    void errorOccurred(const QString &message);

private:
    int indexOf(const QString &id) const;
    void upsert(const QVariantMap &challenge);
    void remove(const QString &id);

    LichessApi *m_api;
    QVector<QVariantMap> m_challenges;
    QSet<QString> m_notified;
};

#endif // CHALLENGESMODEL_H
