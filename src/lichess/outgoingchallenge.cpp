// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "outgoingchallenge.h"

#include "eventstream.h"
#include "core/lichessapi.h"
#include "core/ndjsonstream.h"

#include <QJsonObject>
#include <QRegularExpression>
#include <QTimer>

OutgoingChallenge::OutgoingChallenge(LichessApi *api, EventStream *events, QObject *parent)
    : QObject(parent)
    , m_api(api)
{
    // The keep-alive stream only says "declined"; the event stream has the reason.
    connect(events, &EventStream::challengeDeclined, this, [this](const QVariantMap &challenge) {
        if (challenge.value(QStringLiteral("id")).toString() != m_challengeId
                || (m_state != Creating && m_state != Waiting))
            return;
        m_declineReason = challenge.value(QStringLiteral("declineReason")).toString();
        closeStream();
        setState(Declined);
    });
    connect(events, &EventStream::gameStarted, this, [this](const QVariantMap &game) {
        const QString id = game.value(QStringLiteral("gameId"), game.value(QStringLiteral("id"))).toString();
        if (m_state == Waiting && id == m_challengeId) {
            closeStream();
            setState(Accepted);
        }
    });
}

OutgoingChallenge::~OutgoingChallenge()
{
    closeStream();
}

void OutgoingChallenge::create(const QString &username, const QVariantMap &options)
{
    closeStream();
    m_challengeId.clear();
    m_url.clear();
    m_error.clear();
    m_declineReason.clear();
    m_opponent = username.trimmed();

    static const QRegularExpression validName(QStringLiteral("^[A-Za-z0-9_-]{2,30}$"));
    if (!validName.match(m_opponent).hasMatch()) {
        m_error = tr("\"%1\" is not a valid Lichess username.").arg(m_opponent);
        setState(Failed);
        return;
    }

    QUrlQuery form;
    const bool rated = options.value(QStringLiteral("rated")).toBool();
    form.addQueryItem(QStringLiteral("rated"), rated ? QStringLiteral("true") : QStringLiteral("false"));
    if (options.value(QStringLiteral("correspondence")).toBool()) {
        form.addQueryItem(QStringLiteral("days"), QString::number(options.value(QStringLiteral("days"), 3).toInt()));
    } else {
        const int limit = options.value(QStringLiteral("minutes"), 10).toInt() * 60;
        form.addQueryItem(QStringLiteral("clock.limit"), QString::number(limit));
        form.addQueryItem(QStringLiteral("clock.increment"),
                          QString::number(options.value(QStringLiteral("increment"), 0).toInt()));
    }
    form.addQueryItem(QStringLiteral("color"), options.value(QStringLiteral("color"), QStringLiteral("random")).toString());
    form.addQueryItem(QStringLiteral("variant"), QStringLiteral("standard"));
    form.addQueryItem(QStringLiteral("keepAliveStream"), QStringLiteral("true"));

    setState(Creating);
    m_stream = m_api->openPostStream(QStringLiteral("/api/challenge/%1").arg(m_opponent), form);
    m_stream->setParent(this);

    connect(m_stream.data(), &NdjsonStream::message, this, [this](const QJsonObject &message) {
        const QString done = message.value(QStringLiteral("done")).toString();
        if (!done.isEmpty()) {
            closeStream();
            if (done == QLatin1String("accepted")) {
                setState(Accepted);
            } else if (done == QLatin1String("declined")) {
                // Give the event stream a moment to deliver the reason.
                QTimer::singleShot(1500, this, [this]() {
                    if (m_state == Waiting || m_state == Creating)
                        setState(Declined);
                });
            } else {
                setState(Canceled);
            }
            return;
        }
        const QJsonObject challenge = message.contains(QStringLiteral("challenge"))
                ? message.value(QStringLiteral("challenge")).toObject()
                : message;
        const QString id = challenge.value(QStringLiteral("id")).toString();
        if (!id.isEmpty()) {
            m_challengeId = id;
            m_url = challenge.value(QStringLiteral("url")).toString();
            const QString name = challenge.value(QStringLiteral("destUser")).toObject()
                    .value(QStringLiteral("name")).toString();
            if (!name.isEmpty())
                m_opponent = name;
            setState(Waiting);
        }
    });
    connect(m_stream.data(), &NdjsonStream::finished, this, [this](int status, const QString &error) {
        if (m_stream)
            m_stream->deleteLater();
        m_stream.clear();
        if (m_state != Creating && m_state != Waiting)
            return;
        if (status >= 200 && status < 300 && m_state == Waiting) {
            // Connection dropped without a verdict; the challenge is gone.
            m_error = tr("The connection to Lichess was lost.");
        } else {
            m_error = error.isEmpty() ? tr("Could not create the challenge.") : error;
        }
        setState(Failed);
    });
}

void OutgoingChallenge::cancel()
{
    if (m_state != Creating && m_state != Waiting)
        return;
    if (!m_challengeId.isEmpty())
        m_api->postForm(QStringLiteral("/api/challenge/%1/cancel").arg(m_challengeId), QUrlQuery(), nullptr, nullptr);
    closeStream();
    setState(Canceled);
}

void OutgoingChallenge::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
}

void OutgoingChallenge::closeStream()
{
    if (!m_stream)
        return;
    m_stream->abort();
    m_stream->deleteLater();
    m_stream.clear();
}
