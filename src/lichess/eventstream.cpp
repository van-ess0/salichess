// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "eventstream.h"

#include "core/lichessapi.h"
#include "core/ndjsonstream.h"

#include <QJsonObject>

namespace {
// Lichess sends a keep-alive line every 7 seconds.
const int IdleTimeoutMs = 25 * 1000;
const int MaxBackoffMs = 60 * 1000;
}

EventStream::EventStream(LichessApi *api, QObject *parent)
    : QObject(parent)
    , m_api(api)
{
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &EventStream::connectStream);
}

EventStream::~EventStream()
{
    delete m_stream;
}

void EventStream::start()
{
    if (m_running)
        return;
    m_running = true;
    m_failures = 0;
    connectStream();
}

void EventStream::stop()
{
    m_running = false;
    m_reconnectTimer.stop();
    if (m_stream) {
        m_stream->abort();
        m_stream->deleteLater();
        m_stream.clear();
    }
    setConnected(false);
}

void EventStream::connectStream()
{
    if (!m_running || m_stream)
        return;
    m_stream = m_api->openStream(QStringLiteral("/api/stream/event"), QUrlQuery(), IdleTimeoutMs);
    m_stream->setParent(this);
    connect(m_stream.data(), &NdjsonStream::opened, this, [this]() {
        m_failures = 0;
        setConnected(true);
    });
    connect(m_stream.data(), &NdjsonStream::message, this, &EventStream::onMessage);
    connect(m_stream.data(), &NdjsonStream::finished, this, &EventStream::onFinished);
}

void EventStream::onMessage(const QJsonObject &message)
{
    const QString type = message.value(QStringLiteral("type")).toString();
    const QVariantMap data = message.toVariantMap();
    if (type == QLatin1String("gameStart"))
        emit gameStarted(data.value(QStringLiteral("game")).toMap());
    else if (type == QLatin1String("gameFinish"))
        emit gameFinished(data.value(QStringLiteral("game")).toMap());
    else if (type == QLatin1String("challenge"))
        emit challengeReceived(data.value(QStringLiteral("challenge")).toMap());
    else if (type == QLatin1String("challengeCanceled"))
        emit challengeCanceled(data.value(QStringLiteral("challenge")).toMap());
    else if (type == QLatin1String("challengeDeclined"))
        emit challengeDeclined(data.value(QStringLiteral("challenge")).toMap());
}

void EventStream::onFinished(int status, const QString &error)
{
    Q_UNUSED(error)
    if (m_stream)
        m_stream->deleteLater();
    m_stream.clear();
    setConnected(false);

    if (!m_running || status == 401)
        return;

    // Back off: 2 s, 4 s, 8 s, ... up to a minute (a minute straight away
    // when rate limited).
    ++m_failures;
    int delay = qMin(MaxBackoffMs, 1000 << qMin(m_failures, 6));
    if (status == 429)
        delay = MaxBackoffMs;
    m_reconnectTimer.start(delay);
}

void EventStream::setConnected(bool connected)
{
    if (m_connected == connected)
        return;
    m_connected = connected;
    emit connectedChanged();
}
