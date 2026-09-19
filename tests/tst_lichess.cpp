// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

#include "chess/chessgame.h"
#include "core/appsettings.h"
#include "core/lichessapi.h"
#include "core/ndjsonstream.h"
#include "core/services.h"
#include "core/session.h"
#include "core/tokenstore.h"
#include "fakelichess.h"
#include "lichess/challengesmodel.h"
#include "lichess/chatmodel.h"
#include "lichess/eventstream.h"
#include "lichess/friendsmodel.h"
#include "lichess/gameanalysis.h"
#include "lichess/gamecontroller.h"
#include "lichess/gameshistorymodel.h"
#include "lichess/lobbyseek.h"
#include "lichess/ongoinggamesmodel.h"
#include "lichess/outgoingchallenge.h"
#include "lichess/outgoingchallenges.h"
#include "lichess/puzzlecontroller.h"
#include "lichess/puzzlestore.h"
#include "testdata.h"

namespace {

class MemoryTokenStore : public TokenStore
{
public:
    QString load() override { return token; }
    bool save(const QString &value) override { token = value; return true; }
    void clear() override { token.clear(); }

    QString token;
};

const char AccountJson[] =
        R"({"id":"me","username":"Me","perfs":{"puzzle":{"rating":1600},"blitz":{"rating":1500}}})";

// One ndjson line; JSON written over several lines in the test is compacted.
QByteArray line(const char *text)
{
    const QJsonDocument doc = QJsonDocument::fromJson(text);
    Q_ASSERT(!doc.isNull());
    return doc.toJson(QJsonDocument::Compact) + '\n';
}

QVariantMap map(const char *text)
{
    return QJsonDocument::fromJson(text).object().toVariantMap();
}

// A puzzle batch: the daily puzzle under each of |ids|.
QByteArray puzzleBatch(const QStringList &ids)
{
    QJsonArray puzzles;
    for (const QString &id : ids) {
        QJsonObject entry = QJsonDocument::fromJson(DailyPuzzle).object();
        QJsonObject puzzle = entry.value(QStringLiteral("puzzle")).toObject();
        puzzle.insert(QStringLiteral("id"), id);
        entry.insert(QStringLiteral("puzzle"), puzzle);
        puzzles.append(entry);
    }
    QJsonObject body;
    body.insert(QStringLiteral("puzzles"), puzzles);
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

QString offlinePuzzlePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/offline-puzzles.json");
}

QVariant role(QAbstractItemModel &model, int row, const char *name)
{
    const QHash<int, QByteArray> roles = model.roleNames();
    return model.data(model.index(row, 0), roles.key(name));
}

} // namespace

// Everything that talks to Lichess, run against FakeLichess.
class TestLichess : public QObject
{
    Q_OBJECT

private:
    FakeLichess *server = nullptr;
    LichessApi *api = nullptr;
    MemoryTokenStore *store = nullptr;
    Session *session = nullptr;
    AppSettings *settings = nullptr;
    PuzzleStore *puzzles = nullptr;

    void logIn()
    {
        server->route("GET", "/api/account", 200, AccountJson);
        session->login(QStringLiteral("tok"));
        QTRY_VERIFY(session->loggedIn());
    }

    // Plays the daily test puzzle to the end without mistakes.
    void solve(PuzzleController &controller)
    {
        QTRY_COMPARE(controller.game()->ply(), 73);
        controller.move("b1b3");
        QTRY_COMPARE(controller.game()->ply(), 75);
        controller.move("f8f3");
        QTRY_COMPARE(controller.game()->ply(), 77);
        controller.move("b3d3");
        QCOMPARE(controller.state(), PuzzleController::Finished);
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        server = new FakeLichess;
        api = new LichessApi;
        api->setServerUrl(server->url());
        store = new MemoryTokenStore;
        session = new Session(api, store);
        settings = new AppSettings;
        // Each test starts without stored puzzles, and only asks for them
        // where it says so.
        settings->setOfflinePuzzles(0);
        QFile::remove(offlinePuzzlePath());
        puzzles = new PuzzleStore(api, session, settings);
        Services::init(api, session, settings, puzzles);
    }

    void cleanup()
    {
        Services::init(nullptr, nullptr, nullptr, nullptr);
        delete puzzles;
        delete settings;
        delete session;
        delete store;
        delete api;
        delete server;
    }

    // --- NdjsonStream ---

    void streamSplitsLines()
    {
        server->streamRoute("GET", "/stream");
        QScopedPointer<NdjsonStream> stream(api->openStream(QStringLiteral("/stream")));
        QSignalSpy opened(stream.data(), &NdjsonStream::opened);
        QSignalSpy messages(stream.data(), &NdjsonStream::message);
        QSignalSpy finished(stream.data(), &NdjsonStream::finished);
        QTRY_COMPARE(server->openStreams("/stream"), 1);

        // A keep-alive line and a message split across two writes.
        server->push("/stream", "{\"n\":1}\n\n{\"n\"");
        QTRY_COMPARE(messages.count(), 1);
        QCOMPARE(opened.count(), 1);
        server->push("/stream", ":2}\n");
        QTRY_COMPARE(messages.count(), 2);
        QCOMPARE(messages.at(1).first().toJsonObject().value("n").toInt(), 2);

        // The last line may lack a newline.
        server->push("/stream", "{\"n\":3}");
        server->closeStreams("/stream");
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(messages.count(), 3);
        QCOMPARE(finished.first().at(0).toInt(), 200);
        QCOMPARE(finished.first().at(1).toString(), QString());
    }

    void streamReportsHttpErrors()
    {
        QScopedPointer<NdjsonStream> stream(api->openStream(QStringLiteral("/nowhere")));
        QSignalSpy messages(stream.data(), &NdjsonStream::message);
        QSignalSpy finished(stream.data(), &NdjsonStream::finished);
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(finished.first().at(0).toInt(), 404);
        QCOMPARE(finished.first().at(1).toString(), QStringLiteral("Not found"));
        QCOMPARE(messages.count(), 0);
    }

    void streamIdleTimeout()
    {
        server->streamRoute("GET", "/quiet");
        QScopedPointer<NdjsonStream> stream(api->openStream(QStringLiteral("/quiet"), QUrlQuery(), 300));
        QSignalSpy finished(stream.data(), &NdjsonStream::finished);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
        QVERIFY(!finished.first().at(1).toString().isEmpty());
    }

    // --- LichessApi ---

    void requestsAreSerialized()
    {
        server->route("GET", "/a", 200, R"({"v":"a"})", 100);
        server->route("GET", "/b", 200, R"({"v":"b"})", 100);
        server->route("GET", "/c", 200, R"({"v":"c"})", 100);
        QStringList order;
        for (const char *path : { "/a", "/b", "/c" }) {
            api->get(QString::fromLatin1(path), QUrlQuery(), this, [&order](const ApiResult &result) {
                order.append(result.json.object().value("v").toString());
            });
        }
        QTRY_COMPARE(order.size(), 3);
        QCOMPARE(order, QStringList({ "a", "b", "c" }));
        QCOMPARE(server->maxConcurrent(), 1);
    }

    void requestHeadersAndBodies()
    {
        server->route("GET", "/get", 200, "{}");
        server->route("POST", "/form", 200, "{}");
        server->route("POST", "/json", 200, "{}");
        api->setToken(QStringLiteral("tok"));

        QUrlQuery query;
        query.addQueryItem("nb", "5");
        api->get(QStringLiteral("/get"), query, nullptr, nullptr);
        QUrlQuery form;
        form.addQueryItem("text", "good luck & have fun");
        api->postForm(QStringLiteral("/form"), form, nullptr, nullptr);
        api->postJson(QStringLiteral("/json"), QUrlQuery(), QJsonDocument(QJsonObject { { "x", 1 } }), nullptr, nullptr);
        QTRY_COMPARE(server->requests().size(), 3);

        const FakeLichess::Request get = server->lastRequest("GET", "/get");
        QCOMPARE(get.headers.value("authorization"), QByteArray("Bearer tok"));
        QVERIFY(get.headers.value("user-agent").contains("github.com/van-ess0/salichess"));
        QCOMPARE(get.query.queryItemValue("nb"), QStringLiteral("5"));

        const FakeLichess::Request post = server->lastRequest("POST", "/form");
        QCOMPARE(post.headers.value("content-type"), QByteArray("application/x-www-form-urlencoded"));
        QCOMPARE(post.form().queryItemValue("text", QUrl::FullyDecoded), QStringLiteral("good luck & have fun"));

        const FakeLichess::Request jsonPost = server->lastRequest("POST", "/json");
        QCOMPARE(jsonPost.headers.value("content-type"), QByteArray("application/json"));
        QCOMPARE(QJsonDocument::fromJson(jsonPost.body).object().value("x").toInt(), 1);
    }

    void errorMessages()
    {
        server->route("POST", "/field-error", 400, R"({"error":{"clock.limit":["Invalid clock"]}})");
        server->route("GET", "/plain-error", 400, R"({"error":"Not your turn"})");
        QList<ApiResult> results;
        auto collect = [&results](const ApiResult &result) { results.append(result); };
        api->postForm(QStringLiteral("/field-error"), QUrlQuery(), this, collect);
        api->get(QStringLiteral("/plain-error"), QUrlQuery(), this, collect);
        QTRY_COMPARE(results.size(), 2);
        QVERIFY(!results.at(0).ok());
        QCOMPARE(results.at(0).status, 400);
        QCOMPARE(results.at(0).errorString, QStringLiteral("Invalid clock"));
        QCOMPARE(results.at(1).errorString, QStringLiteral("Not your turn"));
    }

    void rateLimitPausesQueue()
    {
        server->route("GET", "/limited", 429, "{}");
        server->route("GET", "/next", 200, "{}");
        QSignalSpy limited(api, &LichessApi::rateLimitedChanged);
        ApiResult first;
        api->get(QStringLiteral("/limited"), QUrlQuery(), this, [&first](const ApiResult &r) { first = r; });
        api->get(QStringLiteral("/next"), QUrlQuery(), nullptr, nullptr);
        QTRY_VERIFY(api->rateLimited());
        QCOMPARE(limited.count(), 1);
        QCOMPARE(first.status, 429);
        QTest::qWait(300);
        QCOMPARE(server->requestCount("GET", "/next"), 0);
    }

    void unauthorizedSignal()
    {
        server->route("GET", "/secret", 401, R"({"error":"No such token"})");
        QSignalSpy unauthorized(api, &LichessApi::unauthorized);
        api->setToken(QStringLiteral("tok"));
        api->get(QStringLiteral("/secret"), QUrlQuery(), nullptr, nullptr);
        QTRY_COMPARE(unauthorized.count(), 1);
    }

    void callbackSkippedForDeletedContext()
    {
        server->route("GET", "/slow", 200, "{}", 100);
        server->route("GET", "/done", 200, "{}");
        bool called = false;
        bool done = false;
        QObject *context = new QObject;
        api->get(QStringLiteral("/slow"), QUrlQuery(), context, [&called](const ApiResult &) { called = true; });
        delete context;
        api->get(QStringLiteral("/done"), QUrlQuery(), this, [&done](const ApiResult &) { done = true; });
        QTRY_VERIFY(done);
        QVERIFY(!called);
    }

    // --- Session and storage ---

    void sessionLoginLogout()
    {
        QSignalSpy loggedIn(session, &Session::loggedInChanged);
        logIn();
        QCOMPARE(loggedIn.count(), 1);
        QCOMPARE(session->userId(), QStringLiteral("me"));
        QCOMPARE(session->username(), QStringLiteral("Me"));
        QCOMPARE(session->puzzleRating(), 1600);
        QCOMPARE(session->rating("blitz"), 1500);
        QCOMPARE(store->token, QStringLiteral("tok"));
        QCOMPARE(server->lastRequest("GET", "/api/account").headers.value("authorization"), QByteArray("Bearer tok"));

        server->route("DELETE", "/api/token", 204, "");
        session->logout();
        QVERIFY(session->busy());
        QTRY_VERIFY(!session->loggedIn());
        QVERIFY(!session->busy());
        QVERIFY(store->token.isEmpty());
        QVERIFY(!api->hasToken());
        // The token is revoked on the server with its own credentials.
        QCOMPARE(server->requestCount("DELETE", "/api/token"), 1);
        QCOMPARE(server->lastRequest("DELETE", "/api/token").headers.value("authorization"), QByteArray("Bearer tok"));
    }

    void sessionLogoutWaitsForRevocation()
    {
        logIn();
        QSignalSpy logoutFailed(session, &Session::logoutFailed);
        QSignalSpy loginFailed(session, &Session::loginFailed);

        // Not revoked: the login stays, so that the user can try again.
        server->route("DELETE", "/api/token", 500, R"({"error":"Server error"})");
        session->logout();
        QTRY_COMPARE(logoutFailed.count(), 1);
        QVERIFY(logoutFailed.first().first().toString().contains("Server error"));
        QVERIFY(session->loggedIn());
        QVERIFY(!session->busy());
        QCOMPARE(store->token, QStringLiteral("tok"));
        QVERIFY(api->hasToken());

        // Invalid already: nothing left to revoke, and not an expired login.
        server->route("DELETE", "/api/token", 401, R"({"error":"No such token"})");
        session->logout();
        QTRY_VERIFY(!session->loggedIn());
        QVERIFY(store->token.isEmpty());
        QCOMPARE(loginFailed.count(), 0);
    }

    void sessionLoginRejected()
    {
        server->route("GET", "/api/account", 401, R"({"error":"No such token"})");
        QSignalSpy failed(session, &Session::loginFailed);
        session->login(QStringLiteral("bad"));
        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(!session->loggedIn());
        QVERIFY(store->token.isEmpty());
        QVERIFY(!api->hasToken());
    }

    void sessionRestoreRevokedToken()
    {
        server->route("GET", "/api/account", 401, R"({"error":"No such token"})");
        store->token = QStringLiteral("old");
        QSignalSpy failed(session, &Session::loginFailed);
        session->restore();
        QVERIFY(session->loggedIn()); // optimistic until the server answers
        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(!session->loggedIn());
        QVERIFY(store->token.isEmpty());
    }

    void settingsPersist()
    {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir(dir).removeRecursively();

        {
            AppSettings settings;
            QCOMPARE(settings.boardTheme(), QStringLiteral("brown"));
            QVERIFY(settings.showCoordinates());
            QSignalSpy changed(&settings, &AppSettings::boardThemeChanged);
            settings.setBoardTheme(QStringLiteral("blue"));
            settings.setBoardTheme(QStringLiteral("blue"));
            QCOMPARE(changed.count(), 1);
            settings.setShowCoordinates(false);
        }
        AppSettings reloaded;
        QCOMPARE(reloaded.boardTheme(), QStringLiteral("blue"));
        QVERIFY(!reloaded.showCoordinates());
        QDir(dir).removeRecursively();
    }

    // --- Event stream and models ---

    void eventStreamDispatches()
    {
        server->streamRoute("GET", "/api/stream/event");
        EventStream events(api);
        QSignalSpy started(&events, &EventStream::gameStarted);
        QSignalSpy challenged(&events, &EventStream::challengeReceived);
        QSignalSpy declined(&events, &EventStream::challengeDeclined);
        events.start();
        QTRY_COMPARE(server->openStreams("/api/stream/event"), 1);

        server->push("/api/stream/event", "\n"); // keep-alive
        QTRY_VERIFY(events.connected());
        server->push("/api/stream/event", line(R"({"type":"gameStart","game":{"gameId":"g1"}})")
                     + line(R"({"type":"challenge","challenge":{"id":"c1"}})")
                     + line(R"({"type":"challengeDeclined","challenge":{"id":"c2"}})"));
        QTRY_COMPARE(declined.count(), 1);
        QCOMPARE(started.first().first().toMap().value("gameId").toString(), QStringLiteral("g1"));
        QCOMPARE(challenged.first().first().toMap().value("id").toString(), QStringLiteral("c1"));

        // A dropped connection is re-established.
        server->closeStreams("/api/stream/event");
        QTRY_VERIFY(!events.connected());
        QTRY_COMPARE_WITH_TIMEOUT(server->requestCount("GET", "/api/stream/event"), 2, 5000);

        events.stop();
        QVERIFY(!events.connected());
    }

    void challengesModel()
    {
        logIn();
        EventStream events(api);
        ChallengesModel model(api, &events);
        QSignalSpy incoming(&model, &ChallengesModel::newIncomingChallenge);

        const QVariantMap fromFriend = map(R"({"id":"c1","status":"created",
            "challenger":{"id":"friend","name":"Friend","rating":1500},"destUser":{"id":"me","name":"Me"},
            "rated":true,"speed":"blitz","color":"white","perf":{"name":"Blitz"},"variant":{"name":"Standard"},
            "timeControl":{"type":"clock","limit":300,"increment":3,"show":"5+3"}})");
        emit events.challengeReceived(fromFriend);
        emit events.challengeReceived(fromFriend); // resent after a reconnect
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.incomingCount(), 1);
        QCOMPARE(incoming.count(), 1);
        QCOMPARE(incoming.first().at(0).toString(), QStringLiteral("c1"));
        QVERIFY(incoming.first().at(2).toString().contains("5+3"));
        QCOMPARE(role(model, 0, "direction").toString(), QStringLiteral("in"));
        QCOMPARE(role(model, 0, "opponentName").toString(), QStringLiteral("Friend"));
        QCOMPARE(role(model, 0, "myColor").toString(), QStringLiteral("black"));

        emit events.challengeReceived(map(R"({"id":"c2","status":"created",
            "challenger":{"id":"me","name":"Me"},"destUser":{"id":"pal","name":"Pal"},
            "timeControl":{"type":"correspondence","daysPerTurn":3},"perf":{"name":"Correspondence"}})"));
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.incomingCount(), 1);
        QCOMPARE(role(model, 1, "direction").toString(), QStringLiteral("out"));
        QCOMPARE(role(model, 1, "opponentName").toString(), QStringLiteral("Pal"));
        QCOMPARE(role(model, 1, "timeControl").toString(), QStringLiteral("3 days"));

        // The game of an accepted challenge has the challenge's id.
        emit events.gameStarted(map(R"({"gameId":"c2"})"));
        QCOMPARE(model.rowCount(), 1);

        server->route("POST", "/api/challenge/c1/accept", 200, R"({"ok":true})");
        QSignalSpy accepted(&model, &ChallengesModel::challengeAccepted);
        model.accept(QStringLiteral("c1"));
        QTRY_COMPARE(accepted.count(), 1);
        QCOMPARE(accepted.first().first().toString(), QStringLiteral("c1"));
        QCOMPARE(model.rowCount(), 0);

        server->route("GET", "/api/challenge", 200, R"({"in":[{"id":"c3","challenger":{"id":"x","name":"X"},
            "timeControl":{"type":"unlimited"}}],"out":[]})");
        model.refresh();
        QTRY_COMPARE(model.rowCount(), 1);
        QCOMPARE(role(model, 0, "direction").toString(), QStringLiteral("in"));
        QCOMPARE(role(model, 0, "timeControl").toString(), QStringLiteral("Unlimited"));
    }

    void ongoingGames()
    {
        logIn();
        EventStream events(api);
        OngoingGamesModel model(api, &events);
        QSignalSpy myTurn(&model, &OngoingGamesModel::myTurn);

        server->route("GET", "/api/account/playing", 200, R"({"nowPlaying":[
            {"gameId":"g1","isMyTurn":false,"opponent":{"username":"Friend"},"color":"white"},
            {"gameId":"g2","isMyTurn":true,"opponent":{"username":"Other"},"color":"black",
             "compat":{"board":false}}]})");
        model.refresh();
        QTRY_COMPARE(model.count(), 2);
        QCOMPARE(server->lastRequest("GET", "/api/account/playing").query.queryItemValue("nb"), QStringLiteral("50"));
        QCOMPARE(model.myTurnCount(), 1);
        QCOMPARE(model.myTurnOpponents(), QStringList({ "Other" }));
        QCOMPARE(myTurn.count(), 0); // not reported for the first load
        QCOMPARE(role(model, 0, "boardCompatible").toBool(), true);
        QCOMPARE(role(model, 1, "boardCompatible").toBool(), false);

        server->route("GET", "/api/account/playing", 200, R"({"nowPlaying":[
            {"gameId":"g1","isMyTurn":true,"opponent":{"username":"Friend"},"color":"white"},
            {"gameId":"g2","isMyTurn":true,"opponent":{"username":"Other"},"color":"black"}]})");
        model.refresh();
        QTRY_COMPARE(myTurn.count(), 1);
        QCOMPARE(myTurn.first().at(0).toString(), QStringLiteral("g1"));
        QCOMPARE(myTurn.first().at(1).toString(), QStringLiteral("Friend"));
        QCOMPARE(model.myTurnOpponents().size(), 2);
    }

    void ongoingGamesTurnDeadline()
    {
        logIn();
        EventStream events(api);
        OngoingGamesModel model(api, &events);
        auto load = [&](const char *json) {
            server->route("GET", "/api/account/playing", 200, json);
            model.refresh();
            QTRY_VERIFY(!model.loading());
        };
        // g1 waits for the opponent: the list only holds my own allowance for
        // the move after theirs, so the game itself is asked for.
        server->route("GET", "/game/export/g1", 200,
                      R"({"id":"g1","lastMoveAt":1700000000000,"daysPerTurn":3})");
        load(R"({"nowPlaying":[
            {"gameId":"g1","isMyTurn":false,"lastMove":"e2e4","speed":"correspondence","secondsLeft":259200},
            {"gameId":"g2","isMyTurn":true,"lastMove":"d7d5","speed":"correspondence","secondsLeft":3600},
            {"gameId":"g3","isMyTurn":false,"speed":"rapid","secondsLeft":600}]})");
        QCOMPARE(model.count(), 3);
        QTRY_COMPARE(role(model, 0, "turnDeadline").toLongLong(), Q_INT64_C(1700259200000));
        QCOMPARE(server->lastRequest("GET", "/game/export/g1").query.queryItemValue("moves"),
                 QStringLiteral("false"));
        // My own turn is counted from the load; a game with a clock has no
        // turn deadline.
        const qint64 mine = role(model, 1, "turnDeadline").toLongLong();
        QVERIFY(qAbs(mine - (QDateTime::currentMSecsSinceEpoch() + 3600 * 1000)) < 5000);
        QCOMPARE(role(model, 2, "turnDeadline").toLongLong(), Q_INT64_C(0));

        // The same move is not asked about again.
        load(R"({"nowPlaying":[
            {"gameId":"g1","isMyTurn":false,"lastMove":"e2e4","speed":"correspondence","secondsLeft":259200}]})");
        QTest::qWait(100);
        QCOMPARE(server->requestCount("GET", "/game/export/g1"), 1);
        QCOMPARE(role(model, 0, "turnDeadline").toLongLong(), Q_INT64_C(1700259200000));

        // The opponent moved: a new deadline.
        server->route("GET", "/game/export/g1", 200,
                      R"({"id":"g1","lastMoveAt":1700100000000,"daysPerTurn":3})");
        load(R"({"nowPlaying":[
            {"gameId":"g1","isMyTurn":false,"lastMove":"g1f3","speed":"correspondence","secondsLeft":259200}]})");
        QTRY_COMPARE(role(model, 0, "turnDeadline").toLongLong(), Q_INT64_C(1700359200000));
        QCOMPARE(server->requestCount("GET", "/game/export/g1"), 2);
    }

    void ongoingGamesUpdateRowsInPlace()
    {
        logIn();
        EventStream events(api);
        OngoingGamesModel model(api, &events);
        auto load = [&](const char *json) {
            server->route("GET", "/api/account/playing", 200, json);
            model.refresh();
            QTRY_VERIFY(!model.loading());
        };
        const char *twoGames = R"({"nowPlaying":[
            {"gameId":"g1","isMyTurn":false,"opponent":{"username":"A"}},
            {"gameId":"g2","isMyTurn":true,"opponent":{"username":"B"}}]})";
        load(twoGames);
        QCOMPARE(model.count(), 2);

        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        QSignalSpy moved(&model, &QAbstractItemModel::rowsMoved);
        QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);

        // Nothing changed: the delegates (mini boards) are left alone.
        load(twoGames);
        QCOMPARE(reset.count() + changed.count() + moved.count() + inserted.count() + removed.count(), 0);

        // g2 moves up and changes, g3 is new, g1 stays as it is.
        load(R"({"nowPlaying":[
            {"gameId":"g2","isMyTurn":false,"opponent":{"username":"B"}},
            {"gameId":"g3","isMyTurn":true,"opponent":{"username":"C"}},
            {"gameId":"g1","isMyTurn":false,"opponent":{"username":"A"}}]})");
        QCOMPARE(moved.count(), 1);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(inserted.count(), 1);
        QCOMPARE(removed.count(), 0);
        QCOMPARE(role(model, 0, "gameId").toString(), QStringLiteral("g2"));
        QCOMPARE(role(model, 0, "isMyTurn").toBool(), false);
        QCOMPARE(role(model, 1, "gameId").toString(), QStringLiteral("g3"));
        QCOMPARE(role(model, 2, "gameId").toString(), QStringLiteral("g1"));

        load(R"({"nowPlaying":[{"gameId":"g3","isMyTurn":true,"opponent":{"username":"C"}}]})");
        QCOMPARE(removed.count(), 2);
        QCOMPARE(model.count(), 1);
        QCOMPARE(role(model, 0, "gameId").toString(), QStringLiteral("g3"));
        QCOMPARE(reset.count(), 0);
    }

    void ongoingGamesPolling()
    {
        logIn();
        EventStream events(api);
        OngoingGamesModel model(api, &events);
        auto load = [&](const char *json) {
            server->route("GET", "/api/account/playing", 200, json);
            model.refresh();
            QTRY_VERIFY(!model.loading());
        };
        const char *waiting = R"({"nowPlaying":[{"gameId":"g1","isMyTurn":false},{"gameId":"g2","isMyTurn":true}]})";
        model.setPolling(true);
        QCOMPARE(model.pollIntervalMs(), 60 * 1000); // until the first load

        // A game waits for the opponent: every minute, slower while nothing changes.
        for (int i = 0; i < 5; ++i)
            load(waiting);
        QCOMPARE(model.pollIntervalMs(), 60 * 1000);
        load(waiting);
        QCOMPARE(model.pollIntervalMs(), 2 * 60 * 1000);
        load(waiting);
        load(waiting);
        QCOMPARE(model.pollIntervalMs(), 5 * 60 * 1000);
        // Looking at the games again brings back full speed.
        model.refreshIfStale();
        QCOMPARE(model.pollIntervalMs(), 60 * 1000);

        // Every game waits for me: only moves made elsewhere are left to notice.
        load(R"({"nowPlaying":[{"gameId":"g1","isMyTurn":true},{"gameId":"g2","isMyTurn":true}]})");
        QCOMPARE(model.pollIntervalMs(), 5 * 60 * 1000);
        // No games: new ones are announced on the event stream.
        load(R"({"nowPlaying":[]})");
        QCOMPARE(model.pollIntervalMs(), 0);

        load(waiting);
        QCOMPARE(model.pollIntervalMs(), 60 * 1000);
        model.setPolling(false);
        QCOMPARE(model.pollIntervalMs(), 0);
    }

    void ongoingGamesRefreshWhenStreamOpens()
    {
        logIn();
        server->streamRoute("GET", "/api/stream/event");
        server->route("GET", "/api/account/playing", 200, R"({"nowPlaying":[{"gameId":"g1","isMyTurn":false}]})");
        EventStream events(api);
        OngoingGamesModel model(api, &events);

        // Left to the event stream, which isn't up yet.
        model.refreshIfStale();
        events.start();
        QTRY_COMPARE(server->openStreams("/api/stream/event"), 1);
        // The stream opens with all ongoing games: one refresh for the lot.
        server->push("/api/stream/event", line(R"({"type":"gameStart","game":{"gameId":"g1"}})")
                     + line(R"({"type":"gameStart","game":{"gameId":"g2"}})"));
        QTRY_COMPARE(model.count(), 1);
        QCOMPARE(server->requestCount("GET", "/api/account/playing"), 1);

        // A list that recent isn't loaded again.
        model.refreshIfStale();
        QTest::qWait(200);
        QCOMPARE(server->requestCount("GET", "/api/account/playing"), 1);
        events.stop();
    }

    void friendsModel()
    {
        logIn();
        server->streamRoute("GET", "/api/rel/following");
        server->route("GET", "/api/users/status", 200, R"([{"id":"bob","online":true},{"id":"alice","online":false}])");
        FriendsModel model(api);
        model.refresh();
        QTRY_COMPARE(server->openStreams("/api/rel/following"), 1);
        server->push("/api/rel/following", line(R"({"id":"alice","username":"Alice"})")
                     + line(R"({"id":"bob","username":"Bob","title":"GM","perfs":{"blitz":{"rating":2500}}})"));
        server->closeStreams("/api/rel/following");
        QTRY_COMPARE(model.count(), 2);
        QCOMPARE(server->lastRequest("GET", "/api/users/status").query.queryItemValue("ids"), QStringLiteral("alice,bob"));
        // Online players first.
        QCOMPARE(role(model, 0, "username").toString(), QStringLiteral("Bob"));
        QCOMPARE(role(model, 0, "online").toBool(), true);
        QCOMPARE(role(model, 0, "title").toString(), QStringLiteral("GM"));
        QCOMPARE(role(model, 0, "blitzRating").toInt(), 2500);
        QCOMPARE(role(model, 1, "username").toString(), QStringLiteral("Alice"));
        QVERIFY(!model.loading());
    }

    // --- Seeks for a random opponent ---

    void lobbySeekFindsGame()
    {
        server->route("GET", "/api/account", 200, R"({"id":"me","username":"Me","perfs":{"rapid":{"rating":1700}}})");
        session->login(QStringLiteral("tok"));
        QTRY_VERIFY(session->loggedIn());
        server->streamRoute("POST", "/api/board/seek");
        EventStream events(api);
        LobbySeek seek(api, &events);
        QSignalSpy found(&seek, &LobbySeek::gameFound);

        seek.create({ { "minutes", 10 }, { "increment", 5 }, { "rated", true }, { "ratingDelta", 200 } });
        QCOMPARE(seek.state(), LobbySeek::Seeking);
        QCOMPARE(seek.description(), QStringLiteral("10+5 • Rapid • Rated"));
        QTRY_COMPARE(server->openStreams("/api/board/seek"), 1);
        const QUrlQuery form = server->lastRequest("POST", "/api/board/seek").form();
        QCOMPARE(form.queryItemValue("time"), QStringLiteral("10"));
        QCOMPARE(form.queryItemValue("increment"), QStringLiteral("5"));
        QCOMPARE(form.queryItemValue("rated"), QStringLiteral("true"));
        QCOMPARE(form.queryItemValue("color"), QStringLiteral("random"));
        QCOMPARE(form.queryItemValue("ratingRange"), QStringLiteral("1500-1900"));
        QVERIFY(!form.hasQueryItem("days"));

        // Only a fresh game from the lobby or a pool, with the seek's speed.
        emit events.gameStarted(map(R"({"gameId":"f1","source":"friend","speed":"rapid","rated":true,"hasMoved":false})"));
        emit events.gameStarted(map(R"({"gameId":"c1","source":"lobby","speed":"classical","rated":true,"hasMoved":false})"));
        emit events.gameStarted(map(R"({"gameId":"o1","source":"pool","speed":"rapid","rated":true,"hasMoved":true})"));
        QCOMPARE(seek.state(), LobbySeek::Seeking);
        emit events.gameStarted(map(R"({"gameId":"g1","source":"pool","speed":"rapid","rated":true,"hasMoved":false,
            "opponent":{"username":"Stranger"}})"));
        QCOMPARE(seek.state(), LobbySeek::Found);
        QCOMPARE(found.count(), 1);
        QCOMPARE(seek.gameId(), QStringLiteral("g1"));
        QCOMPARE(seek.opponent(), QStringLiteral("Stranger"));
        QTRY_COMPARE(server->openStreams("/api/board/seek"), 0); // the seek request is closed
    }

    void lobbySeekCanceledExpiredAndFailed()
    {
        logIn();
        server->streamRoute("POST", "/api/board/seek");
        EventStream events(api);
        LobbySeek seek(api, &events);

        // Closing the request cancels the seek on Lichess.
        seek.create({ { "minutes", 15 }, { "increment", 10 }, { "ratingDelta", 100 } });
        QTRY_COMPARE(server->openStreams("/api/board/seek"), 1);
        const QUrlQuery form = server->lastRequest("POST", "/api/board/seek").form();
        QCOMPARE(form.queryItemValue("rated"), QStringLiteral("false"));
        QVERIFY(!form.hasQueryItem("ratingRange")); // no rapid rating yet
        seek.cancel();
        QCOMPARE(seek.state(), LobbySeek::Canceled);
        QTRY_COMPARE(server->openStreams("/api/board/seek"), 0);

        // Lichess ends the request; a game starting right after means a match.
        seek.create({ { "minutes", 30 }, { "increment", 0 } });
        QTRY_COMPARE(server->openStreams("/api/board/seek"), 1);
        server->closeStreams("/api/board/seek");
        QTest::qWait(200);
        QCOMPARE(seek.state(), LobbySeek::Seeking);
        emit events.gameStarted(map(R"({"gameId":"g2","source":"lobby","speed":"classical","rated":false,"hasMoved":false})"));
        QCOMPARE(seek.state(), LobbySeek::Found);
        QCOMPARE(seek.opponent(), QStringLiteral("Anonymous"));

        // Without a game, the seek ended without an opponent.
        seek.create({ { "minutes", 30 }, { "increment", 0 } });
        QTRY_COMPARE(server->openStreams("/api/board/seek"), 1);
        server->closeStreams("/api/board/seek");
        QTRY_COMPARE(seek.state(), LobbySeek::Expired);

        // Refused by Lichess.
        server->route("POST", "/api/board/seek", 400, R"({"error":"You must also play some games as black"})");
        seek.create({ { "minutes", 10 }, { "color", "white" } });
        QTRY_COMPARE(seek.state(), LobbySeek::Failed);
        QCOMPARE(seek.errorString(), QStringLiteral("You must also play some games as black"));
        QCOMPARE(server->lastRequest("POST", "/api/board/seek").form().queryItemValue("color"), QStringLiteral("white"));

        // Blitz never reaches the server.
        const int requests = server->requests().size();
        seek.create({ { "minutes", 5 }, { "increment", 0 } });
        QCOMPARE(seek.state(), LobbySeek::Failed);
        QCOMPARE(server->requests().size(), requests);
    }

    void lobbySeekCorrespondence()
    {
        logIn();
        server->route("POST", "/api/board/seek", 200, R"({"id":"s1"})");
        EventStream events(api);
        LobbySeek seek(api, &events);

        seek.create({ { "correspondence", true }, { "days", 3 } });
        QVERIFY(seek.correspondence());
        QTRY_COMPARE(seek.state(), LobbySeek::Posted);
        const QUrlQuery form = server->lastRequest("POST", "/api/board/seek").form();
        QCOMPARE(form.queryItemValue("days"), QStringLiteral("3"));
        QVERIFY(!form.hasQueryItem("time"));
        QCOMPARE(seek.description(), QStringLiteral("3 days • Correspondence • Casual"));
        seek.cancel(); // only possible on lichess.org
        QCOMPARE(seek.state(), LobbySeek::Posted);

        server->route("POST", "/api/board/seek", 400, R"({"error":"Already playing too many games"})");
        seek.create({ { "correspondence", true }, { "days", 1 } });
        QTRY_COMPARE(seek.state(), LobbySeek::Failed);
        QCOMPARE(seek.errorString(), QStringLiteral("Already playing too many games"));
    }

    void lobbySeekCorrespondenceJoined()
    {
        logIn();
        EventStream events(api);
        OngoingGamesModel model(api, &events);
        QSignalSpy newGame(&model, &OngoingGamesModel::newLobbyGame);
        QSignalSpy myTurn(&model, &OngoingGamesModel::myTurn);
        server->route("GET", "/api/account/playing", 200, R"({"nowPlaying":[]})");
        model.refresh();
        QTRY_VERIFY(!model.loading());

        // Someone joined my correspondence seek, and I play white.
        server->route("GET", "/api/account/playing", 200, R"({"nowPlaying":[
            {"gameId":"c1","isMyTurn":true,"source":"lobby","speed":"correspondence","opponent":{"username":"Joiner"}},
            {"gameId":"f1","isMyTurn":true,"source":"friend","speed":"correspondence","opponent":{"username":"Pal"}}]})");
        model.refresh();
        QTRY_COMPARE(model.count(), 2);
        QCOMPARE(newGame.count(), 1);
        QCOMPARE(newGame.first().at(0).toString(), QStringLiteral("c1"));
        QCOMPARE(newGame.first().at(1).toString(), QStringLiteral("Joiner"));
        QCOMPARE(myTurn.count(), 1);
        QCOMPARE(myTurn.first().at(0).toString(), QStringLiteral("f1"));
    }

    // --- Outgoing challenges ---

    void outgoingChallengeAccepted()
    {
        logIn();
        server->streamRoute("POST", "/api/challenge/friend");
        EventStream events(api);
        OutgoingChallenges challenges(api, &events);
        QSignalSpy accepted(&challenges, &OutgoingChallenges::challengeAccepted);
        QSignalSpy finished(&challenges, &OutgoingChallenges::challengeFinished);

        OutgoingChallenge *challenge = challenges.create(QStringLiteral("friend"), {
            { "minutes", 10 }, { "increment", 5 }, { "rated", true }, { "color", "white" } });
        QCOMPARE(challenge->state(), OutgoingChallenge::Creating);
        QCOMPARE(challenges.pendingCount(), 1);
        QTRY_COMPARE(server->openStreams("/api/challenge/friend"), 1);

        const QUrlQuery form = server->lastRequest("POST", "/api/challenge/friend").form();
        QCOMPARE(form.queryItemValue("clock.limit"), QStringLiteral("600"));
        QCOMPARE(form.queryItemValue("clock.increment"), QStringLiteral("5"));
        QCOMPARE(form.queryItemValue("rated"), QStringLiteral("true"));
        QCOMPARE(form.queryItemValue("color"), QStringLiteral("white"));
        QCOMPARE(form.queryItemValue("keepAliveStream"), QStringLiteral("true"));
        QVERIFY(!form.hasQueryItem("days"));

        server->push("/api/challenge/friend",
                     line(R"({"id":"ch1","url":"https:\/\/lichess.org\/ch1","destUser":{"name":"Friend"}})"));
        QTRY_COMPARE(challenge->state(), OutgoingChallenge::Waiting);
        QCOMPARE(challenge->challengeId(), QStringLiteral("ch1"));
        QCOMPARE(challenge->opponent(), QStringLiteral("Friend"));
        QCOMPARE(challenges.find(QStringLiteral("ch1")), challenge);

        server->push("/api/challenge/friend", line(R"({"done":"accepted"})"));
        QTRY_COMPARE(challenge->state(), OutgoingChallenge::Accepted);
        QCOMPARE(accepted.count(), 1);
        QCOMPARE(accepted.first().at(0).toString(), QStringLiteral("ch1"));
        QCOMPARE(accepted.first().at(1).toString(), QStringLiteral("Friend"));
        QCOMPARE(finished.count(), 1);
        QCOMPARE(challenges.pendingCount(), 0);
        QCOMPARE(challenges.find(QStringLiteral("ch1")), static_cast<OutgoingChallenge *>(nullptr));
    }

    void outgoingChallengeCanceledAndDeclined()
    {
        logIn();
        server->streamRoute("POST", "/api/challenge/pal");
        server->route("POST", "/api/challenge/ch2/cancel", 200, R"({"ok":true})");
        EventStream events(api);
        OutgoingChallenges challenges(api, &events);
        QSignalSpy declined(&challenges, &OutgoingChallenges::challengeDeclined);

        // Correspondence, then canceled by the user.
        OutgoingChallenge *first = challenges.create(QStringLiteral("pal"), {
            { "correspondence", true }, { "days", 3 } });
        QTRY_COMPARE(server->openStreams("/api/challenge/pal"), 1);
        const QUrlQuery form = server->lastRequest("POST", "/api/challenge/pal").form();
        QCOMPARE(form.queryItemValue("days"), QStringLiteral("3"));
        QVERIFY(!form.hasQueryItem("clock.limit"));
        server->push("/api/challenge/pal", line(R"({"challenge":{"id":"ch2"}})"));
        QTRY_COMPARE(first->state(), OutgoingChallenge::Waiting);
        first->cancel();
        QCOMPARE(first->state(), OutgoingChallenge::Canceled);
        QTRY_COMPARE(server->requestCount("POST", "/api/challenge/ch2/cancel"), 1);

        // Declined: the reason arrives through the event stream.
        server->streamRoute("POST", "/api/challenge/buddy");
        OutgoingChallenge *second = challenges.create(QStringLiteral("buddy"), {});
        QTRY_COMPARE(server->openStreams("/api/challenge/buddy"), 1);
        server->push("/api/challenge/buddy", line(R"({"id":"ch3"})"));
        QTRY_COMPARE(second->state(), OutgoingChallenge::Waiting);
        server->push("/api/challenge/buddy", line(R"({"done":"declined"})"));
        emit events.challengeDeclined(map(R"({"id":"ch3","declineReason":"Not accepting challenges"})"));
        QTRY_COMPARE(second->state(), OutgoingChallenge::Declined);
        QCOMPARE(declined.count(), 1);
        QCOMPARE(declined.first().at(2).toString(), QStringLiteral("Not accepting challenges"));

        // Invalid names never reach the server.
        const int before = server->requests().size();
        OutgoingChallenge *invalid = challenges.create(QStringLiteral("no spaces!"), {});
        QCOMPARE(invalid->state(), OutgoingChallenge::Failed);
        QVERIFY(!invalid->errorString().isEmpty());
        QTest::qWait(100);
        QCOMPARE(server->requests().size(), before);
    }

    // --- Games ---

    void gameControllerPlays()
    {
        logIn();
        server->streamRoute("GET", "/api/board/game/stream/g1");
        server->route("GET", "/api/board/game/g1/chat", 200, "[]");
        server->route("POST", "/api/board/game/g1/move/e2e4", 200, R"({"ok":true})");
        server->route("POST", "/api/board/game/g1/move/g1f3", 400, R"({"error":"Not your turn, or game already over"})");
        server->route("POST", "/api/board/game/g1/draw/yes", 200, R"({"ok":true})");
        server->route("POST", "/api/board/game/g1/chat", 200, R"({"ok":true})");

        GameController game;
        game.setGameId(QStringLiteral("g1"));
        QVERIFY(game.loading());
        QTRY_COMPARE(server->openStreams("/api/board/game/stream/g1"), 1);

        const QString stream = QStringLiteral("/api/board/game/stream/g1");
        server->push(stream, line(R"({"type":"gameFull","id":"g1",
            "white":{"id":"me","name":"Me","rating":1500},"black":{"id":"friend","name":"Friend","rating":1600},
            "speed":"rapid","perf":{"name":"Rapid"},"rated":true,"variant":{"name":"Standard"},
            "clock":{"initial":600000,"increment":0},"initialFen":"startpos",
            "state":{"type":"gameState","moves":"","wtime":600000,"btime":600000,"winc":0,"binc":0,"status":"started"}})"));
        QTRY_COMPARE(game.myColor(), QStringLiteral("white"));
        QVERIFY(!game.loading());
        QVERIFY(game.isMyTurn());
        QVERIFY(game.canAbort());
        QVERIFY(game.hasClock());
        QCOMPARE(game.perfName(), QStringLiteral("Rapid"));
        QCOMPARE(game.black().value("name").toString(), QStringLiteral("Friend"));
        QCOMPARE(game.whiteTime(), 600000);
        QCOMPARE(game.runningClock(), QString()); // clocks start after both moved

        // Moves are shown at once and confirmed by the server.
        game.move("e1e8"); // illegal: ignored
        QCOMPARE(game.game()->ply(), 0);
        game.move("e2e4");
        QCOMPARE(game.game()->ply(), 1);
        QVERIFY(!game.isMyTurn());
        QTRY_COMPARE(server->requestCount("POST", "/api/board/game/g1/move/e2e4"), 1);

        server->push(stream, line(R"({"type":"gameState","moves":"e2e4 e7e5","wtime":590000,"btime":595000,
            "winc":0,"binc":0,"status":"started","bdraw":true})"));
        QTRY_COMPARE(game.game()->ply(), 2);
        QVERIFY(game.isMyTurn());
        QVERIFY(!game.canAbort());
        QCOMPARE(game.runningClock(), QStringLiteral("white"));
        QVERIFY(game.whiteTime() <= 590000 && game.whiteTime() > 580000);
        QCOMPARE(game.blackTime(), 595000);
        QVERIFY(game.opponentOffersDraw());
        QVERIFY(!game.iOfferDraw());

        game.answerDraw(true);
        QTRY_COMPARE(server->requestCount("POST", "/api/board/game/g1/draw/yes"), 1);

        // A rejected move is rolled back.
        QSignalSpy rejected(&game, &GameController::moveRejected);
        game.move("g1f3");
        QCOMPARE(game.game()->ply(), 3);
        QTRY_COMPARE(rejected.count(), 1);
        QCOMPARE(rejected.first().first().toString(), QStringLiteral("Not your turn, or game already over"));
        QCOMPARE(game.game()->ply(), 2);

        // Chat
        server->push(stream, line(R"({"type":"chatLine","room":"player","username":"Friend","text":"gl"})")
                     + line(R"({"type":"chatLine","room":"spectator","username":"Someone","text":"hi"})"));
        QTRY_COMPARE(game.chat()->count(), 1);
        QCOMPARE(role(*game.chat(), 0, "mine").toBool(), false);
        game.sendChat(QStringLiteral("  hf  "));
        QTRY_COMPARE(server->requestCount("POST", "/api/board/game/g1/chat"), 1);
        const QUrlQuery chat = server->lastRequest("POST", "/api/board/game/g1/chat").form();
        QCOMPARE(chat.queryItemValue("room"), QStringLiteral("player"));
        QCOMPARE(chat.queryItemValue("text"), QStringLiteral("hf"));

        // A takeback shortens the move list.
        server->push(stream, line(R"({"type":"gameState","moves":"e2e4","wtime":590000,"btime":595000,
            "winc":0,"binc":0,"status":"started"})"));
        QTRY_COMPARE(game.game()->ply(), 1);

        // The end
        server->push(stream, line(R"({"type":"gameState","moves":"e2e4","wtime":590000,"btime":595000,
            "winc":0,"binc":0,"status":"resign","winner":"white"})"));
        QTRY_VERIFY(game.gameOver());
        QVERIFY(!game.isMyTurn());
        QVERIFY(game.resultText().contains(QStringLiteral("Friend resigned")));
        QVERIFY(game.resultText().contains(QStringLiteral("White is victorious")));
    }

    void chatTimestamps()
    {
        logIn();
        server->streamRoute("GET", "/api/board/game/stream/t1");
        server->route("GET", "/api/board/game/t1/chat", 200, R"([{"text":"earlier","user":"Pal"}])");
        GameController game;
        game.setGameId(QStringLiteral("t1"));
        QTRY_COMPARE(server->openStreams("/api/board/game/stream/t1"), 1);
        server->push("/api/board/game/stream/t1", line(R"({"type":"gameFull","id":"t1",
            "white":{"id":"me","name":"Me"},"black":{"id":"pal","name":"Pal"},"initialFen":"startpos",
            "state":{"type":"gameState","moves":"","wtime":0,"btime":0,"winc":0,"binc":0,"status":"started"}})"));
        QTRY_COMPARE(game.chat()->count(), 1); // the history

        const QDateTime before = QDateTime::currentDateTime();
        server->push("/api/board/game/stream/t1", line(R"({"type":"chatLine","room":"player","username":"Pal","text":"now"})"));
        QTRY_COMPARE(game.chat()->count(), 2);

        // Lichess sends no times: history lines have none, live lines the arrival time.
        QVERIFY(!role(*game.chat(), 0, "time").isValid());
        const QDateTime arrived = role(*game.chat(), 1, "time").toDateTime();
        QVERIFY(arrived.isValid());
        QVERIFY(arrived >= before.addSecs(-1) && arrived <= QDateTime::currentDateTime());
        QCOMPARE(role(*game.chat(), 1, "text").toString(), QStringLiteral("now"));
    }

    void gameControllerCorrespondence()
    {
        logIn();
        server->streamRoute("GET", "/api/board/game/stream/c1");
        server->route("GET", "/api/board/game/c1/chat", 200, "[]");
        GameController game;
        game.setGameId(QStringLiteral("c1"));
        QTRY_COMPARE(server->openStreams("/api/board/game/stream/c1"), 1);
        // No clock; wtime/btime are the time left for the current move.
        server->push("/api/board/game/stream/c1", line(R"({"type":"gameFull","id":"c1",
            "white":{"id":"me","name":"Me"},"black":{"id":"pal","name":"Pal"},
            "speed":"correspondence","perf":{"name":"Correspondence"},"daysPerTurn":3,"initialFen":"startpos",
            "state":{"type":"gameState","moves":"e2e4 e7e5","wtime":259200000,"btime":200000000,
                     "winc":0,"binc":0,"status":"started"}})"));
        QTRY_COMPARE(game.myColor(), QStringLiteral("white"));
        QVERIFY(!game.hasClock());
        QVERIFY(game.hasTurnTimer());
        QCOMPARE(game.daysPerTurn(), 3);
        QCOMPARE(game.runningClock(), QStringLiteral("white"));
        QVERIFY(game.whiteTime() <= 259200000 && game.whiteTime() > 259100000);
        QCOMPARE(game.blackTime(), 200000000);
    }

    void gameControllerSpectatorAndErrors()
    {
        logIn();
        server->streamRoute("GET", "/api/board/game/stream/g2");
        GameController game;
        game.setGameId(QStringLiteral("g2"));
        QTRY_COMPARE(server->openStreams("/api/board/game/stream/g2"), 1);
        server->push("/api/board/game/stream/g2", line(R"({"type":"gameFull","id":"g2",
            "white":{"id":"a","name":"A"},"black":{"aiLevel":3},"initialFen":"startpos",
            "state":{"type":"gameState","moves":"e2e4","wtime":0,"btime":0,"winc":0,"binc":0,"status":"started"}})"));
        QTRY_COMPARE(game.game()->ply(), 1);
        QCOMPARE(game.myColor(), QString());
        QVERIFY(!game.isMyTurn());
        QVERIFY(!game.hasClock());
        QVERIFY(!game.hasTurnTimer()); // no days per move either
        QCOMPARE(game.runningClock(), QString());
        QCOMPARE(game.black().value("name").toString(), QStringLiteral("Stockfish level 3"));

        // A game the Board API refuses: shown as an error, no reconnect loop.
        GameController refused;
        refused.setGameId(QStringLiteral("nope"));
        QTRY_VERIFY(!refused.errorString().isEmpty());
        QVERIFY(!refused.loading());
        QTest::qWait(1500);
        QCOMPARE(server->requestCount("GET", "/api/board/game/stream/nope"), 1);
    }

    // --- Puzzles ---

    void puzzleBatchAndResult()
    {
        logIn();
        const QByteArray batch = "{\"puzzles\":[" + QByteArray(DailyPuzzle) + "]}";
        server->route("GET", "/api/puzzle/batch/mix", 200, batch);
        server->route("POST", "/api/puzzle/batch/mix", 200,
                      R"({"rounds":[{"id":"1Sqyb","win":true,"ratingDiff":12}],"glicko":{"rating":1612.4}})");

        PuzzleController controller;
        controller.loadNext();
        QCOMPARE(controller.state(), PuzzleController::Loading);
        QTRY_COMPARE(controller.state(), PuzzleController::Playing);
        QCOMPARE(server->lastRequest("GET", "/api/puzzle/batch/mix").query.queryItemValue("nb"), QStringLiteral("15"));
        solve(controller);
        QVERIFY(controller.solved());

        QTRY_VERIFY(controller.resultSubmitted());
        QCOMPARE(controller.ratingDiff(), 12);
        QCOMPARE(controller.userRating(), 1612);
        const FakeLichess::Request submit = server->lastRequest("POST", "/api/puzzle/batch/mix");
        QCOMPARE(submit.query.queryItemValue("nb"), QStringLiteral("0"));
        const QJsonObject solution = QJsonDocument::fromJson(submit.body).object()
                .value("solutions").toArray().first().toObject();
        QCOMPARE(solution.value("id").toString(), QStringLiteral("1Sqyb"));
        QCOMPARE(solution.value("win").toBool(), true);
        QCOMPARE(solution.value("rated").toBool(), true);

        // With a hint the attempt is casual; viewing the solution is a loss.
        controller.loadNext();
        QTRY_COMPARE(controller.state(), PuzzleController::Playing);
        QTRY_COMPARE(controller.game()->ply(), 73);
        controller.showHint();
        controller.viewSolution();
        QTRY_COMPARE_WITH_TIMEOUT(controller.state(), PuzzleController::Finished, 10000);
        QTRY_COMPARE(server->requestCount("POST", "/api/puzzle/batch/mix"), 2);
        const QJsonObject second = QJsonDocument::fromJson(server->lastRequest("POST", "/api/puzzle/batch/mix").body)
                .object().value("solutions").toArray().first().toObject();
        QCOMPARE(second.value("win").toBool(), false);
        QCOMPARE(second.value("rated").toBool(), false);
    }

    void offlineIsNoticed()
    {
        // Nothing listens there, so the request never reaches Lichess.
        api->setServerUrl(QStringLiteral("http://127.0.0.1:1"));
        QString error;
        api->get(QStringLiteral("/api/account"), QUrlQuery(), this,
                 [&error](const ApiResult &result) { error = result.errorString; });
        QTRY_VERIFY(api->offline());
        QCOMPARE(error, QStringLiteral("No internet connection"));

        // An answer means the connection is back.
        server->route("GET", "/api/account", 200, AccountJson);
        api->setServerUrl(server->url());
        api->get(QStringLiteral("/api/account"), QUrlQuery(), this, [](const ApiResult &) {});
        QTRY_VERIFY(!api->offline());
    }

    void offlinePuzzles()
    {
        logIn();
        server->route("GET", "/api/puzzle/batch/mix", 200, puzzleBatch({ "p1", "p2", "p3" }));
        settings->setOfflinePuzzles(3);
        QTRY_COMPARE(puzzles->count(), 3);
        QCOMPARE(server->lastRequest("GET", "/api/puzzle/batch/mix").query.queryItemValue("nb"),
                 QStringLiteral("3"));

        // Puzzles are played out of the pool, which fills up again.
        server->route("GET", "/api/puzzle/batch/mix", 200, puzzleBatch({ "p4" }));
        PuzzleController controller;
        controller.loadNext();
        QTRY_COMPARE(controller.state(), PuzzleController::Playing);
        QCOMPARE(controller.puzzleId(), QStringLiteral("p1"));
        QTRY_COMPARE(puzzles->count(), 3);

        // Solved without a connection: the result waits for one.
        api->setServerUrl(QStringLiteral("http://127.0.0.1:1"));
        solve(controller);
        QTRY_VERIFY(api->offline());
        QTRY_COMPARE(puzzles->pendingResults(), 1);
        QVERIFY(!controller.resultSubmitted());

        // The next puzzle comes from the pool while offline.
        controller.loadNext();
        QTRY_COMPARE(controller.state(), PuzzleController::Playing);
        QCOMPARE(controller.puzzleId(), QStringLiteral("p2"));

        // Back online, the stored results are sent.
        server->route("POST", "/api/puzzle/batch/mix", 200, R"({"rounds":[]})");
        api->setServerUrl(server->url());
        api->get(QStringLiteral("/api/account"), QUrlQuery(), this, [](const ApiResult &) {});
        QTRY_COMPARE(puzzles->pendingResults(), 0);
        const QJsonObject sent = QJsonDocument::fromJson(server->lastRequest("POST", "/api/puzzle/batch/mix").body)
                .object().value("solutions").toArray().first().toObject();
        QCOMPARE(sent.value("id").toString(), QStringLiteral("p1"));
        QCOMPARE(sent.value("win").toBool(), true);

        // Another difficulty needs other puzzles.
        settings->setPuzzleDifficulty(QStringLiteral("harder"));
        QCOMPARE(puzzles->count(), 0);
        QTRY_COMPARE(server->lastRequest("GET", "/api/puzzle/batch/mix").query.queryItemValue("difficulty"),
                     QStringLiteral("harder"));
        settings->setPuzzleDifficulty(QString());
    }

    void offlinePuzzlesWithoutStock()
    {
        logIn();
        // Nothing stored and no connection: the puzzle page says so.
        api->setServerUrl(QStringLiteral("http://127.0.0.1:1"));
        PuzzleController controller;
        controller.loadNext();
        QTRY_COMPARE(controller.state(), PuzzleController::Error);
        QVERIFY(api->offline());
        QVERIFY(controller.errorString().contains(QStringLiteral("offline")));
    }

    void puzzleDailyAndErrors()
    {
        // Not logged in: results are not submitted.
        server->route("GET", "/api/puzzle/daily", 200, DailyPuzzle);
        PuzzleController controller;
        controller.loadDaily();
        QTRY_COMPARE(controller.state(), PuzzleController::Playing);
        QVERIFY(controller.isDaily());
        QCOMPARE(controller.puzzleUrl(), QStringLiteral("https://lichess.org/training/1Sqyb"));
        QCOMPARE(controller.gameUrl(), QStringLiteral("https://lichess.org/HxbFI25U/black#73"));
        solve(controller);
        QTest::qWait(200);
        QCOMPARE(server->requestCount("POST", "/api/puzzle/batch/mix"), 0);

        server->route("GET", "/api/puzzle/batch/fork", 200, R"({"puzzles":[]})");
        controller.setAngle(QStringLiteral("fork"));
        controller.loadNext();
        QTRY_COMPARE(controller.state(), PuzzleController::Error);
        QVERIFY(!controller.errorString().isEmpty());

        const int before = server->requests().size();
        controller.loadPuzzle(QStringLiteral("../../api/account"));
        QCOMPARE(controller.state(), PuzzleController::Error);
        QCOMPARE(server->requests().size(), before);

        server->route("GET", "/api/puzzle/1Sqyb", 200, DailyPuzzle);
        controller.loadPuzzle(QStringLiteral("1Sqyb"));
        QTRY_COMPARE(controller.state(), PuzzleController::Playing);
        QVERIFY(!controller.isDaily());
    }

    // --- GamesHistoryModel ---

    void gamesHistoryLoadsGames()
    {
        logIn();
        server->streamRoute("GET", "/api/games/user/Me");
        GamesHistoryModel model;
        QTRY_COMPARE(server->openStreams("/api/games/user/Me"), 1);
        QVERIFY(model.loading());

        const FakeLichess::Request request = server->lastRequest("GET", "/api/games/user/Me");
        QCOMPARE(request.headers.value("accept"), QByteArray("application/x-ndjson"));
        QCOMPARE(request.query.queryItemValue("max"), QStringLiteral("20"));
        QCOMPARE(request.query.queryItemValue("finished"), QStringLiteral("true"));
        QCOMPARE(request.query.queryItemValue("lastFen"), QStringLiteral("true"));
        QCOMPARE(request.query.queryItemValue("moves"), QStringLiteral("false"));
        QVERIFY(!request.query.hasQueryItem("until"));

        server->push("/api/games/user/Me", line(HistoryGame) + line(HistoryGameOlder));
        server->closeStreams("/api/games/user/Me");
        QTRY_COMPARE(model.count(), 2);
        QVERIFY(!model.loading());
        // A page shorter than the one asked for is the end of the history.
        QVERIFY(!model.hasMore());

        QCOMPARE(role(model, 0, "gameId").toString(), QStringLiteral("hist0001"));
        QCOMPARE(role(model, 0, "color").toString(), QStringLiteral("black"));
        QCOMPARE(role(model, 0, "result").toString(), QStringLiteral("loss"));
        QCOMPARE(role(model, 0, "opponentName").toString(), QStringLiteral("Rival"));
        QCOMPARE(role(model, 0, "opponentTitle").toString(), QStringLiteral("FM"));
        QCOMPARE(role(model, 0, "opponentRating").toInt(), 2100);
        QCOMPARE(role(model, 0, "ratingDiff").toInt(), -8);
        QCOMPARE(role(model, 0, "perf").toString(), QStringLiteral("blitz"));
        QVERIFY(role(model, 0, "rated").toBool());
        QCOMPARE(role(model, 0, "opening").toString(), QStringLiteral("Italian Game"));
        QVERIFY(role(model, 0, "fen").toString().startsWith(QStringLiteral("rnbqkbnr/")));
        QVERIFY(role(model, 0, "resultText").toString().contains(QStringLiteral("Checkmate")));

        // Playing white against a computer, which Lichess leaves nameless.
        QCOMPARE(role(model, 1, "color").toString(), QStringLiteral("white"));
        QCOMPARE(role(model, 1, "result").toString(), QStringLiteral("win"));
        QCOMPARE(role(model, 1, "opponentName").toString(), QStringLiteral("Stockfish level 3"));
        QVERIFY(!role(model, 1, "rated").toBool());
    }

    void gamesHistoryPagesOnDemand()
    {
        logIn();
        server->streamRoute("GET", "/api/games/user/Me");
        GamesHistoryModel model;
        QTRY_COMPARE(server->openStreams("/api/games/user/Me"), 1);

        // A full page means there may be more.
        QByteArray page;
        for (int i = 0; i < 20; ++i) {
            QJsonObject game = QJsonDocument::fromJson(HistoryGame).object();
            game.insert(QStringLiteral("id"), QStringLiteral("g%1").arg(i));
            // Lichess sends them newest first.
            game.insert(QStringLiteral("createdAt"), 1700000019000LL - i * 1000);
            page += QJsonDocument(game).toJson(QJsonDocument::Compact) + '\n';
        }
        server->push("/api/games/user/Me", page);
        server->closeStreams("/api/games/user/Me");
        QTRY_COMPARE(model.count(), 20);
        QVERIFY(model.hasMore());

        model.loadMore();
        QTRY_COMPARE(server->requestCount("GET", "/api/games/user/Me"), 2);
        // The oldest game held was made at 1700000000000; the next page has
        // to stop just short of it.
        QCOMPARE(server->lastRequest("GET", "/api/games/user/Me").query.queryItemValue("until"),
                 QStringLiteral("1699999999999"));
        QCOMPARE(role(model, 19, "createdAt").toLongLong(), 1700000000000LL);

        // A game already held is not listed twice.
        server->push("/api/games/user/Me", line(HistoryGame) + page.left(page.indexOf('\n') + 1));
        server->closeStreams("/api/games/user/Me");
        QTRY_COMPARE(model.count(), 21);
        QCOMPARE(role(model, 20, "gameId").toString(), QStringLiteral("hist0001"));
    }

    void gamesHistoryFilters()
    {
        logIn();
        server->streamRoute("GET", "/api/games/user/Me");
        GamesHistoryModel model;
        QTRY_COMPARE(server->requestCount("GET", "/api/games/user/Me"), 1);
        QVERIFY(!model.filtered());

        model.setPerfType(QStringLiteral("blitz,rapid"));
        model.setColor(QStringLiteral("white"));
        model.setRated(GamesHistoryModel::CasualOnly);
        model.setAnalysedOnly(true);
        model.setOpponent(QStringLiteral("Rival"));
        QVERIFY(model.filtered());
        // The five changes arrive together, so they cost one request.
        QTRY_COMPARE(server->requestCount("GET", "/api/games/user/Me"), 2);

        const QUrlQuery query = server->lastRequest("GET", "/api/games/user/Me").query;
        QCOMPARE(query.queryItemValue("perfType"), QStringLiteral("blitz,rapid"));
        QCOMPARE(query.queryItemValue("color"), QStringLiteral("white"));
        QCOMPARE(query.queryItemValue("rated"), QStringLiteral("false"));
        QCOMPARE(query.queryItemValue("analysed"), QStringLiteral("true"));
        QCOMPARE(query.queryItemValue("vs"), QStringLiteral("Rival"));

        model.clearFilters();
        QVERIFY(!model.filtered());
        QTRY_COMPARE(server->requestCount("GET", "/api/games/user/Me"), 3);
        QVERIFY(!server->lastRequest("GET", "/api/games/user/Me").query.hasQueryItem("perfType"));
    }

    void gamesHistoryFollowsTheAccount()
    {
        server->streamRoute("GET", "/api/games/user/Me");
        GamesHistoryModel model;
        // Logged out there is nobody whose games could be shown.
        QTest::qWait(50);
        QCOMPARE(server->requestCount("GET", "/api/games/user/Me"), 0);
        QVERIFY(!model.loading());

        logIn();
        QTRY_COMPARE(server->requestCount("GET", "/api/games/user/Me"), 1);

        // Another player's games are asked for under their own name.
        server->streamRoute("GET", "/api/games/user/Rival");
        model.setUsername(QStringLiteral("Rival"));
        QTRY_COMPARE(server->requestCount("GET", "/api/games/user/Rival"), 1);
    }

    void gamesHistoryEmptiesOnLogout()
    {
        logIn();
        server->streamRoute("GET", "/api/games/user/Me");
        GamesHistoryModel model;
        QTRY_COMPARE(server->openStreams("/api/games/user/Me"), 1);
        server->push("/api/games/user/Me", line(HistoryGame));
        server->closeStreams("/api/games/user/Me");
        QTRY_COMPARE(model.count(), 1);

        server->route("DELETE", "/api/token", 200, "{}");
        session->logout();
        QTRY_COMPARE(model.count(), 0);
    }

    void gamesHistoryReportsErrors()
    {
        logIn();
        GamesHistoryModel model;
        model.setUsername(QStringLiteral("Nobody")); // no route: 404
        QTRY_VERIFY(!model.errorString().isEmpty());
        QCOMPARE(model.count(), 0);
        QVERIFY(!model.hasMore());
        QVERIFY(!model.loading());
    }

    // --- GameAnalysis ---

    void gameAnalysisReadsServerAnalysis()
    {
        logIn();
        server->route("GET", "/game/export/anal0001", 200, AnalysedGame);
        GameAnalysis analysis;
        analysis.setGameId(QStringLiteral("anal0001"));
        QTRY_COMPARE(analysis.game()->ply(), 4);
        QVERIFY(!analysis.loading());
        QVERIFY(analysis.errorString().isEmpty());
        QVERIFY(analysis.hasServerAnalysis());

        const QUrlQuery query = server->lastRequest("GET", "/game/export/anal0001").query;
        QCOMPARE(query.queryItemValue("evals"), QStringLiteral("true"));
        QCOMPARE(query.queryItemValue("accuracy"), QStringLiteral("true"));
        QCOMPARE(query.queryItemValue("clocks"), QStringLiteral("true"));

        QCOMPARE(analysis.game()->sanMoves(), QStringList({ "e4", "e5", "Qh5", "Nc6" }));
        QCOMPARE(analysis.white().value("name").toString(), QStringLiteral("Me"));
        QCOMPARE(analysis.white().value("accuracy").toInt(), 61);
        QCOMPARE(analysis.white().value("acpl").toInt(), 120);
        QCOMPARE(analysis.white().value("blunder").toInt(), 1);
        QCOMPARE(analysis.black().value("title").toString(), QStringLiteral("FM"));
        QCOMPARE(analysis.black().value("ratingDiff").toInt(), 7);
        QCOMPARE(analysis.openingName(), QStringLiteral("Open Game"));
        QCOMPARE(analysis.openingEco(), QStringLiteral("C20"));
        QVERIFY(analysis.resultText().contains(QStringLiteral("Black is victorious")));
        QCOMPARE(analysis.judgments(), QStringList({ "", "", "Blunder", "" }));

        // Entry i belongs to the position after move i + 1, so the blunder
        // is the third move.
        analysis.game()->goToPly(3);
        QVERIFY(analysis.hasEval());
        QCOMPARE(analysis.evalCp(), -90);
        QCOMPARE(analysis.evalSource(), QStringLiteral("server"));
        QCOMPARE(analysis.judgment(), QStringLiteral("Blunder"));
        QCOMPARE(analysis.bestMove(), QStringLiteral("g1f3"));
        QCOMPARE(analysis.bestVariation(), QStringLiteral("Nf3 Nc6"));
        QCOMPARE(analysis.clockMs(), 287000);
        QVERIFY(analysis.winPercent() < 50); // black is better

        // bestMove is the alternative to the move that led here, so it
        // belongs to the position before this one; nextBestMove is the one
        // to play from the position on the board, and that is what an arrow
        // can be drawn for. The blunder is the third move, so it is the
        // second position that has something better to play.
        analysis.game()->goToPly(2);
        QCOMPARE(analysis.nextBestMove(), QStringLiteral("g1f3"));
        QCOMPARE(analysis.nextBestVariation(), QStringLiteral("Nf3 Nc6"));
        QCOMPARE(analysis.bestMove(), QString()); // move 2 was not faulted

        analysis.game()->goToPly(3);
        QCOMPARE(analysis.bestMove(), QStringLiteral("g1f3"));
        QCOMPARE(analysis.nextBestMove(), QString()); // nothing said about move 4

        analysis.game()->goToPly(1);
        QCOMPARE(analysis.evalCp(), 20);
        QCOMPARE(analysis.judgment(), QString());
        QVERIFY(analysis.winPercent() > 50);

        const QVariantList points = analysis.evalPoints();
        QCOMPARE(points.size(), 4);
        QCOMPARE(points.at(2).toMap().value("ply").toInt(), 3);
        QCOMPARE(points.at(2).toMap().value("judgment").toString(), QStringLiteral("Blunder"));

        // The plies Lichess analysed need nothing from the cloud, and
        // neither does the blank board shown while a game is loading.
        QTest::qWait(600);
        QCOMPARE(server->requestCount("GET", "/api/cloud-eval"), 0);

        // The starting position is not one of the analysed plies, so that
        // one is looked up.
        server->route("GET", "/api/cloud-eval", 404, R"({"error":"nothing here"})");
        analysis.game()->viewFirst();
        QVERIFY(!analysis.hasEval());
        QCOMPARE(analysis.clockMs(), -1);
        QTRY_COMPARE(server->requestCount("GET", "/api/cloud-eval"), 1);
    }

    void gameAnalysisIgnoresSideLines()
    {
        logIn();
        server->route("GET", "/game/export/anal0001", 200, AnalysedGame);
        server->route("GET", "/api/cloud-eval", 404, R"({"error":"nothing here"})");
        GameAnalysis analysis;
        analysis.setGameId(QStringLiteral("anal0001"));
        QTRY_COMPARE(analysis.game()->ply(), 4);

        analysis.game()->setAllowVariations(true);
        analysis.game()->goToPly(2);
        QCOMPARE(analysis.evalCp(), 15);
        QCOMPARE(analysis.clockMs(), 298500);

        // A move of the user's own is not part of what Lichess analysed, so
        // neither its evaluations nor its clocks apply from there on.
        // The branch point is still the game's own position, so what should
        // have been played there still holds.
        QCOMPARE(analysis.nextBestMove(), QStringLiteral("g1f3"));

        QVERIFY(analysis.game()->playUci("b1c3"));
        QVERIFY(analysis.game()->inVariation());
        QCOMPARE(analysis.nextBestMove(), QString()); // a side line is not the game
        QVERIFY(!analysis.hasEval());
        QCOMPARE(analysis.judgment(), QString());
        QCOMPARE(analysis.clockMs(), -1);
        QCOMPARE(analysis.whiteClockMs(), -1);
        // The cloud is asked about the side line instead.
        QTRY_VERIFY(server->requestCount("GET", "/api/cloud-eval") > 0);

        // Back on the game everything is there again.
        analysis.game()->exitVariation();
        QCOMPARE(analysis.game()->viewPly(), 2);
        QCOMPARE(analysis.evalCp(), 15);
        QCOMPARE(analysis.clockMs(), 298500);
    }

    void gameAnalysisFallsBackToCloudEval()
    {
        logIn();
        server->route("GET", "/game/export/anal0002", 200, UnanalysedGame);
        server->route("GET", "/api/cloud-eval", 200,
                      R"({"fen":"x","knodes":1,"depth":30,"pvs":[{"cp":34,"moves":"g1f3"}]})");
        GameAnalysis analysis;
        analysis.setGameId(QStringLiteral("anal0002"));
        QTRY_COMPARE(analysis.game()->ply(), 2);
        QVERIFY(!analysis.hasServerAnalysis());

        QTRY_VERIFY(analysis.hasEval());
        QCOMPARE(analysis.evalCp(), 34);
        QCOMPARE(analysis.evalSource(), QStringLiteral("cloud"));
        QCOMPARE(server->lastRequest("GET", "/api/cloud-eval").query.queryItemValue("fen"),
                 analysis.game()->fen());

        // Positions Lichess has never seen answer with 404, which is normal.
        server->route("GET", "/api/cloud-eval", 404, R"({"error":"No cloud evaluation available"})");
        analysis.game()->viewFirst();
        QTRY_COMPARE(server->requestCount("GET", "/api/cloud-eval"), 2);
        QVERIFY(!analysis.hasEval());
        QVERIFY(analysis.errorString().isEmpty());

        // A position already asked about is not asked about again.
        analysis.game()->viewLatest();
        QTRY_VERIFY(analysis.hasEval());
        QTest::qWait(600);
        QCOMPARE(server->requestCount("GET", "/api/cloud-eval"), 2);
    }

    void gameAnalysisOfASetUpPosition()
    {
        logIn();
        server->route("GET", "/api/cloud-eval", 200,
                      R"({"fen":"x","knodes":1,"depth":30,"pvs":[{"cp":-120,"moves":"e8d7"}]})");
        const QString fen = QStringLiteral("4k3/8/8/8/8/8/4P3/4K3 b - - 0 1");
        GameAnalysis analysis;
        analysis.setStartFen(fen);
        QCOMPARE(analysis.game()->fen(), fen);
        QCOMPARE(analysis.game()->ply(), 0);
        QVERIFY(!analysis.loading());

        // The cloud is asked about the position; there is no game to fetch.
        QTRY_VERIFY(analysis.hasEval());
        QCOMPARE(analysis.evalCp(), -120);
        QCOMPARE(server->lastRequest("GET", "/api/cloud-eval").query.queryItemValue("fen"), fen);
        for (const FakeLichess::Request &request : server->requests())
            QVERIFY(!request.path.startsWith(QStringLiteral("/game/export")));

        // Moves can be played from it, and reload() has nothing to fetch.
        QVERIFY(analysis.game()->playUci(QStringLiteral("e8d7")));
        QCOMPARE(analysis.game()->ply(), 1);
        analysis.reload();
        QVERIFY(!analysis.loading());
    }

    void gameAnalysisRejectsUnplayableVariants()
    {
        logIn();
        server->route("GET", "/game/export/anal0003", 200, CrazyhouseGame);
        GameAnalysis analysis;
        analysis.setGameId(QStringLiteral("anal0003"));
        QTRY_VERIFY(!analysis.errorString().isEmpty());
        QCOMPARE(analysis.game()->ply(), 0);
        QCOMPARE(analysis.variantName(), QStringLiteral("crazyhouse"));

        // A game that is not there says so rather than staying blank.
        analysis.setGameId(QStringLiteral("nosuchgame"));
        QTRY_VERIFY(!analysis.errorString().isEmpty());
        QVERIFY(!analysis.loading());
    }
};

int runLichessTests(int argc, char *argv[])
{
    TestLichess test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_lichess.moc"
