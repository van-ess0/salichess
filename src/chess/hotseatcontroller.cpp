// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "hotseatcontroller.h"

#include "chessgame.h"

namespace {

const int kTickMs = 200;
// The longest a side may be given, so a slip of the finger cannot set a clock
// nobody will ever see run out.
const int kMaxSeconds = 180 * 60;

} // namespace

HotseatController::HotseatController(QObject *parent)
    : QObject(parent)
    , m_game(new ChessGame(this))
{
    m_clockTimer.setInterval(kTickMs);
    connect(&m_clockTimer, &QTimer::timeout, this, &HotseatController::tick);
}

QString HotseatController::other(const QString &color)
{
    return color == QLatin1String("white") ? QStringLiteral("black") : QStringLiteral("white");
}

void HotseatController::setInitialSeconds(int seconds)
{
    seconds = qBound(0, seconds, kMaxSeconds);
    if (m_initialSeconds == seconds)
        return;
    m_initialSeconds = seconds;
    emit timeControlChanged();
}

void HotseatController::setIncrement(int seconds)
{
    seconds = qBound(0, seconds, 180);
    if (m_increment == seconds)
        return;
    m_increment = seconds;
    emit timeControlChanged();
}

void HotseatController::setAutoSwitch(bool automatic)
{
    if (m_autoSwitch == automatic)
        return;
    m_autoSwitch = automatic;
    // The player who owed a press no longer does.
    if (!m_awaitingPress.isEmpty() && automatic) {
        switchTo(other(m_awaitingPress));
        emit stateChanged();
        emit clockChanged();
    }
    emit autoSwitchChanged();
}

QString HotseatController::timeControlText() const
{
    if (untimed())
        return tr("No clock");
    const int minutes = m_initialSeconds / 60;
    const int seconds = m_initialSeconds % 60;
    const QString initial = seconds == 0
            ? QString::number(minutes)
            : (minutes == 0 ? tr("%1 s").arg(seconds)
                            : QStringLiteral("%1:%2").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0')));
    return QStringLiteral("%1+%2").arg(initial).arg(m_increment);
}

QString HotseatController::sideToMove() const
{
    return m_game->sideToMove();
}

QString HotseatController::runningClock() const
{
    if (m_state != Running || untimed())
        return QString();
    return m_clockOwner;
}

int HotseatController::whiteTime() const
{
    if (runningClock() == QLatin1String("white") && m_clockStamp.isValid())
        return int(qMax<qint64>(0, m_whiteTime - m_clockStamp.elapsed()));
    return int(m_whiteTime);
}

int HotseatController::blackTime() const
{
    if (runningClock() == QLatin1String("black") && m_clockStamp.isValid())
        return int(qMax<qint64>(0, m_blackTime - m_clockStamp.elapsed()));
    return int(m_blackTime);
}

void HotseatController::start(const QString &fen)
{
    m_game->reset(fen);
    m_whiteTime = qint64(m_initialSeconds) * 1000;
    m_blackTime = m_whiteTime;
    m_clockOwner = sideToMove();
    m_awaitingPress.clear();
    m_status.clear();
    m_winner.clear();
    m_state = Running;
    m_clockStamp.restart();
    updateClockTimer();
    emit stateChanged();
    emit clockChanged();
}

void HotseatController::move(const QString &uci)
{
    if (!acceptsMoves())
        return;
    const QString played = m_game->position().normalizeUci(uci);
    if (played.isEmpty())
        return;

    const QString mover = sideToMove();
    m_game->playUci(played);

    // A move alone does not hand the clock over: pressing the clock does,
    // unless the players asked for that to happen by itself.
    if (m_autoSwitch || untimed())
        switchTo(other(mover));
    else
        m_awaitingPress = mover;

    // Mate or a draw by the rules ends the game whatever the clocks say, and
    // leaves nothing to press.
    checkOutcome();

    emit stateChanged();
    emit clockChanged();
}

void HotseatController::pressClock()
{
    if (m_state != Running || m_awaitingPress.isEmpty())
        return;
    switchTo(other(m_awaitingPress));
    emit stateChanged();
    emit clockChanged();
}

void HotseatController::switchTo(const QString &color)
{
    if (m_state != Running)
        return;
    chargeElapsed();
    // Fischer increment: credited to the side that has just finished its turn.
    if (!untimed() && !m_clockOwner.isEmpty()) {
        qint64 &finished = m_clockOwner == QLatin1String("white") ? m_whiteTime : m_blackTime;
        finished += qint64(m_increment) * 1000;
    }
    m_clockOwner = color;
    m_awaitingPress.clear();
    m_clockStamp.restart();
    updateClockTimer();
}

void HotseatController::chargeElapsed()
{
    if (untimed() || m_clockOwner.isEmpty() || !m_clockStamp.isValid())
        return;
    const qint64 elapsed = m_clockStamp.elapsed();
    qint64 &spent = m_clockOwner == QLatin1String("white") ? m_whiteTime : m_blackTime;
    spent = qMax<qint64>(0, spent - elapsed);
    m_clockStamp.restart();
}

void HotseatController::updateClockTimer()
{
    if (runningClock().isEmpty()) {
        m_clockTimer.stop();
        return;
    }
    if (!m_clockTimer.isActive())
        m_clockTimer.start();
}

void HotseatController::tick()
{
    if (m_state == Running && !untimed()) {
        const int left = m_clockOwner == QLatin1String("white") ? whiteTime() : blackTime();
        if (left <= 0) {
            flag();
            emit stateChanged();
            emit clockChanged();
            return;
        }
    }
    emit clockChanged();
}

void HotseatController::checkOutcome()
{
    const QString outcome = m_game->outcome();
    if (outcome.isEmpty())
        return;
    // After the mating move it is the mated side's turn.
    const QString winner = outcome == QLatin1String("checkmate") ? other(sideToMove()) : QString();
    finish(outcome, winner);
}

void HotseatController::flag()
{
    const QString loser = m_clockOwner;
    chargeElapsed(); // leaves the flagged clock at zero
    // A player who cannot mate with what is left on the board does not win on
    // the opponent's flag; the game is a draw.
    const QString opponent = other(loser);
    finish(QStringLiteral("timeout"), hasMatingMaterial(opponent) ? opponent : QString());
}

void HotseatController::resign(const QString &color)
{
    if (m_state != Running && m_state != Paused)
        return;
    if (color != QLatin1String("white") && color != QLatin1String("black"))
        return;
    finish(QStringLiteral("resign"), other(color));
}

void HotseatController::finish(const QString &status, const QString &winner)
{
    chargeElapsed();
    m_status = status;
    m_winner = winner;
    m_awaitingPress.clear();
    m_state = Finished;
    m_clockTimer.stop();
}

void HotseatController::pause()
{
    if (m_state != Running)
        return;
    chargeElapsed();
    m_state = Paused;
    m_clockTimer.stop();
    emit stateChanged();
    emit clockChanged();
}

void HotseatController::resume()
{
    if (m_state != Paused)
        return;
    m_state = Running;
    m_clockStamp.restart();
    updateClockTimer();
    emit stateChanged();
    emit clockChanged();
}

void HotseatController::undoMove()
{
    if (m_state == Setup || m_game->ply() == 0)
        return;
    chargeElapsed();
    m_game->undo();
    // Taking the mating move back puts the game back on.
    if (m_state == Finished) {
        m_status.clear();
        m_winner.clear();
        m_state = Running;
    }
    // Whoever has to move now holds the clock, with nothing left to press.
    m_awaitingPress.clear();
    m_clockOwner = sideToMove();
    m_clockStamp.restart();
    updateClockTimer();
    emit stateChanged();
    emit clockChanged();
}

bool HotseatController::hasMatingMaterial(const QString &color) const
{
    const bool white = color == QLatin1String("white");
    const std::array<char, 64> board = m_game->position().pieces();
    int minors = 0;
    for (char piece : board) {
        if (piece == '.')
            continue;
        const bool isWhite = piece >= 'A' && piece <= 'Z';
        if (isWhite != white)
            continue;
        switch (piece) {
        case 'P': case 'p':
        case 'R': case 'r':
        case 'Q': case 'q':
            return true;
        case 'N': case 'n':
        case 'B': case 'b':
            ++minors;
            break;
        default:
            break;
        }
    }
    // A lone king, or a king and one minor piece, cannot force mate.
    return minors >= 2;
}

QString HotseatController::resultText() const
{
    if (m_state != Finished)
        return QString();
    const bool white = m_winner == QLatin1String("white");
    if (m_status == QLatin1String("checkmate"))
        return white ? tr("Checkmate — White wins") : tr("Checkmate — Black wins");
    if (m_status == QLatin1String("resign"))
        return white ? tr("Black resigned — White wins") : tr("White resigned — Black wins");
    if (m_status == QLatin1String("timeout")) {
        if (m_winner.isEmpty())
            return tr("Out of time — draw, the other side cannot mate");
        return white ? tr("Black ran out of time — White wins") : tr("White ran out of time — Black wins");
    }
    if (m_status == QLatin1String("stalemate"))
        return tr("Draw by stalemate");
    return tr("Draw");
}

QString HotseatController::resultTextFor(const QString &color) const
{
    if (m_state != Finished)
        return QString();
    if (m_winner.isEmpty())
        return tr("Draw");
    return m_winner == color ? tr("You win") : tr("You lose");
}
