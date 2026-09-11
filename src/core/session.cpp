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
        // While logging out, logout() handles the answers itself.
        if (!m_loggedIn || m_busy)
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
    if (!m_loggedIn || m_busy)
        return;
    // The token is only forgotten once Lichess has revoked it (401: it was
    // invalid already), so that it can't stay valid without the user knowing.
    setBusy(true);
    m_api->deleteResource(QStringLiteral("/api/token"), this, [this](const ApiResult &result) {
        setBusy(false);
        if (!result.ok() && result.status != 401) {
            emit logoutFailed(tr("You are still logged in, because Lichess did not confirm the logout: %1")
                              .arg(result.errorString));
            return;
        }
        clearLocalState();
    });
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
