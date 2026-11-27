// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SERVICES_H
#define SERVICES_H

class AppSettings;
class EngineController;
class LichessApi;
class NnueWeights;
class PuzzleStore;
class Session;

// App-wide service objects, created in main(). QML-instantiated controllers
// (GameController, PuzzleController, ...) need a default constructor, so they
// look their dependencies up here instead of receiving them.
namespace Services {

void init(LichessApi *api, Session *session, AppSettings *settings, PuzzleStore *puzzles,
          NnueWeights *weights = nullptr);

LichessApi *api();
Session *session();
AppSettings *settings();
PuzzleStore *puzzles();
// The engine networks; null where there is no engine (some tests).
NnueWeights *weights();
// The analysis engine, or null where there is none. A game against the
// computer asks it to give its memory back first: Stockfish 19 holds about
// 220 MB, and a phone should not hold two of them.
EngineController *engine();
void setEngine(EngineController *engine);

} // namespace Services

#endif // SERVICES_H
