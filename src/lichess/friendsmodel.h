// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FRIENDSMODEL_H
#define FRIENDSMODEL_H

#include <QAbstractListModel>
#include <QPointer>
#include <QVariantMap>
#include <QVector>

class LichessApi;
class Session;
class NdjsonStream;

// Players the user follows, with online status; online players first. Each
// also has the number of games the user has played against them (-1 while
// unknown), the time of the latest such game and the time they were last on
// Lichess, in ms since the epoch (0 while unknown). The followed list doesn't
// carry the games, so with refresh(true) they are fetched once it is shown, one
// friend at a time, until stopDetails() or the next refresh. Exposed to QML as
// "friends".
class FriendsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
public:
    enum Roles {
        UserIdRole = Qt::UserRole + 1,
        UsernameRole,
        TitleRole,
        OnlineRole,
        PlayingRole,
        BlitzRatingRole,
        RapidRatingRole,
        ClassicalRatingRole,
        GamesCountRole,
        LastGameRole,
        SeenAtRole
    };

    FriendsModel(LichessApi *api, Session *session, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_friends.size(); }
    bool loading() const { return m_loading; }

    Q_INVOKABLE void refresh(bool withDetails = false);
    Q_INVOKABLE void stopDetails();
    void clear();

signals:
    void countChanged();
    void loadingChanged();

private:
    struct Friend {
        QString id;
        QString name;
        QString title;
        bool online = false;
        bool playing = false;
        int blitz = 0;
        int rapid = 0;
        int classical = 0;
        int games = -1;
        qint64 lastGameAt = 0;
        qint64 seenAt = 0;
    };

    void fetchStatuses(int from, int generation);
    void publish();
    void fetchDetails(int index, int generation);
    void fetchLastGame(int index, int generation);
    void abortGameStream();
    void setLoading(bool loading);

    LichessApi *m_api;
    Session *m_session;
    QPointer<NdjsonStream> m_stream;
    QPointer<NdjsonStream> m_gameStream;
    // Bumped whenever the list is replaced, so a late answer for an old list is dropped.
    int m_generation = 0;
    QVector<Friend> m_pending;
    QVector<Friend> m_friends;
    bool m_loading = false;
    bool m_withDetails = false;
};

#endif // FRIENDSMODEL_H
