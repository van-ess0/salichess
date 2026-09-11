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
#include "lichess/gamecontroller.h"
#include "lichess/ongoinggamesmodel.h"
#include "lichess/outgoingchallenge.h"
#include "lichess/outgoingchallenges.h"
#include "lichess/puzzlecontroller.h"
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
        Services::init(api, session, nullptr);
    }

    void cleanup()
    {
        Services::init(nullptr, nullptr, nullptr);
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
        QVERIFY(!session->loggedIn());
        QVERIFY(store->token.isEmpty());
        QVERIFY(!api->hasToken());
        // The token is revoked on the server with its own credentials.
        QTRY_COMPARE(server->requestCount("DELETE", "/api/token"), 1);
        QCOMPARE(server->lastRequest("DELETE", "/api/token").headers.value("authorization"), QByteArray("Bearer tok"));
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
};

int runLichessTests(int argc, char *argv[])
{
    TestLichess test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_lichess.moc"
