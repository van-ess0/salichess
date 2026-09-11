// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef APPACTIVATION_H
#define APPACTIVATION_H

#include <QDBusAbstractAdaptor>
#include <QObject>

// Receives activation requests from notification actions. Exposed to QML as
// "appActivation"; QML brings the window forward and navigates.
class AppActivation : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString serviceName READ serviceName CONSTANT)
    Q_PROPERTY(QString objectPath READ objectPath CONSTANT)
    Q_PROPERTY(QString interfaceName READ interfaceName CONSTANT)
public:
    explicit AppActivation(QObject *parent = nullptr);

    // Registers the D-Bus service; returns false if the name is taken.
    bool registerOnBus();

    static QString serviceName();
    static QString objectPath();
    static QString interfaceName();

signals:
    void activateRequested();
    void openGameRequested(const QString &gameId);
    void openChallengesRequested();
};

class AppActivationAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.vaness0.salichess")
public:
    explicit AppActivationAdaptor(AppActivation *parent);

public slots:
    Q_NOREPLY void activate();
    Q_NOREPLY void openGame(const QString &gameId);
    Q_NOREPLY void openChallenges();

private:
    AppActivation *m_activation;
};

#endif // APPACTIVATION_H
