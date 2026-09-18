// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef HOTSEATCONTROLLER_H
#define HOTSEATCONTROLLER_H

#include "chessposition.h"

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>

class ChessGame;

// A game two people play on one phone: no account, no network. The phone
// lies on the table between them, so the board is drawn with each side's
// pieces facing its own player and each player gets a big clock button on
// their own edge of the screen. Created in QML:
//     HotseatController { initialSeconds: 300; increment: 3 }
//
// The clock works like a real chess clock: playing a move does not hand it
// over, pressing your own button does, which is what the big button is for.
// autoSwitch hands it over the moment a move is played instead, for players
// who would rather not press anything.
class HotseatController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ChessGame *game READ game CONSTANT)

    // Time control. initialSeconds == 0 means no clock at all; the increment
    // is added to a side's clock when it hands the clock over.
    Q_PROPERTY(int initialSeconds READ initialSeconds WRITE setInitialSeconds NOTIFY timeControlChanged)
    Q_PROPERTY(int increment READ increment WRITE setIncrement NOTIFY timeControlChanged)
    Q_PROPERTY(bool untimed READ untimed NOTIFY timeControlChanged)
    Q_PROPERTY(QString timeControlText READ timeControlText NOTIFY timeControlChanged)
    Q_PROPERTY(bool autoSwitch READ autoSwitch WRITE setAutoSwitch NOTIFY autoSwitchChanged)

    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool gameOver READ gameOver NOTIFY stateChanged)
    // Whether the board should take moves: not while the game is paused or
    // over, and not while a player still owes a press of their clock.
    Q_PROPERTY(bool acceptsMoves READ acceptsMoves NOTIFY stateChanged)
    Q_PROPERTY(QString sideToMove READ sideToMove NOTIFY stateChanged)
    // The side that has moved and owes a press of its own clock, "" if none.
    Q_PROPERTY(QString awaitingPress READ awaitingPress NOTIFY stateChanged)

    Q_PROPERTY(int whiteTime READ whiteTime NOTIFY clockChanged) // ms
    Q_PROPERTY(int blackTime READ blackTime NOTIFY clockChanged)
    Q_PROPERTY(QString runningClock READ runningClock NOTIFY clockChanged) // "white", "black" or ""

    // "checkmate", "stalemate", "draw", "timeout", "resign" or "" while the
    // game is still on.
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString winner READ winner NOTIFY stateChanged) // "white", "black" or "" for a draw
    Q_PROPERTY(QString resultText READ resultText NOTIFY stateChanged)

public:
    enum State { Setup, Running, Paused, Finished };
    Q_ENUM(State)

    explicit HotseatController(QObject *parent = nullptr);

    ChessGame *game() const { return m_game; }

    int initialSeconds() const { return m_initialSeconds; }
    void setInitialSeconds(int seconds);
    int increment() const { return m_increment; }
    void setIncrement(int seconds);
    bool untimed() const { return m_initialSeconds <= 0; }
    QString timeControlText() const;
    bool autoSwitch() const { return m_autoSwitch; }
    void setAutoSwitch(bool automatic);

    State state() const { return m_state; }
    bool gameOver() const { return m_state == Finished; }
    bool acceptsMoves() const { return m_state == Running && m_awaitingPress.isEmpty(); }
    QString sideToMove() const;
    QString awaitingPress() const { return m_awaitingPress; }

    int whiteTime() const;
    int blackTime() const;
    QString runningClock() const;

    QString status() const { return m_status; }
    QString winner() const { return m_winner; }
    QString resultText() const;
    // The result as the player of |color| reads it, for their own button.
    Q_INVOKABLE QString resultTextFor(const QString &color) const;

    // Starts a new game, from |fen| if one is given. The clocks are set to
    // the time control as it stands and white's starts running.
    Q_INVOKABLE void start(const QString &fen = QString());
    Q_INVOKABLE void move(const QString &uci);
    // Ends the presser's turn: adds the increment and hands the clock over.
    Q_INVOKABLE void pressClock();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    // Takes the last move back, reopening the game if it had just ended.
    Q_INVOKABLE void undoMove();
    Q_INVOKABLE void resign(const QString &color);

signals:
    void timeControlChanged();
    void autoSwitchChanged();
    void stateChanged();
    void clockChanged();

private:
    void tick();
    // Charges the time since the last stamp to whoever holds the clock.
    void chargeElapsed();
    // Hands the clock to |color|, crediting the increment to the side that
    // held it. Does nothing once the game is over.
    void switchTo(const QString &color);
    void updateClockTimer();
    // Ends the game on a mate or a draw by the rules, if the position is one.
    void checkOutcome();
    void flag();
    void finish(const QString &status, const QString &winner);
    // Whether |color| has enough material left to mate at all: a bare king,
    // or a king and one minor piece, can only draw on the opponent's flag.
    bool hasMatingMaterial(const QString &color) const;
    static QString other(const QString &color);

    ChessGame *m_game;
    int m_initialSeconds = 300;
    int m_increment = 0;
    bool m_autoSwitch = false;

    State m_state = Setup;
    QString m_status;
    QString m_winner;
    QString m_awaitingPress;

    // Clock: the times as they stood at m_clockStamp, plus who is spending.
    qint64 m_whiteTime = 0;
    qint64 m_blackTime = 0;
    QString m_clockOwner;
    QElapsedTimer m_clockStamp;
    QTimer m_clockTimer;
};

#endif // HOTSEATCONTROLLER_H
