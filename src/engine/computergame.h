// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COMPUTERGAME_H
#define COMPUTERGAME_H

#include <QObject>
#include <QString>

class ChessGame;
class NnueWeights;
class QThread;
class StockfishEngine;

// A game against Stockfish on the phone: no account and no network, once the
// engine's networks are downloaded. Created in QML:
//     ComputerGame { level: 3 }
//
// The engine runs on a thread of its own, separate from the analysis one, and
// is freed again when this object goes. It is never started before the
// networks are checked (Stockfish ends the process when they are missing), so
// a game without them just waits, saying why, and carries on by itself when
// they arrive.
//
// The strength follows the levels of the Lichess AI, 1 to 8: Stockfish's skill
// level, a depth and a time limit per move.
class ComputerGame : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ChessGame *game READ game CONSTANT)
    Q_PROPERTY(int level READ level WRITE setLevel NOTIFY levelChanged)
    Q_PROPERTY(QString playerColor READ playerColor NOTIFY stateChanged) // "white" or "black"
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool gameOver READ gameOver NOTIFY stateChanged)
    Q_PROPERTY(bool isMyTurn READ isMyTurn NOTIFY stateChanged)
    // The engine is working out its move.
    Q_PROPERTY(bool thinking READ thinking NOTIFY stateChanged)
    // The computer is to move but cannot yet: the networks are not on the
    // phone (or are still being read in).
    Q_PROPERTY(bool waitingForEngine READ waitingForEngine NOTIFY stateChanged)
    // The networks are on the phone.
    Q_PROPERTY(bool engineAvailable READ engineAvailable NOTIFY engineAvailableChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY stateChanged)

    // "mate", "stalemate", "draw" or "resign", as Lichess names them, or ""
    // while the game is on.
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString winner READ winner NOTIFY stateChanged) // "white", "black" or ""
    Q_PROPERTY(QString resultText READ resultText NOTIFY stateChanged)

public:
    enum State { Setup, Playing, Finished };
    Q_ENUM(State)

    explicit ComputerGame(QObject *parent = nullptr);
    ~ComputerGame() override;

    ChessGame *game() const { return m_game; }
    int level() const { return m_level; }
    void setLevel(int level);
    QString playerColor() const { return m_playerColor; }
    State state() const { return m_state; }
    bool gameOver() const { return m_state == Finished; }
    bool isMyTurn() const;
    bool thinking() const { return m_requestId != 0; }
    bool waitingForEngine() const { return m_waiting; }
    bool engineAvailable() const;
    QString errorString() const { return m_error; }

    QString status() const { return m_status; }
    QString winner() const { return m_winner; }
    QString resultText() const;

    // Starts a game, taking |color| ("white", "black" or "random").
    Q_INVOKABLE void start(const QString &color);
    Q_INVOKABLE void move(const QString &uci);
    // Takes back the player's last move and the reply to it.
    Q_INVOKABLE void takeBack();
    Q_INVOKABLE void resign();
    // Asks the engine for its move again, after an error.
    Q_INVOKABLE void retry();
    Q_INVOKABLE void downloadEngine();

    // How the levels play: Stockfish's skill level, search depth and the
    // time one move may take.
    static int skillForLevel(int level);
    static int depthForLevel(int level);
    static int moveTimeForLevel(int level);

signals:
    void levelChanged();
    void stateChanged();
    void engineAvailableChanged();

private:
    void requestEngineMove();
    void ensureEngine();
    void onBestMove(int requestId, const QString &uci);
    void onEngineFailed(const QString &error);
    void cancelRequest();
    void checkOutcome();
    void finish(const QString &status, const QString &winner);
    void setError(const QString &error);
    QString computerColor() const;

    ChessGame *m_game;
    NnueWeights *m_weights;
    QThread *m_thread = nullptr;
    StockfishEngine *m_engine = nullptr;
    bool m_engineReady = false;
    bool m_loading = false;

    int m_level = 3;
    State m_state = Setup;
    QString m_playerColor = QStringLiteral("white");
    QString m_status;
    QString m_winner;
    QString m_error;
    bool m_waiting = false;
    // The search in flight, 0 if none. Answers for any other id are for a
    // position the game has left.
    int m_requestId = 0;
    int m_lastRequestId = 0;
};

#endif // COMPUTERGAME_H
