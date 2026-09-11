// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FRIENDSMODEL_H
#define FRIENDSMODEL_H

#include <QAbstractListModel>
#include <QPointer>
#include <QVariantMap>
#include <QVector>

class LichessApi;
class NdjsonStream;

// Players the user follows, with online status; online players first.
// Exposed to QML as "friends".
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
        ClassicalRatingRole
    };

    explicit FriendsModel(LichessApi *api, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_friends.size(); }
    bool loading() const { return m_loading; }

    Q_INVOKABLE void refresh();
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
    };

    void fetchStatuses(int from);
    void publish();
    void setLoading(bool loading);

    LichessApi *m_api;
    QPointer<NdjsonStream> m_stream;
    QVector<Friend> m_pending;
    QVector<Friend> m_friends;
    bool m_loading = false;
};

#endif // FRIENDSMODEL_H
