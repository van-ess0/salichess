// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef STOCKFISHENGINE_H
#define STOCKFISHENGINE_H

#include <QObject>
#include <QStringList>

#include <atomic>
#include <memory>
#include <string>

namespace Stockfish {
class Engine;
class Score;
}

// Stockfish, driven through its own Engine class rather than by writing UCI
// commands to another process: Harbour allows the app one binary, and the
// engine is part of it.
//
// This object does the slow work — starting the engine, reading the networks
// in, searching — so it is meant to live on a thread of its own, with
// EngineController calling into it from the UI thread. Its signals carry
// nothing but values, so they cross back safely.
//
// Careful: Stockfish ends the process if a search is started before its
// networks are in. load() is the only thing that opens the gate, and a
// search before it succeeds is refused here.
class StockfishEngine : public QObject
{
    Q_OBJECT
public:
    explicit StockfishEngine(QObject *parent = nullptr);
    ~StockfishEngine() override;

    bool isReady() const { return m_ready; }

public slots:
    // Starts the engine if it is not running yet and reads the two networks.
    // Both paths must already have been checked with NnueWeights.
    void load(const QString &bigNetwork, const QString &smallNetwork);
    // Frees the engine and everything it holds.
    void unload();
    void setThreads(int threads);
    void setHashSize(int megabytes);
    void setMultiPv(int lines);
    // 0 (weakest) to 20 (full strength). Only a game against the engine
    // wants anything but the default.
    void setSkillLevel(int level);
    // Searches the position after |moves| (UCI) from |fen| until |depth|.
    void search(const QString &fen, const QStringList &moves, int depth);
    // Picks a move for the side to move, within |depth| plies and
    // |movetimeMs|, whichever ends the search first. The answer comes back
    // as bestMove() with the same |requestId|, so that one for a position
    // the game has left again can be told from the one wanted.
    void play(const QString &fen, const QStringList &moves, int depth, int movetimeMs, int requestId);
    void stop();

signals:
    void readyChanged(bool ready);
    void failed(const QString &error);
    // One line of the search: |multiPv| counts from 1, |scoreCp| is in
    // centipawns and |mateIn| in moves, from the side to move's point of
    // view. Exactly one of them says anything: mateIn is 0 when the score is
    // a centipawn one.
    void info(int depth, int multiPv, int scoreCp, int mateIn, const QString &pvUci);
    void searchFinished();
    // The move chosen by play(); empty if the position has no legal move.
    void bestMove(int requestId, const QString &uci);

private:
    void startSearch(const QString &fen, const QStringList &moves, int depth, int movetimeMs);
    bool ensureEngine();
    // Turns one of Stockfish's scores into centipawns or moves-to-mate.
    static void readScore(const Stockfish::Score &score, int &cp, int &mate);
    void applyOption(const char *name, const std::string &value);
    void applyOption(const char *name, int value);

    std::unique_ptr<Stockfish::Engine> m_engine;
    bool m_ready = false;      // networks in, searching allowed
    bool m_searching = false;
    int m_threads = 1;
    int m_hashMb = 16;
    int m_multiPv = 1;
    int m_skill = 20;
    // Set before each play() search, read by the callback of its thread.
    std::atomic<int> m_requestId{0};
    int m_verifyMessages = 0;  // how many "networks are fine" lines were logged
};

#endif // STOCKFISHENGINE_H
