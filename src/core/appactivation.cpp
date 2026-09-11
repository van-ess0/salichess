// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "appactivation.h"

#include <QDBusConnection>

AppActivation::AppActivation(QObject *parent)
    : QObject(parent)
{
    new AppActivationAdaptor(this);
}

bool AppActivation::registerOnBus()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    // Sailjail lets an app own the name "<OrganizationName>.<ApplicationName>".
    if (!bus.registerService(serviceName()))
        return false;
    return bus.registerObject(objectPath(), this);
}

QString AppActivation::serviceName()
{
    return QStringLiteral("io.github.vaness0.harbour-salichess");
}

QString AppActivation::objectPath()
{
    return QStringLiteral("/io/github/vaness0/salichess");
}

QString AppActivation::interfaceName()
{
    return QStringLiteral("io.github.vaness0.salichess");
}

AppActivationAdaptor::AppActivationAdaptor(AppActivation *parent)
    : QDBusAbstractAdaptor(parent)
    , m_activation(parent)
{
}

void AppActivationAdaptor::activate()
{
    emit m_activation->activateRequested();
}

void AppActivationAdaptor::openGame(const QString &gameId)
{
    emit m_activation->openGameRequested(gameId);
}

void AppActivationAdaptor::openChallenges()
{
    emit m_activation->openChallengesRequested();
}
