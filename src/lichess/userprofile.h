// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef USERPROFILE_H
#define USERPROFILE_H

#include <QObject>
#include <QString>
#include <QVariantList>

// A Lichess player: ratings, game counts and what they say about
// themselves (/api/user/{username}), plus the rating history behind the
// graph. Created in QML:
//     UserProfile { username: "DrNykterstein" }
class UserProfile : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY usernameChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY profileChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

    Q_PROPERTY(QString name READ name NOTIFY profileChanged)
    Q_PROPERTY(QString title READ title NOTIFY profileChanged)
    Q_PROPERTY(bool online READ online NOTIFY profileChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY profileChanged)
    Q_PROPERTY(bool patron READ patron NOTIFY profileChanged)
    // The account was closed or is marked for breaking the rules.
    Q_PROPERTY(bool closed READ closed NOTIFY profileChanged)
    Q_PROPERTY(QString bio READ bio NOTIFY profileChanged)
    Q_PROPERTY(QString location READ location NOTIFY profileChanged)
    Q_PROPERTY(qint64 createdAt READ createdAt NOTIFY profileChanged) // ms since the epoch
    Q_PROPERTY(qint64 seenAt READ seenAt NOTIFY profileChanged)
    Q_PROPERTY(int playTime READ playTime NOTIFY profileChanged)      // seconds
    Q_PROPERTY(int followers READ followers NOTIFY profileChanged)
    // Game counts: "all", "rated", "win", "loss", "draw", "ai".
    Q_PROPERTY(QVariantMap counts READ counts NOTIFY profileChanged)
    // One map per rating: "key", "name", "rating", "games", "prog" and
    // "provisional", the ones that have been played first.
    Q_PROPERTY(QVariantList ratings READ ratings NOTIFY profileChanged)
    // Names of the ratings that have a history to draw.
    Q_PROPERTY(QStringList historyNames READ historyNames NOTIFY historyChanged)

public:
    explicit UserProfile(QObject *parent = nullptr);

    QString username() const { return m_username; }
    void setUsername(const QString &username);
    bool loading() const { return m_loading; }
    bool loaded() const { return m_loaded; }
    QString errorString() const { return m_error; }

    QString name() const { return m_name; }
    QString title() const { return m_title; }
    bool online() const { return m_online; }
    bool playing() const { return m_playing; }
    bool patron() const { return m_patron; }
    bool closed() const { return m_closed; }
    QString bio() const { return m_bio; }
    QString location() const { return m_location; }
    qint64 createdAt() const { return m_createdAt; }
    qint64 seenAt() const { return m_seenAt; }
    int playTime() const { return m_playTime; }
    int followers() const { return m_followers; }
    QVariantMap counts() const { return m_counts; }
    QVariantList ratings() const { return m_ratings; }
    QStringList historyNames() const { return m_historyNames; }

    Q_INVOKABLE void reload();
    // [[ms since the epoch, rating], ...] for the rating called |name|.
    Q_INVOKABLE QVariantList historyPoints(const QString &name) const;

signals:
    void usernameChanged();
    void loadingChanged();
    void errorStringChanged();
    void profileChanged();
    void historyChanged();

private:
    void applyProfile(const QJsonObject &user);
    void applyHistory(const QJsonArray &history);
    void clear();
    void setLoading(bool loading);
    void setError(const QString &error);

    QString m_username;
    bool m_loading = false;
    bool m_loaded = false;
    QString m_error;
    int m_generation = 0;

    QString m_name;
    QString m_title;
    bool m_online = false;
    bool m_playing = false;
    bool m_patron = false;
    bool m_closed = false;
    QString m_bio;
    QString m_location;
    qint64 m_createdAt = 0;
    qint64 m_seenAt = 0;
    int m_playTime = 0;
    int m_followers = 0;
    QVariantMap m_counts;
    QVariantList m_ratings;
    QStringList m_historyNames;
    QList<QVariantList> m_historyPoints;
};

#endif // USERPROFILE_H
