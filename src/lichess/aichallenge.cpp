// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "aichallenge.h"

#include "core/lichessapi.h"
#include "core/services.h"

#include <QJsonObject>
#include <QUrlQuery>

AiChallenge::AiChallenge(QObject *parent)
    : QObject(parent)
{
}

void AiChallenge::start(int level, const QString &color, int seconds, int increment)
{
    if (m_busy || !Services::api())
        return;
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("level"), QString::number(qBound(1, level, 8)));
    form.addQueryItem(QStringLiteral("clock.limit"), QString::number(qMax(0, seconds)));
    form.addQueryItem(QStringLiteral("clock.increment"), QString::number(qMax(0, increment)));
    form.addQueryItem(QStringLiteral("color"), color == QLatin1String("white") || color == QLatin1String("black")
                                                   ? color : QStringLiteral("random"));
    form.addQueryItem(QStringLiteral("variant"), QStringLiteral("standard"));

    m_error.clear();
    emit errorStringChanged();
    setBusy(true);
    Services::api()->postForm(QStringLiteral("/api/challenge/ai"), form, this, [this](const ApiResult &result) {
        setBusy(false);
        const QString id = result.json.object().value(QStringLiteral("id")).toString();
        if (!result.ok() || id.isEmpty()) {
            m_error = result.errorString.isEmpty() ? tr("Lichess did not start the game.") : result.errorString;
            emit errorStringChanged();
            emit failed(m_error);
            return;
        }
        emit started(id);
    });
}

void AiChallenge::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}
