// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SESSION_H
#define SESSION_H

#include <QObject>
#include <QStringList>
#include <QVariantMap>

class LichessApi;
class TokenStore;

// Login state and the logged-in account. Exposed to QML as "session".
//
// The OAuth PKCE flow itself runs in QML (Amber.Web.Authorization); this
// class provides its configuration and receives the resulting token.
class Session : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY loggedInChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString userId READ userId NOTIFY accountChanged)
    Q_PROPERTY(QString username READ username NOTIFY accountChanged)
    Q_PROPERTY(QString title READ title NOTIFY accountChanged)
    Q_PROPERTY(QVariantMap account READ account NOTIFY accountChanged)
    Q_PROPERTY(int puzzleRating READ puzzleRating NOTIFY accountChanged)

    Q_PROPERTY(QString oauthClientId READ oauthClientId CONSTANT)
    Q_PROPERTY(QStringList oauthScopes READ oauthScopes CONSTANT)
    Q_PROPERTY(QString oauthAuthorizationEndpoint READ oauthAuthorizationEndpoint CONSTANT)
    Q_PROPERTY(QString oauthTokenEndpoint READ oauthTokenEndpoint CONSTANT)

public:
    Session(LichessApi *api, TokenStore *tokenStore, QObject *parent = nullptr);

    bool loggedIn() const { return m_loggedIn; }
    bool busy() const { return m_busy; }
    QString userId() const;
    QString username() const;
    QString title() const;
    QVariantMap account() const { return m_account; }
    int puzzleRating() const;
    // Rating in a perf ("blitz", "rapid", "puzzle", ...), 0 if unknown.
    Q_INVOKABLE int rating(const QString &perf) const;

    QString oauthClientId() const;
    QStringList oauthScopes() const;
    QString oauthAuthorizationEndpoint() const;
    QString oauthTokenEndpoint() const;

    // Restores a saved token at startup.
    void restore();

    Q_INVOKABLE void login(const QString &accessToken);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void refreshAccount();

signals:
    void loggedInChanged();
    void busyChanged();
    void accountChanged();
    void loginFailed(const QString &error);

private:
    void setLoggedIn(bool loggedIn);
    void setBusy(bool busy);
    void clearLocalState();

    LichessApi *m_api;
    TokenStore *m_tokenStore;
    QVariantMap m_account;
    bool m_loggedIn = false;
    bool m_busy = false;
};

#endif // SESSION_H
