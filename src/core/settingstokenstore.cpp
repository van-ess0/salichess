// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingstokenstore.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>

namespace {
const char TokenKey[] = "lichess/accessToken";
}

QString SettingsTokenStore::load()
{
    QSettings settings(filePath(), QSettings::IniFormat);
    return settings.value(TokenKey).toString();
}

bool SettingsTokenStore::save(const QString &token)
{
    const QString path = filePath();
    QSettings settings(path, QSettings::IniFormat);
    settings.setValue(TokenKey, token);
    settings.sync();
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return settings.status() == QSettings::NoError;
}

void SettingsTokenStore::clear()
{
    QSettings settings(filePath(), QSettings::IniFormat);
    settings.remove(TokenKey);
    settings.sync();
}

QString SettingsTokenStore::filePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/auth.conf");
}
