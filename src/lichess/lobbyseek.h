// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOBBYSEEK_H
#define LOBBYSEEK_H

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantMap>

class EventStream;
class LichessApi;
class NdjsonStream;

// A seek for a game with a random opponent (/api/board/seek). Exposed to QML
// as "lobbySeek". Lichess allows one real-time seek per user: its request
// stays open while the seek waits, closing it cancels the seek, and the game
// is announced on the event stream. A correspondence seek is posted and
// stays in the lobby until someone joins; the API can't withdraw it.
class LobbySeek : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool seeking READ seeking NOTIFY stateChanged)
    Q_PROPERTY(bool correspondence READ correspondence NOTIFY stateChanged)
    // "10+5 • Rapid • Rated"
    Q_PROPERTY(QString description READ description NOTIFY stateChanged)
    Q_PROPERTY(QString gameId READ gameId NOTIFY stateChanged)
    Q_PROPERTY(QString opponent READ opponent NOTIFY stateChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY stateChanged)
public:
    enum State { Idle, Seeking, Found, Posted, Expired, Canceled, Failed };
    Q_ENUM(State)

    LobbySeek(LichessApi *api, EventStream *events, QObject *parent = nullptr);
    ~LobbySeek() override;

    State state() const { return m_state; }
    bool seeking() const { return m_state == Seeking; }
    bool correspondence() const { return m_correspondence; }
    QString description() const;
    QString gameId() const { return m_gameId; }
    QString opponent() const { return m_opponent; }
    QString errorString() const { return m_error; }

    // Lichess' speed of a clock: "bullet", "blitz", "rapid" or "classical".
    static QString speedOf(int limitSeconds, int increment);

    // options: correspondence (bool), minutes (int), increment (int),
    // days (int), rated (bool), color ("random", "white", "black"),
    // ratingDelta (int: only opponents this close to my rating, 0 for any)
    Q_INVOKABLE void create(const QVariantMap &options);
    // Real-time seeks only.
    Q_INVOKABLE void cancel();

signals:
    void stateChanged();
    void gameFound(const QString &gameId, const QString &opponent);

private:
    void onGameStarted(const QVariantMap &game);
    void setState(State state);
    void closeStream();

    LichessApi *m_api;
    QPointer<NdjsonStream> m_stream;
    QTimer m_gameWait;
    State m_state = Idle;
    bool m_correspondence = false;
    bool m_rated = false;
    QString m_speed;
    QString m_timeControl;
    QString m_gameId;
    QString m_opponent;
    QString m_error;
};

#endif // LOBBYSEEK_H
