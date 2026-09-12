// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SERVICES_H
#define SERVICES_H

class AppSettings;
class LichessApi;
class PuzzleStore;
class Session;

// App-wide service objects, created in main(). QML-instantiated controllers
// (GameController, PuzzleController, ...) need a default constructor, so they
// look their dependencies up here instead of receiving them.
namespace Services {

void init(LichessApi *api, Session *session, AppSettings *settings, PuzzleStore *puzzles);

LichessApi *api();
Session *session();
AppSettings *settings();
PuzzleStore *puzzles();

} // namespace Services

#endif // SERVICES_H
