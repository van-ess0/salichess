// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "session.h"

#include "lichessapi.h"
#include "tokenstore.h"

#include <QJsonObject>

Session::Session(LichessApi *api, TokenStore *tokenStore, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_tokenStore(tokenStore)
{
    connect(m_api, &LichessApi::unauthorized, this, [this]() {
        if (!m_loggedIn)
            return;
        clearLocalState();
        emit loginFailed(tr("Your Lichess login has expired or was revoked. Please log in again."));
    });
}

QString Session::userId() const
{
    return m_account.value(QStringLiteral("id")).toString();
}

QString Session::username() const
{
    return m_account.value(QStringLiteral("username")).toString();
}

QString Session::title() const
{
    return m_account.value(QStringLiteral("title")).toString();
}

int Session::puzzleRating() const
{
    return rating(QStringLiteral("puzzle"));
}

int Session::rating(const QString &perf) const
{
    const QVariantMap perfs = m_account.value(QStringLiteral("perfs")).toMap();
    return perfs.value(perf).toMap().value(QStringLiteral("rating")).toInt();
}

QString Session::oauthClientId() const
{
    return QStringLiteral("io.github.vaness0.harbour-salichess");
}

QStringList Session::oauthScopes() const
{
    return {
        QStringLiteral("board:play"),
        QStringLiteral("challenge:read"),
        QStringLiteral("challenge:write"),
        QStringLiteral("puzzle:read"),
        QStringLiteral("puzzle:write"),
        QStringLiteral("follow:read"),
    };
}

QString Session::oauthAuthorizationEndpoint() const
{
    return LichessApi::siteUrl() + QStringLiteral("/oauth");
}

QString Session::oauthTokenEndpoint() const
{
    return LichessApi::siteUrl() + QStringLiteral("/api/token");
}

void Session::restore()
{
    const QString token = m_tokenStore->load();
    if (token.isEmpty())
        return;
    m_api->setToken(token);
    setLoggedIn(true);
    refreshAccount();
}

void Session::login(const QString &accessToken)
{
    if (accessToken.isEmpty()) {
        emit loginFailed(tr("Lichess did not return an access token."));
        return;
    }
    setBusy(true);
    m_api->setToken(accessToken);
    m_api->get(QStringLiteral("/api/account"), QUrlQuery(), this, [this, accessToken](const ApiResult &result) {
        setBusy(false);
        if (!result.ok()) {
            m_api->setToken(QString());
            emit loginFailed(result.errorString);
            return;
        }
        m_tokenStore->save(accessToken);
        m_account = result.json.object().toVariantMap();
        emit accountChanged();
        setLoggedIn(true);
    });
}

void Session::logout()
{
    if (!m_loggedIn)
        return;
    // Revoke the token on the server; the request carries the token even
    // though it is cleared locally right away.
    m_api->deleteResource(QStringLiteral("/api/token"), nullptr, nullptr);
    clearLocalState();
}

void Session::refreshAccount()
{
    if (!m_api->hasToken())
        return;
    m_api->get(QStringLiteral("/api/account"), QUrlQuery(), this, [this](const ApiResult &result) {
        if (!result.ok())
            return;
        m_account = result.json.object().toVariantMap();
        emit accountChanged();
    });
}

void Session::setLoggedIn(bool loggedIn)
{
    if (m_loggedIn == loggedIn)
        return;
    m_loggedIn = loggedIn;
    emit loggedInChanged();
}

void Session::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void Session::clearLocalState()
{
    m_tokenStore->clear();
    m_api->setToken(QString());
    m_account.clear();
    emit accountChanged();
    setLoggedIn(false);
}
