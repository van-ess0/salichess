// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gameinfo.h"

#include <QCoreApplication>
#include <QJsonObject>

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("GameInfo", text);
}

// Lichess leaves the name out for computers and anonymous players.
QString nameOf(const QString &name, int aiLevel)
{
    if (!name.isEmpty())
        return name;
    if (aiLevel > 0)
        return tr("Stockfish level %1").arg(aiLevel);
    return tr("Anonymous");
}

} // namespace

namespace GameInfo {

bool isOver(const QString &status)
{
    return !status.isEmpty() && status != QLatin1String("created")
            && status != QLatin1String("started");
}

QString resultText(const QString &status, const QString &winner,
                   const QString &whiteName, const QString &blackName)
{
    if (!isOver(status))
        return QString();

    const QString loser = winner == QLatin1String("white") ? blackName : whiteName;
    QString verdict;
    if (winner == QLatin1String("white"))
        verdict = tr("White is victorious");
    else if (winner == QLatin1String("black"))
        verdict = tr("Black is victorious");

    QString reason;
    if (status == QLatin1String("mate"))
        reason = tr("Checkmate");
    else if (status == QLatin1String("resign"))
        reason = tr("%1 resigned").arg(loser);
    else if (status == QLatin1String("stalemate"))
        reason = tr("Stalemate");
    else if (status == QLatin1String("timeout"))
        reason = tr("%1 left the game").arg(loser);
    else if (status == QLatin1String("draw"))
        reason = tr("Draw");
    else if (status == QLatin1String("outoftime"))
        reason = winner.isEmpty() ? tr("Time out") : tr("%1 ran out of time").arg(loser);
    else if (status == QLatin1String("aborted"))
        reason = tr("Game aborted");
    else if (status == QLatin1String("noStart"))
        reason = tr("%1 didn't move").arg(loser);
    else if (status == QLatin1String("cheat"))
        reason = tr("Cheat detected");
    else if (status == QLatin1String("insufficientMaterialClaim"))
        reason = tr("Insufficient material");
    else
        reason = tr("Game over");

    if (verdict.isEmpty() && status != QLatin1String("aborted"))
        verdict = tr("Draw");
    if (verdict.isEmpty() || reason == verdict)
        return reason;
    return reason + QStringLiteral(" • ") + verdict;
}

QString outcome(const QString &status, const QString &winner, const QString &color)
{
    if (!isOver(status) || status == QLatin1String("aborted") || color.isEmpty())
        return QString();
    if (winner.isEmpty())
        return QStringLiteral("draw");
    return winner == color ? QStringLiteral("win") : QStringLiteral("loss");
}

QVariantMap streamPlayer(const QJsonObject &player)
{
    QVariantMap info;
    info.insert(QStringLiteral("id"), player.value(QStringLiteral("id")).toString());
    info.insert(QStringLiteral("name"), nameOf(player.value(QStringLiteral("name")).toString(),
                                               player.value(QStringLiteral("aiLevel")).toInt()));
    info.insert(QStringLiteral("rating"), player.value(QStringLiteral("rating")).toInt());
    info.insert(QStringLiteral("title"), player.value(QStringLiteral("title")).toString());
    info.insert(QStringLiteral("provisional"), player.value(QStringLiteral("provisional")).toBool());
    return info;
}

QVariantMap exportedPlayer(const QJsonObject &player)
{
    const QJsonObject user = player.value(QStringLiteral("user")).toObject();
    QVariantMap info;
    info.insert(QStringLiteral("id"), user.value(QStringLiteral("id")).toString());
    info.insert(QStringLiteral("name"), nameOf(user.value(QStringLiteral("name")).toString(),
                                               player.value(QStringLiteral("aiLevel")).toInt()));
    info.insert(QStringLiteral("rating"), player.value(QStringLiteral("rating")).toInt());
    info.insert(QStringLiteral("title"), user.value(QStringLiteral("title")).toString());
    info.insert(QStringLiteral("provisional"), player.value(QStringLiteral("provisional")).toBool());
    info.insert(QStringLiteral("ratingDiff"), player.value(QStringLiteral("ratingDiff")).toInt());

    const QJsonValue analysis = player.value(QStringLiteral("analysis"));
    if (analysis.isObject()) {
        const QJsonObject a = analysis.toObject();
        info.insert(QStringLiteral("accuracy"), a.value(QStringLiteral("accuracy")).toInt());
        info.insert(QStringLiteral("acpl"), a.value(QStringLiteral("acpl")).toInt());
        info.insert(QStringLiteral("inaccuracy"), a.value(QStringLiteral("inaccuracy")).toInt());
        info.insert(QStringLiteral("mistake"), a.value(QStringLiteral("mistake")).toInt());
        info.insert(QStringLiteral("blunder"), a.value(QStringLiteral("blunder")).toInt());
    }
    return info;
}

} // namespace GameInfo
