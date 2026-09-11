// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ndjsonstream.h"

#include <QJsonDocument>
#include <QNetworkReply>

NdjsonStream::NdjsonStream(QNetworkReply *reply, int idleTimeoutMs, QObject *parent)
    : QObject(parent)
    , m_reply(reply)
{
    connect(reply, &QNetworkReply::readyRead, this, &NdjsonStream::onReadyRead);
    connect(reply, &QNetworkReply::finished, this, &NdjsonStream::onFinished);

    if (idleTimeoutMs > 0) {
        m_idleTimer.setInterval(idleTimeoutMs);
        m_idleTimer.setSingleShot(true);
        connect(&m_idleTimer, &QTimer::timeout, this, [this]() {
            if (m_reply)
                m_reply->abort();
        });
        m_idleTimer.start();
    }
}

NdjsonStream::~NdjsonStream()
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
    }
}

void NdjsonStream::abort()
{
    if (m_reply && !m_done) {
        m_done = true; // closed by us: no finished() signal
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply.clear();
    }
    m_idleTimer.stop();
}

void NdjsonStream::onReadyRead()
{
    if (!m_reply || !m_reply->isOpen())
        return; // aborted, e.g. by the idle timer
    const bool first = m_status == 0;
    if (first)
        m_status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (m_idleTimer.interval() > 0)
        m_idleTimer.start();

    m_buffer += m_reply->readAll();
    // Error responses are plain JSON, not ndjson; onFinished() parses them.
    if (m_status < 200 || m_status >= 300)
        return;

    // A signal handler may abort or even delete this stream.
    QPointer<NdjsonStream> self(this);
    if (first) {
        emit opened();
        if (!self || m_done)
            return;
    }
    int newline;
    while ((newline = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(newline).trimmed();
        m_buffer.remove(0, newline + 1);
        if (!line.isEmpty())
            processLine(line);
        if (!self || m_done)
            return;
    }
}

void NdjsonStream::onFinished()
{
    if (m_done || !m_reply)
        return;
    QPointer<NdjsonStream> self(this);
    onReadyRead();
    if (!self || m_done)
        return;
    m_done = true;
    m_idleTimer.stop();

    m_status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString error;
    if (m_status >= 200 && m_status < 300) {
        const QByteArray rest = m_buffer.trimmed();
        if (!rest.isEmpty()) {
            processLine(rest);
            if (!self)
                return;
        }
        if (m_reply->error() != QNetworkReply::NoError
                && m_reply->error() != QNetworkReply::RemoteHostClosedError)
            error = m_reply->errorString();
    } else {
        const QJsonObject body = QJsonDocument::fromJson(m_buffer.trimmed()).object();
        error = body.value(QStringLiteral("error")).toString();
        if (error.isEmpty())
            error = m_reply->errorString();
    }
    m_buffer.clear();
    m_reply->deleteLater();
    m_reply.clear();
    emit finished(m_status, error);
}

void NdjsonStream::processLine(const QByteArray &line)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error == QJsonParseError::NoError && doc.isObject())
        emit message(doc.object());
}
