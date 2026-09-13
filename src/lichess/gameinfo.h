// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GAMEINFO_H
#define GAMEINFO_H

#include <QString>
#include <QVariantMap>

class QJsonObject;

// Pieces of a Lichess game that are read the same way wherever the game
// comes from: the games being played (GameController, from the Board API
// stream), the games in the history, and the games being reviewed
// (GameAnalysis, from /game/export). Lichess describes a player differently
// in the stream than in an export, hence the two player functions.
namespace GameInfo {

// A game with this status is over. "created" and "started" are not, and
// neither is an empty status (nothing is known yet).
bool isOver(const QString &status);

// "Checkmate • White is victorious", "Sara resigned • Draw", ...
// |winner| is "white", "black" or empty.
QString resultText(const QString &status, const QString &winner,
                   const QString &whiteName, const QString &blackName);

// The result from |color|'s point of view: "win", "loss", "draw", or an
// empty string if the game was aborted or is not over.
QString outcome(const QString &status, const QString &winner, const QString &color);

// A player of a game being streamed: {"id", "name", "rating", "title",
// "provisional"} out of a flat {"id":..., "name":...} object.
QVariantMap streamPlayer(const QJsonObject &player);

// A player of an exported game: the same keys out of a {"user": {...},
// "rating":...} object, plus "ratingDiff" and, where Lichess analysed the
// game, "accuracy", "acpl", "inaccuracy", "mistake" and "blunder".
QVariantMap exportedPlayer(const QJsonObject &player);

} // namespace GameInfo

#endif // GAMEINFO_H
