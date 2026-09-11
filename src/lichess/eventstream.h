// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef EVENTSTREAM_H
#define EVENTSTREAM_H

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantMap>

class LichessApi;
class NdjsonStream;

// The logged-in user's event stream (/api/stream/event): game starts and
// finishes, incoming and outgoing challenges. Reconnects with backoff.
class EventStream : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
public:
    explicit EventStream(LichessApi *api, QObject *parent = nullptr);
    ~EventStream() override;

    bool connected() const { return m_connected; }

    void start();
    void stop();

signals:
    void connectedChanged();
    void gameStarted(const QVariantMap &game);
    void gameFinished(const QVariantMap &game);
    void challengeReceived(const QVariantMap &challenge);
    void challengeCanceled(const QVariantMap &challenge);
    void challengeDeclined(const QVariantMap &challenge);

private:
    void connectStream();
    void onMessage(const QJsonObject &message);
    void onFinished(int status, const QString &error);
    void setConnected(bool connected);

    LichessApi *m_api;
    QPointer<NdjsonStream> m_stream;
    QTimer m_reconnectTimer;
    int m_failures = 0;
    bool m_running = false;
    bool m_connected = false;
};

#endif // EVENTSTREAM_H
