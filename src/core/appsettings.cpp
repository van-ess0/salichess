// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "appsettings.h"

#include <QDir>
#include <QStandardPaths>

namespace {

QString settingsPath()
{
    // Sailjail only lets the app write inside its own config directory, so
    // the file lives there rather than at QSettings' default location.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/settings.conf");
}

} // namespace

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
    , m_settings(settingsPath(), QSettings::IniFormat)
{
}

bool AppSettings::store(const char *key, const QVariant &value)
{
    const QString k = QString::fromLatin1(key);
    if (m_settings.contains(k) && m_settings.value(k) == value)
        return false;
    m_settings.setValue(k, value);
    return true;
}

QString AppSettings::boardTheme() const
{
    return m_settings.value(QStringLiteral("board/theme"), QStringLiteral("brown")).toString();
}

void AppSettings::setBoardTheme(const QString &theme)
{
    if (store("board/theme", theme))
        emit boardThemeChanged();
}

QString AppSettings::pieceSet() const
{
    return m_settings.value(QStringLiteral("board/pieceSet"), QStringLiteral("cburnett")).toString();
}

void AppSettings::setPieceSet(const QString &set)
{
    if (store("board/pieceSet", set))
        emit pieceSetChanged();
}

bool AppSettings::showCoordinates() const
{
    return m_settings.value(QStringLiteral("board/showCoordinates"), true).toBool();
}

void AppSettings::setShowCoordinates(bool show)
{
    if (store("board/showCoordinates", show))
        emit showCoordinatesChanged();
}

bool AppSettings::showLegalMoves() const
{
    return m_settings.value(QStringLiteral("board/showLegalMoves"), true).toBool();
}

void AppSettings::setShowLegalMoves(bool show)
{
    if (store("board/showLegalMoves", show))
        emit showLegalMovesChanged();
}

bool AppSettings::animatePieces() const
{
    return m_settings.value(QStringLiteral("board/animatePieces"), true).toBool();
}

void AppSettings::setAnimatePieces(bool animate)
{
    if (store("board/animatePieces", animate))
        emit animatePiecesChanged();
}

bool AppSettings::keepScreenOn() const
{
    return m_settings.value(QStringLiteral("general/keepScreenOn"), true).toBool();
}

void AppSettings::setKeepScreenOn(bool keep)
{
    if (store("general/keepScreenOn", keep))
        emit keepScreenOnChanged();
}

bool AppSettings::notifications() const
{
    return m_settings.value(QStringLiteral("general/notifications"), true).toBool();
}

void AppSettings::setNotifications(bool enabled)
{
    if (store("general/notifications", enabled))
        emit notificationsChanged();
}

QString AppSettings::puzzleDifficulty() const
{
    return m_settings.value(QStringLiteral("puzzles/difficulty"), QStringLiteral("normal")).toString();
}

void AppSettings::setPuzzleDifficulty(const QString &difficulty)
{
    if (store("puzzles/difficulty", difficulty))
        emit puzzleDifficultyChanged();
}
