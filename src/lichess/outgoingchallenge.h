// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OUTGOINGCHALLENGE_H
#define OUTGOINGCHALLENGE_H

#include <QObject>
#include <QPointer>
#include <QVariantMap>

class EventStream;
class LichessApi;
class NdjsonStream;

// A challenge the user is sending to a friend. The request is kept open
// (keepAliveStream) so real-time challenges don't expire while waiting.
// Created and owned by OutgoingChallenges.
class OutgoingChallenge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString challengeId READ challengeId NOTIFY stateChanged)
    Q_PROPERTY(QString url READ url NOTIFY stateChanged)
    Q_PROPERTY(QString opponent READ opponent NOTIFY stateChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY stateChanged)
    Q_PROPERTY(QString declineReason READ declineReason NOTIFY stateChanged)
public:
    enum State { Idle, Creating, Waiting, Accepted, Declined, Canceled, Failed };
    Q_ENUM(State)

    OutgoingChallenge(LichessApi *api, EventStream *events, QObject *parent = nullptr);
    ~OutgoingChallenge() override;

    State state() const { return m_state; }
    QString challengeId() const { return m_challengeId; }
    QString url() const { return m_url; }
    QString opponent() const { return m_opponent; }
    QString errorString() const { return m_error; }
    QString declineReason() const { return m_declineReason; }

    // options: correspondence (bool), minutes (int), increment (int),
    // days (int), rated (bool), color ("random", "white", "black")
    Q_INVOKABLE void create(const QString &username, const QVariantMap &options);
    Q_INVOKABLE void cancel();

signals:
    void stateChanged();

private:
    void setState(State state);
    void closeStream();

    LichessApi *m_api;
    QPointer<NdjsonStream> m_stream;
    State m_state = Idle;
    QString m_challengeId;
    QString m_url;
    QString m_opponent;
    QString m_error;
    QString m_declineReason;
};

#endif // OUTGOINGCHALLENGE_H
