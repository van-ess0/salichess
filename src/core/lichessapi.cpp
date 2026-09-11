// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lichessapi.h"

#include "ndjsonstream.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>

namespace {

const int RequestTimeoutMs = 30 * 1000;
const int RateLimitPauseMs = 60 * 1000;

QByteArray userAgent()
{
    // Lichess asks API clients to identify themselves.
    return QStringLiteral("%1/%2 (Sailfish OS; +https://github.com/van-ess0/salichess)")
            .arg(QCoreApplication::applicationName(), QCoreApplication::applicationVersion())
            .toUtf8();
}

QString errorFromBody(const QJsonDocument &json)
{
    const QJsonValue error = json.object().value(QStringLiteral("error"));
    if (error.isString())
        return error.toString();
    if (error.isObject()) {
        // Validation errors come as {"error": {"field": ["message"]}}.
        const QJsonObject fields = error.toObject();
        for (auto it = fields.begin(); it != fields.end(); ++it) {
            const QJsonValue value = it.value();
            if (value.isArray() && !value.toArray().isEmpty())
                return value.toArray().first().toString();
            if (value.isString())
                return value.toString();
        }
    }
    return QString();
}

} // namespace

LichessApi::LichessApi(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_serverUrl(siteUrl())
{
    // Qt 5.6's bearer management can leave QNAM stuck in "not accessible"
    // after connectivity changes on Sailfish. Keep trying; a real outage
    // still shows up as a network error.
    connect(m_nam, &QNetworkAccessManager::networkAccessibleChanged, this,
            [this](QNetworkAccessManager::NetworkAccessibility accessible) {
        if (accessible == QNetworkAccessManager::NotAccessible)
            m_nam->setNetworkAccessible(QNetworkAccessManager::Accessible);
    });

    m_timeoutTimer.setSingleShot(true);
    m_timeoutTimer.setInterval(RequestTimeoutMs);
    connect(&m_timeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_activeReply)
            m_activeReply->abort();
    });

    m_rateLimitTimer.setSingleShot(true);
    m_rateLimitTimer.setInterval(RateLimitPauseMs);
    connect(&m_rateLimitTimer, &QTimer::timeout, this, [this]() {
        m_rateLimited = false;
        emit rateLimitedChanged();
        startNext();
    });
}

QString LichessApi::siteUrl()
{
    return QStringLiteral("https://lichess.org");
}

void LichessApi::setToken(const QString &token)
{
    m_token = token;
}

void LichessApi::get(const QString &path, const QUrlQuery &query, QObject *context, Callback callback)
{
    enqueue("GET", makeRequest(path, query), QByteArray(), context, std::move(callback));
}

void LichessApi::postForm(const QString &path, const QUrlQuery &form, QObject *context, Callback callback)
{
    QNetworkRequest request = makeRequest(path, QUrlQuery());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/x-www-form-urlencoded"));
    enqueue("POST", request, form.query(QUrl::FullyEncoded).toUtf8(), context, std::move(callback));
}

void LichessApi::postJson(const QString &path, const QUrlQuery &query, const QJsonDocument &body,
                          QObject *context, Callback callback)
{
    QNetworkRequest request = makeRequest(path, query);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/json"));
    enqueue("POST", request, body.toJson(QJsonDocument::Compact), context, std::move(callback));
}

void LichessApi::deleteResource(const QString &path, QObject *context, Callback callback)
{
    enqueue("DELETE", makeRequest(path, QUrlQuery()), QByteArray(), context, std::move(callback));
}

NdjsonStream *LichessApi::openStream(const QString &path, const QUrlQuery &query, int idleTimeoutMs)
{
    QNetworkRequest request = makeRequest(path, query);
    request.setRawHeader("Accept", "application/x-ndjson");
    return new NdjsonStream(m_nam->get(request), idleTimeoutMs);
}

NdjsonStream *LichessApi::openPostStream(const QString &path, const QUrlQuery &form, int idleTimeoutMs)
{
    QNetworkRequest request = makeRequest(path, QUrlQuery());
    request.setRawHeader("Accept", "application/x-ndjson");
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/x-www-form-urlencoded"));
    return new NdjsonStream(m_nam->post(request, form.query(QUrl::FullyEncoded).toUtf8()), idleTimeoutMs);
}

QNetworkRequest LichessApi::makeRequest(const QString &path, const QUrlQuery &query) const
{
    QUrl url(m_serverUrl + path);
    if (!query.isEmpty())
        url.setQuery(query);
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", userAgent());
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    if (!m_token.isEmpty())
        request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    return request;
}

void LichessApi::enqueue(const QByteArray &verb, const QNetworkRequest &request, const QByteArray &body,
                         QObject *context, Callback callback)
{
    m_queue.enqueue(Pending { verb, request, body, QPointer<QObject>(context), context != nullptr,
                              std::move(callback) });
    startNext();
}

void LichessApi::startNext()
{
    if (m_activeReply || m_rateLimited || m_queue.isEmpty())
        return;

    m_current = m_queue.dequeue();
    // Qt 5.6 has no sendCustomRequest() overload taking a QByteArray body.
    if (m_current.verb == "POST")
        m_activeReply = m_nam->post(m_current.request, m_current.body);
    else if (m_current.verb == "DELETE")
        m_activeReply = m_nam->deleteResource(m_current.request);
    else
        m_activeReply = m_nam->get(m_current.request);

    QNetworkReply *reply = m_activeReply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onReplyFinished(reply); });
    m_timeoutTimer.start();
}

void LichessApi::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();
    m_timeoutTimer.stop();
    m_activeReply = nullptr;

    ApiResult result;
    result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.json = QJsonDocument::fromJson(reply->readAll());
    if (!result.ok()) {
        result.errorString = errorFromBody(result.json);
        if (result.errorString.isEmpty())
            result.errorString = reply->error() == QNetworkReply::OperationCanceledError
                    ? tr("The request timed out")
                    : reply->errorString();
    }

    Pending finished = std::move(m_current);
    m_current = Pending();

    if (result.status == 429) {
        m_rateLimited = true;
        emit rateLimitedChanged();
        m_rateLimitTimer.start();
        result.errorString = tr("Too many requests. Please wait a minute.");
    } else if (result.status == 401 && !m_token.isEmpty()) {
        emit unauthorized();
    }

    // Skip the callback if its context object has been destroyed meanwhile.
    if (finished.callback && !(finished.hasContext && finished.context.isNull()))
        finished.callback(result);

    startNext();
}
