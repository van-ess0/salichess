// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fakelichess.h"

#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

namespace {

QByteArray routeKey(const QByteArray &method, const QString &path)
{
    return method + ' ' + path.toUtf8();
}

QByteArray reason(int status)
{
    switch (status) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 404: return "Not Found";
    case 429: return "Too Many Requests";
    default: return "Status";
    }
}

} // namespace

FakeLichess::FakeLichess(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    m_server->listen(QHostAddress::LocalHost);
    connect(m_server, &QTcpServer::newConnection, this, [this]() {
        while (QTcpSocket *socket = m_server->nextPendingConnection()) {
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onReadyRead(socket); });
            connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
                m_buffers.remove(socket);
                socket->deleteLater();
            });
        }
    });
}

FakeLichess::~FakeLichess() = default;

QString FakeLichess::url() const
{
    return QStringLiteral("http://127.0.0.1:%1").arg(m_server->serverPort());
}

void FakeLichess::route(const QByteArray &method, const QString &path, int status, const QByteArray &body,
                        int delayMs)
{
    Route r;
    r.status = status;
    r.body = body;
    r.delayMs = delayMs;
    m_routes.insert(routeKey(method, path), r);
}

void FakeLichess::streamRoute(const QByteArray &method, const QString &path)
{
    Route r;
    r.stream = true;
    m_routes.insert(routeKey(method, path), r);
}

void FakeLichess::push(const QString &path, const QByteArray &data)
{
    for (const QPointer<QTcpSocket> &socket : m_streams.values(path)) {
        if (socket) {
            socket->write(data);
            socket->flush();
        }
    }
}

void FakeLichess::closeStreams(const QString &path)
{
    for (const QPointer<QTcpSocket> &socket : m_streams.values(path)) {
        if (socket)
            socket->disconnectFromHost();
    }
    m_streams.remove(path);
}

int FakeLichess::openStreams(const QString &path) const
{
    int n = 0;
    for (const QPointer<QTcpSocket> &socket : m_streams.values(path)) {
        if (socket && socket->state() == QAbstractSocket::ConnectedState)
            ++n;
    }
    return n;
}

int FakeLichess::requestCount(const QByteArray &method, const QString &path) const
{
    int n = 0;
    for (const Request &request : m_requests) {
        if (request.method == method && request.path == path)
            ++n;
    }
    return n;
}

FakeLichess::Request FakeLichess::lastRequest(const QByteArray &method, const QString &path) const
{
    for (int i = m_requests.size() - 1; i >= 0; --i) {
        if (m_requests.at(i).method == method && m_requests.at(i).path == path)
            return m_requests.at(i);
    }
    return Request();
}

void FakeLichess::onReadyRead(QTcpSocket *socket)
{
    QByteArray &buffer = m_buffers[socket];
    buffer += socket->readAll();

    const int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0)
        return;

    Request request;
    const QList<QByteArray> lines = buffer.left(headerEnd).split('\n');
    const QList<QByteArray> requestLine = lines.first().trimmed().split(' ');
    request.method = requestLine.value(0);
    const QUrl target(QString::fromUtf8(requestLine.value(1)));
    request.path = target.path();
    request.query = QUrlQuery(target.query());
    for (int i = 1; i < lines.size(); ++i) {
        const int colon = lines.at(i).indexOf(':');
        if (colon > 0)
            request.headers.insert(lines.at(i).left(colon).trimmed().toLower(), lines.at(i).mid(colon + 1).trimmed());
    }

    const int length = request.headers.value("content-length").toInt();
    if (buffer.size() < headerEnd + 4 + length)
        return; // body incomplete
    request.body = buffer.mid(headerEnd + 4, length);
    buffer.clear();

    dispatch(socket, request);
}

void FakeLichess::dispatch(QTcpSocket *socket, const Request &request)
{
    m_requests.append(request);
    emit requestReceived();

    const auto it = m_routes.constFind(routeKey(request.method, request.path));
    if (it == m_routes.constEnd()) {
        respond(socket, 404, R"({"error":"Not found"})");
        return;
    }

    const Route route = it.value();
    if (route.stream) {
        socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/x-ndjson\r\nConnection: close\r\n\r\n");
        socket->flush();
        m_streams.insert(request.path, QPointer<QTcpSocket>(socket));
        return;
    }

    m_maxConcurrent = qMax(m_maxConcurrent, ++m_concurrent);
    QPointer<QTcpSocket> guarded(socket);
    QTimer::singleShot(route.delayMs, this, [this, guarded, route]() {
        --m_concurrent;
        if (guarded)
            respond(guarded, route.status, route.body);
    });
}

void FakeLichess::respond(QTcpSocket *socket, int status, const QByteArray &body)
{
    socket->write("HTTP/1.1 " + QByteArray::number(status) + ' ' + reason(status) + "\r\n"
                  "Content-Type: application/json\r\n"
                  "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                  "Connection: close\r\n\r\n" + body);
    socket->disconnectFromHost();
}
