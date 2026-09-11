// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NDJSONSTREAM_H
#define NDJSONSTREAM_H

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QTimer>

class QNetworkReply;

// Reads a newline-delimited JSON response (Lichess streaming endpoints) and
// emits one message per line. Empty keep-alive lines are swallowed.
class NdjsonStream : public QObject
{
    Q_OBJECT
public:
    // |idleTimeoutMs| > 0 aborts the stream if nothing (not even a keep-alive
    // line) arrives for that long, so dead connections get noticed.
    NdjsonStream(QNetworkReply *reply, int idleTimeoutMs, QObject *parent = nullptr);
    ~NdjsonStream() override;

    int httpStatus() const { return m_status; }
    void abort();

signals:
    // The server accepted the request (2xx) and started streaming.
    void opened();
    void message(const QJsonObject &object);
    // Emitted once. |status| is the HTTP status (0 on network failure);
    // |error| is empty when the server closed the stream normally.
    void finished(int status, const QString &error);

private:
    void onReadyRead();
    void onFinished();
    void processLine(const QByteArray &line);

    QPointer<QNetworkReply> m_reply;
    QByteArray m_buffer;
    QTimer m_idleTimer;
    int m_status = 0;
    bool m_done = false;
};

#endif // NDJSONSTREAM_H
