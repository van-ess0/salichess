// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TESTDATA_H
#define TESTDATA_H

// Daily puzzle example from the Lichess API documentation.
static const char DailyPuzzle[] = R"({
  "game": {
    "id": "HxbFI25U",
    "pgn": "c4 Nf6 Nc3 g6 Nf3 Bg7 g3 d6 Bg2 O-O O-O e5 d3 Nbd7 Rb1 c6 e4 a5 a3 Nh5 Bg5 f6 Be3 Nc5 b4 Ne6 b5 Bd7 a4 Qe8 Qd2 f5 exf5 gxf5 Bh6 Rf7 Nh4 f4 Bxg7 Rxg7 Nf5 Rg5 Nxd6 Qg6 Nce4 fxg3 fxg3 Nef4 gxf4 Nxf4 Rxf4 exf4 Nxg5 Qxg5 Rf1 Qc5+ Kh1 Qxd6 bxc6 Bxc6 Rxf4 Qe5 Rg4+ Kh8 d4 Bxg2+ Rxg2 Qe4 Kg1 Qb1+ Kf2 Rf8+ Ke3"
  },
  "puzzle": {
    "id": "1Sqyb",
    "rating": 1857,
    "plays": 60048,
    "solution": ["b1b3", "d2d3", "f8f3", "e3f3", "b3d3"],
    "themes": ["deflection", "endgame", "crushing", "attraction", "long"],
    "initialPly": 72
  }
})";

static const char DailyPuzzleFen[] = "5r1k/1p5p/8/p7/P1PP4/4K3/3Q2RP/1q6 b - -";

// An exported game, as /api/games/user/{username} streams them: the player
// "me" lost with black. Written the way the endpoint answers the query the
// history makes (lastFen and the opening, no moves).
static const char HistoryGame[] = R"({
  "id": "hist0001",
  "rated": true,
  "variant": "standard",
  "speed": "blitz",
  "perf": "blitz",
  "createdAt": 1700000200000,
  "lastMoveAt": 1700000900000,
  "status": "mate",
  "winner": "white",
  "players": {
    "white": {"user": {"name": "Rival", "title": "FM", "id": "rival"}, "rating": 2100, "ratingDiff": 6},
    "black": {"user": {"name": "Me", "id": "me"}, "rating": 1500, "ratingDiff": -8}
  },
  "opening": {"eco": "C50", "name": "Italian Game", "ply": 5},
  "lastFen": "rnbqkbnr\/pppp1ppp\/8\/4p3\/4P3\/8\/PPPP1PPP\/RNBQKBNR w KQkq - 0 2"
})";

// An older game, won with white against a computer.
static const char HistoryGameOlder[] = R"({
  "id": "hist0002",
  "rated": false,
  "variant": "standard",
  "speed": "rapid",
  "perf": "rapid",
  "createdAt": 1700000100000,
  "status": "resign",
  "winner": "white",
  "players": {
    "white": {"user": {"name": "Me", "id": "me"}, "rating": 1490},
    "black": {"aiLevel": 3}
  },
  "lastFen": "8\/8\/8\/8\/8\/8\/8\/K6k w - - 0 1"
})";

// One game with a Lichess computer analysis: four moves, a blunder on the
// third, and clocks. "eval" is from white's point of view, and entry i
// belongs to the position after move i + 1.
static const char AnalysedGame[] = R"({
  "id": "anal0001",
  "rated": true,
  "variant": "standard",
  "speed": "blitz",
  "perf": "blitz",
  "createdAt": 1700000000000,
  "status": "resign",
  "winner": "black",
  "moves": "e4 e5 Qh5 Nc6",
  "clocks": [30010, 29850, 28700, 28010],
  "opening": {"eco": "C20", "name": "Open Game", "ply": 2},
  "players": {
    "white": {"user": {"name": "Me", "id": "me"}, "rating": 1500, "ratingDiff": -7,
              "analysis": {"inaccuracy": 1, "mistake": 0, "blunder": 1, "acpl": 120, "accuracy": 61}},
    "black": {"user": {"name": "Rival", "title": "FM", "id": "rival"}, "rating": 1560, "ratingDiff": 7,
              "analysis": {"inaccuracy": 0, "mistake": 1, "blunder": 0, "acpl": 40, "accuracy": 88}}
  },
  "analysis": [
    {"eval": 20},
    {"eval": 15},
    {"eval": -90, "best": "g1f3", "variation": "Nf3 Nc6",
     "judgment": {"name": "Blunder", "comment": "Blunder. Nf3 was best."}},
    {"eval": -80}
  ]
})";

// The same game as Lichess never analysed it.
static const char UnanalysedGame[] = R"({
  "id": "anal0002",
  "rated": true,
  "variant": "standard",
  "speed": "blitz",
  "perf": "blitz",
  "status": "resign",
  "winner": "black",
  "moves": "e4 e5",
  "players": {
    "white": {"user": {"name": "Me", "id": "me"}, "rating": 1500},
    "black": {"user": {"name": "Rival", "id": "rival"}, "rating": 1560}
  }
})";

// A variant the rules engine does not play.
static const char CrazyhouseGame[] = R"({
  "id": "anal0003",
  "rated": true,
  "variant": "crazyhouse",
  "speed": "blitz",
  "perf": "crazyhouse",
  "status": "mate",
  "winner": "white",
  "moves": "e4 e5",
  "players": {
    "white": {"user": {"name": "Me", "id": "me"}, "rating": 1500},
    "black": {"user": {"name": "Rival", "id": "rival"}, "rating": 1560}
  }
})";

#endif // TESTDATA_H
