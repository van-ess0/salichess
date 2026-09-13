// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "stockfishengine.h"

#include "nnueweights.h"

#include <QDebug>

#include "bitboard.h"
#include "engine.h"
#include "misc.h"
#include "position.h"
#include "search.h"
#include "tune.h"
#include "ucioption.h"

#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Stockfish parses "startpos" in its UCI layer, not in Position::set, which
// asserts on anything that is not a real FEN.
const char StartFen[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// std::visit needs one callable with an overload per alternative.
template<typename... Ts>
struct Overload : Ts... {
    using Ts::operator()...;
};
template<typename... Ts>
Overload(Ts...) -> Overload<Ts...>;

// The move tables Stockfish looks moves up in are global and are filled in
// by its own main(), which this app does not have. Without them the very
// first position it is given walks off the end of them.
void initStockfishTables()
{
    static std::once_flag once;
    std::call_once(once, []() {
        Stockfish::Bitboards::init();
        Stockfish::Position::init();
    });
}

} // namespace

void StockfishEngine::readScore(const Stockfish::Score &score, int &cp, int &mate)
{
    cp = 0;
    mate = 0;
    score.visit(Overload{
        [&](Stockfish::Score::Mate m) {
            mate = (m.plies > 0 ? (m.plies + 1) : m.plies) / 2;
            // A mate that is already on the board still needs a sign.
            if (mate == 0)
                mate = m.plies > 0 ? 1 : -1;
        },
        [&](Stockfish::Score::Tablebase tb) { cp = tb.win ? 20000 - tb.plies : -20000 - tb.plies; },
        [&](Stockfish::Score::InternalUnits units) { cp = units.value; },
    });
}

StockfishEngine::StockfishEngine(QObject *parent)
    : QObject(parent)
{
}

StockfishEngine::~StockfishEngine()
{
    unload();
}

bool StockfishEngine::ensureEngine()
{
    if (m_engine)
        return true;
    initStockfishTables();
    // Without a path Stockfish looks for its networks in the working
    // directory, which is not where ours are; they are named in full later.
    m_engine.reset(new Stockfish::Engine());
    // Stockfish's search constants are tunable at run time, and the tuner
    // has to be told about the options once the engine owns them.
    Stockfish::Tune::init(m_engine->get_options());

    // Every callback Stockfish may reach for has to be set. It calls them
    // without checking, so an unset one is an empty std::function and throws
    // std::bad_function_call, which ends the whole app rather than the
    // search. onUpdateNoMoves is the one that bites: it is called whenever
    // the position has no legal moves, which is every checkmate and
    // stalemate, and the last position of a game that ended in mate is
    // exactly what the analysis board opens on.
    m_engine->set_on_update_full([this](const Stockfish::Engine::InfoFull &full) {
        int cp = 0;
        int mate = 0;
        readScore(full.score, cp, mate);
        // Called from a search thread: the signal is queued to whoever is
        // listening on another one.
        emit info(full.depth, int(full.multiPV), cp, mate,
                  QString::fromUtf8(full.pv.data(), int(full.pv.size())));
    });

    m_engine->set_on_update_no_moves([this](const Stockfish::Engine::InfoShort &short_) {
        int cp = 0;
        int mate = 0;
        readScore(short_.score, cp, mate);
        // No moves to give, but the score still says how it ended.
        emit info(short_.depth, 1, cp, mate, QString());
    });

    m_engine->set_on_iter([](const Stockfish::Engine::InfoIter &) {
        // Which move the search is on is of no use here, but it must not be
        // left unset.
    });

    m_engine->set_on_bestmove([this](std::string_view, std::string_view) {
        emit searchFinished();
    });

    // Stockfish checks its networks before every single search and says so
    // each time. Only the complaint matters: it ends the process right after
    // making it. The rest is the same two lines over and over, so they are
    // kept to once per load and out of the warnings.
    m_engine->set_on_verify_networks([this](std::string_view message) {
        const QString text =
                QString::fromUtf8(message.data(), int(message.size())).trimmed();
        if (text.contains(QLatin1String("ERROR"))) {
            qWarning("Stockfish: %s", qPrintable(text));
            return;
        }
        if (m_verifyMessages < 2) {
            ++m_verifyMessages; // one for each of the two networks
            qDebug("Stockfish: %s", qPrintable(text));
        }
    });

    applyOption("Threads", m_threads);
    applyOption("Hash", m_hashMb);
    applyOption("MultiPV", m_multiPv);
    return true;
}

void StockfishEngine::applyOption(const char *name, const std::string &value)
{
    if (!m_engine)
        return;
    // Options are set the way a UCI "setoption" line would, which is what
    // makes Stockfish act on them (resizing the thread pool, reading a
    // network, ...).
    std::istringstream command("name " + std::string(name) + " value " + value);
    m_engine->get_options().setoption(command);
}

void StockfishEngine::applyOption(const char *name, int value)
{
    applyOption(name, std::to_string(value));
}

void StockfishEngine::load(const QString &bigNetwork, const QString &smallNetwork)
{
    // Stockfish calls exit() when a search starts without a network, so the
    // files are checked here before it ever sees them.
    if (!NnueWeights::looksUsable(bigNetwork) || !NnueWeights::looksUsable(smallNetwork)) {
        m_ready = false;
        emit readyChanged(false);
        emit failed(tr("The engine networks are missing"));
        return;
    }
    if (!ensureEngine())
        return;

    m_verifyMessages = 0; // say which networks these are, once
    applyOption("EvalFile", bigNetwork.toStdString());
    applyOption("EvalFileSmall", smallNetwork.toStdString());

    m_ready = true;
    emit readyChanged(true);
}

void StockfishEngine::unload()
{
    if (!m_engine)
        return;
    m_engine->stop();
    m_engine->wait_for_search_finished();
    m_engine.reset();
    m_searching = false;
    if (m_ready) {
        m_ready = false;
        emit readyChanged(false);
    }
}

void StockfishEngine::setThreads(int threads)
{
    m_threads = qBound(1, threads, 8);
    applyOption("Threads", m_threads);
}

void StockfishEngine::setHashSize(int megabytes)
{
    m_hashMb = qBound(1, megabytes, 256);
    applyOption("Hash", m_hashMb);
}

void StockfishEngine::setMultiPv(int lines)
{
    m_multiPv = qBound(1, lines, 5);
    applyOption("MultiPV", m_multiPv);
}

void StockfishEngine::search(const QString &fen, const QStringList &moves, int depth)
{
    if (!m_ready || !m_engine)
        return;
    stop();

    std::vector<std::string> uciMoves;
    uciMoves.reserve(moves.size());
    for (const QString &move : moves)
        uciMoves.push_back(move.toStdString());
    const bool start = fen.isEmpty() || fen == QLatin1String("startpos");
    m_engine->set_position(start ? std::string(StartFen) : fen.toStdString(), uciMoves);

    Stockfish::Search::LimitsType limits;
    limits.startTime = Stockfish::now();
    limits.depth = qBound(1, depth, 40);
    m_searching = true;
    m_engine->go(limits);
}

void StockfishEngine::stop()
{
    if (!m_engine || !m_searching)
        return;
    m_engine->stop();
    m_engine->wait_for_search_finished();
    m_searching = false;
}
