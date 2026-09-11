// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LICHESSAPI_H
#define LICHESSAPI_H

#include <QByteArray>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QObject>
#include <QPointer>
#include <QQueue>
#include <QTimer>
#include <QUrlQuery>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class NdjsonStream;

struct ApiResult
{
    int status = 0;          // HTTP status, 0 if the request never got a response
    QJsonDocument json;
    QString errorString;     // Lichess "error" field or network error text

    bool ok() const { return status >= 200 && status < 300; }
};

// HTTP access to lichess.org.
//
// Plain requests go through a queue and are sent one at a time, as the
// Lichess API guidelines ask; a 429 response pauses the queue for a minute.
// Streams (ndjson) are long-lived and bypass the queue.
class LichessApi : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool rateLimited READ rateLimited NOTIFY rateLimitedChanged)
public:
    using Callback = std::function<void(const ApiResult &)>;

    explicit LichessApi(QObject *parent = nullptr);

    // The Lichess website, for links shown to the user and OAuth endpoints.
    static QString siteUrl();

    // Where API requests go; siteUrl() unless changed (tests use a local
    // fake server).
    QString serverUrl() const { return m_serverUrl; }
    void setServerUrl(const QString &url) { m_serverUrl = url; }

    void setToken(const QString &token);
    bool hasToken() const { return !m_token.isEmpty(); }
    bool rateLimited() const { return m_rateLimited; }
    QNetworkAccessManager *networkAccessManager() const { return m_nam; }

    // |context| guards the callback: it is not called if context was deleted.
    void get(const QString &path, const QUrlQuery &query, QObject *context, Callback callback);
    void postForm(const QString &path, const QUrlQuery &form, QObject *context, Callback callback);
    void postJson(const QString &path, const QUrlQuery &query, const QJsonDocument &body,
                  QObject *context, Callback callback);
    void deleteResource(const QString &path, QObject *context, Callback callback);

    // Opens an ndjson stream; the caller owns the returned object.
    NdjsonStream *openStream(const QString &path, const QUrlQuery &query = QUrlQuery(),
                             int idleTimeoutMs = 0);
    NdjsonStream *openPostStream(const QString &path, const QUrlQuery &form,
                                 int idleTimeoutMs = 0);

signals:
    void rateLimitedChanged();
    // A request was rejected with 401: the token is invalid or was revoked.
    void unauthorized();

private:
    struct Pending {
        QByteArray verb;
        QNetworkRequest request;
        QByteArray body;
        QPointer<QObject> context;
        bool hasContext;
        Callback callback;
    };

    QNetworkRequest makeRequest(const QString &path, const QUrlQuery &query) const;
    void enqueue(const QByteArray &verb, const QNetworkRequest &request, const QByteArray &body,
                 QObject *context, Callback callback);
    void startNext();
    void onReplyFinished(QNetworkReply *reply);

    QNetworkAccessManager *m_nam;
    QString m_serverUrl;
    QString m_token;
    QQueue<Pending> m_queue;
    Pending m_current;
    QNetworkReply *m_activeReply = nullptr;
    QTimer m_timeoutTimer;
    QTimer m_rateLimitTimer;
    bool m_rateLimited = false;
};

#endif // LICHESSAPI_H
