// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FAKELICHESS_H
#define FAKELICHESS_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QUrlQuery>

class QTcpServer;
class QTcpSocket;

// A minimal HTTP server standing in for lichess.org in tests. Routes answer
// with canned JSON; stream routes keep the connection open so the test can
// push ndjson lines. Every request is recorded.
class FakeLichess : public QObject
{
    Q_OBJECT
public:
    struct Request {
        QByteArray method;
        QString path;
        QUrlQuery query;
        QMap<QByteArray, QByteArray> headers; // lower-case names
        QByteArray body;

        QUrlQuery form() const { return QUrlQuery(QString::fromUtf8(body)); }
    };

    explicit FakeLichess(QObject *parent = nullptr);
    ~FakeLichess() override;

    QString url() const;

    // Answers |method| |path| (query ignored) with |status| and |body|.
    void route(const QByteArray &method, const QString &path, int status, const QByteArray &body,
               int delayMs = 0);
    // Answers with 200 and an ndjson body that stays open; see push().
    void streamRoute(const QByteArray &method, const QString &path);
    // Writes raw bytes to every open stream of |path|.
    void push(const QString &path, const QByteArray &data);
    void closeStreams(const QString &path);
    int openStreams(const QString &path) const;

    QList<Request> requests() const { return m_requests; }
    int requestCount(const QByteArray &method, const QString &path) const;
    Request lastRequest(const QByteArray &method, const QString &path) const;
    int maxConcurrent() const { return m_maxConcurrent; }

signals:
    void requestReceived();

private:
    struct Route {
        int status = 200;
        QByteArray body;
        int delayMs = 0;
        bool stream = false;
    };

    void onReadyRead(QTcpSocket *socket);
    void dispatch(QTcpSocket *socket, const Request &request);
    void respond(QTcpSocket *socket, int status, const QByteArray &body);

    QTcpServer *m_server;
    QHash<QByteArray, Route> m_routes;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QMultiHash<QString, QPointer<QTcpSocket>> m_streams;
    QList<Request> m_requests;
    int m_concurrent = 0;
    int m_maxConcurrent = 0;
};

#endif // FAKELICHESS_H
