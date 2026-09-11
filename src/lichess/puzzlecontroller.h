// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PUZZLECONTROLLER_H
#define PUZZLECONTROLLER_H

#include "puzzlelogic.h"

#include <QJsonObject>
#include <QObject>
#include <QQueue>
#include <QStringList>
#include <QTimer>

class ChessGame;

// Fetches Lichess puzzles, checks the player's moves, plays the opponent's
// replies and reports results. Created in QML:
//     PuzzleController { angle: "mix" }
class PuzzleController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ChessGame *game READ game CONSTANT)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(Feedback feedback READ feedback NOTIFY feedbackChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY stateChanged)

    // Theme or opening key, e.g. "mix", "mateIn2", "fork".
    Q_PROPERTY(QString angle READ angle WRITE setAngle NOTIFY angleChanged)
    // "easiest" ... "hardest"; empty uses the app setting.
    Q_PROPERTY(QString difficulty READ difficulty WRITE setDifficulty NOTIFY difficultyChanged)

    Q_PROPERTY(QString puzzleId READ puzzleId NOTIFY puzzleChanged)
    Q_PROPERTY(int puzzleRating READ puzzleRating NOTIFY puzzleChanged)
    Q_PROPERTY(int plays READ plays NOTIFY puzzleChanged)
    Q_PROPERTY(QStringList themes READ themes NOTIFY puzzleChanged)
    Q_PROPERTY(QString playerColor READ playerColor NOTIFY puzzleChanged)
    Q_PROPERTY(QString puzzleUrl READ puzzleUrl NOTIFY puzzleChanged)
    Q_PROPERTY(QString gameUrl READ gameUrl NOTIFY puzzleChanged)
    Q_PROPERTY(bool isDaily READ isDaily NOTIFY puzzleChanged)

    Q_PROPERTY(bool solved READ solved NOTIFY stateChanged)       // finished without mistakes
    Q_PROPERTY(bool hadMistake READ hadMistake NOTIFY stateChanged)
    // First hint: the piece to move; second hint: also its destination.
    Q_PROPERTY(int hintSquare READ hintSquare NOTIFY hintSquareChanged)
    Q_PROPERTY(int hintTarget READ hintTarget NOTIFY hintSquareChanged)
    Q_PROPERTY(int ratingDiff READ ratingDiff NOTIFY resultChanged)
    Q_PROPERTY(int userRating READ userRating NOTIFY resultChanged)
    Q_PROPERTY(bool resultSubmitted READ resultSubmitted NOTIFY resultChanged)

public:
    enum State { Idle, Loading, Playing, ShowingSolution, Finished, Error };
    Q_ENUM(State)
    enum Feedback { NoFeedback, GoodMove, WrongMove, Success, SolutionShown };
    Q_ENUM(Feedback)

    explicit PuzzleController(QObject *parent = nullptr);

    ChessGame *game() const { return m_game; }
    State state() const { return m_state; }
    Feedback feedback() const { return m_feedback; }
    QString errorString() const { return m_error; }

    QString angle() const { return m_angle; }
    void setAngle(const QString &angle);
    QString difficulty() const { return m_difficulty; }
    void setDifficulty(const QString &difficulty);

    QString puzzleId() const { return m_puzzleId; }
    int puzzleRating() const { return m_puzzleRating; }
    int plays() const { return m_plays; }
    QStringList themes() const { return m_themes; }
    QString playerColor() const { return m_playerColor; }
    QString puzzleUrl() const;
    QString gameUrl() const;
    bool isDaily() const { return m_isDaily; }

    bool solved() const { return m_state == Finished && !m_logic.failed(); }
    bool hadMistake() const { return m_logic.failed(); }
    int hintSquare() const { return m_hintSquare; }
    int hintTarget() const { return m_hintTarget; }
    int ratingDiff() const { return m_ratingDiff; }
    int userRating() const { return m_userRating; }
    bool resultSubmitted() const { return m_resultSubmitted; }

    Q_INVOKABLE void loadDaily();
    Q_INVOKABLE void loadNext();
    Q_INVOKABLE void loadPuzzle(const QString &id);
    Q_INVOKABLE void move(const QString &uci);
    Q_INVOKABLE void showHint();
    Q_INVOKABLE void viewSolution();

    // Sets up a puzzle from Lichess puzzle JSON ({game, puzzle}).
    Q_INVOKABLE bool startPuzzle(const QJsonObject &puzzleAndGame, bool daily);

signals:
    void stateChanged();
    void feedbackChanged();
    void angleChanged();
    void difficultyChanged();
    void puzzleChanged();
    void hintSquareChanged();
    void resultChanged();

private:
    void fetchSingle(const QString &path, bool daily);
    void fetchBatch();
    void playLeadInMove();
    void playReply();
    void playNextSolutionMove();
    void finish();
    void submitResult(bool win);
    QString effectiveDifficulty() const;
    void setState(State state);
    void setFeedback(Feedback feedback);
    void setHint(int square, int target = -1);
    void setError(const QString &error);

    ChessGame *m_game;
    PuzzleLogic m_logic;
    QQueue<QJsonObject> m_queue;
    QString m_queueKey;
    QTimer m_moveTimer;
    enum class Pending { None, LeadIn, Reply, Undo, Solution };
    Pending m_pending = Pending::None;
    QString m_leadInMove;

    State m_state = Idle;
    Feedback m_feedback = NoFeedback;
    QString m_error;
    QString m_angle = QStringLiteral("mix");
    QString m_difficulty;

    QString m_puzzleId;
    QString m_gameId;
    int m_initialPly = 0;
    int m_puzzleRating = 0;
    int m_plays = 0;
    QStringList m_themes;
    QString m_playerColor;
    bool m_isDaily = false;
    bool m_hintUsed = false;
    int m_hintSquare = -1;
    int m_hintTarget = -1;
    int m_ratingDiff = 0;
    int m_userRating = 0;
    bool m_resultSubmitted = false;
};

#endif // PUZZLECONTROLLER_H
