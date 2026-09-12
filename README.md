# salichess

An unofficial [Lichess](https://lichess.org) client for [Sailfish OS](https://sailfishos.org),
written in C++/Qt 5 with a native Silica user interface.

> salichess is not affiliated with or endorsed by lichess.org.

## Features

- **Log in with Lichess:** OAuth with PKCE. You sign in on lichess.org in the browser, so the app never sees your password.
- **Play with friends**
  - Challenge a player you follow, or any username, with real-time (blitz and slower) or correspondence time controls, choice of colour, rated or casual.
  - Accept or decline incoming challenges. Sent challenges stay open while you use the rest of the app, and the game opens when your friend accepts.
  - Play your ongoing games: animated board with drag or tap-to-move, clocks (for correspondence, the time left for the current move), move list and history, draw and takeback offers, resign, abort, claim victory, and in-game chat.
- **Play with random opponents:** Lichess' quick-pairing time controls, a custom clock or a correspondence seek (the Board API allows rapid and slower), rated or casual, with a rating range and choice of colour. A seek stays open while you use the rest of the app, and the game opens when an opponent is found. Correspondence seeks can only be withdrawn on lichess.org.
- **Puzzles:** the daily puzzle, a healthy mix, or around 60 themes (forks, mates in N, endgames, …). Choose a difficulty, get a two-step hint, or view the solution. Results count towards your Lichess puzzle rating when you are logged in.
- **Sailfish integration:**
  - Notifications for new challenges and for "your turn" in correspondence games, while the app runs.
  - The cover shows the current game or whose move it is.
  - Optionally keeps the screen on during games.
  - Board themes, including one that follows the ambience.

## Installing

There is no store release yet. Build the RPM yourself (see below) and install it on the phone, for
example with `sfdk deploy --sdk`.

salichess is developed against the Sailfish OS 5.1 SDK target and tested on Sailfish OS 5.2. It
needs Sailjail, so older releases without it won't work. It requests the Sailjail permissions
`Internet` and `Secrets` (for the login token), and it talks to lichess.org only.

## Building

You need the [Sailfish SDK](https://docs.sailfishos.org/Tools/Sailfish_SDK/). Open
`harbour-salichess.pro` in the SDK's Qt Creator, or use `sfdk`:

```sh
mkdir ../build-salichess && cd ../build-salichess
sfdk config target=SailfishOS-5.1.0.11-aarch64   # or -armv7hl, or -i486 for the emulator
sfdk build ../salichess                          # produces RPMS/harbour-salichess-*.rpm
sfdk check                                       # Harbour validation
sfdk config device="<your device>"
sfdk deploy --sdk
```

The target ships Qt 5.6 and GCC 13: C++17 is fine, but Qt APIs newer than 5.6 and ES6 JavaScript
in QML are not available.

GitHub Actions (`.github/workflows/build.yml`) builds the aarch64, armv7hl and i486 RPMs with the
Sailfish OS Platform SDK in Docker for every push and pull request, and runs the [tests](#tests)
against Ubuntu's desktop Qt 5. The RPMs are attached to each run as an artifact.

### Releasing

The spec is the source of the version.

1. Set `Version`/`Release` in `rpm/harbour-salichess.spec`, and add an entry for that
   `version-release` at the top of `rpm/harbour-salichess.changes`.
2. Commit, then tag and push: `git tag v0.2-1 && git push origin v0.2-1`.

The workflow checks that the tag matches the spec and the changelog, builds the RPMs, runs the tests,
and publishes a GitHub release with the RPMs attached. The release notes come from the changelog entry. The app's About
page and user agent take their version from the spec too.

## Tests

Everything below the QML layer is covered by host-side tests. They need a desktop Qt 5 and no
Sailfish SDK:

```sh
mkdir ../build-tests && cd ../build-tests
qmake-qt5 ../salichess/tests && make
./tst_salichess                              # offline; a local fake Lichess server stands in
SALICHESS_NETWORK_TESTS=1 ./tst_salichess    # also runs a few read-only tests against lichess.org
```

The suite covers the chess rules and move history, puzzle solving, the HTTP client (request queue,
rate limiting, ndjson streams), login, challenges, the game controller and the models.

### Static analysis

[CodeQL](.github/workflows/codeql.yml) runs on every push and pull request, over the C++ sources and
the workflow files. It needs no Sailfish SDK: the C++ database is built from the host tests, which
means the four files that need SDK-only headers (`main.cpp`, `appactivation.cpp`,
`pieceimageprovider.cpp`, `secretstokenstore.cpp`) are not covered. Findings go to the repository's
Security tab, not into the build log, and never fail a build.

## How it is put together

The C++ core owns all logic and state. QML only presents it.

| Directory | Contents |
|---|---|
| `src/chess/` | `ChessPosition` wraps the vendored [chess-library](https://github.com/Disservin/chess-library) (the only file that includes it); `ChessGame` keeps move history and feeds the board model. |
| `src/core/` | `LichessApi` (serialized REST requests, ndjson streams, 429 handling), `Session` (login state), `TokenStore` (swappable token storage), settings, piece image provider, D-Bus activation. |
| `src/lichess/` | One class per feature: event stream, challenges, ongoing games, friends, outgoing challenges, `GameController` (Board API games), `PuzzleController`. |
| `qml/` | Silica pages, the board and other components, the cover. |
| `tests/` | Host tests and the fake Lichess server. |

There is no C/C++ Lichess client library to build on, so `LichessApi` is a small client for the
parts of the [Lichess API](https://lichess.org/api) the app uses. To add a feature (TV, analysis,
tournaments, …), add a controller or model in `src/lichess/`, register it in `src/main.cpp`, and
add a page in `qml/pages/`. `ChessGame` and `ChessBoard.qml` can be reused.

Limits of the Lichess Board API that the app inherits: games must be blitz or slower (no bullet),
and only standard chess is supported for now.

## Privacy

- The Lichess access token is kept encrypted by Sailfish OS Secrets. It sits in an
  owner-only collection that is unlocked together with the device, which is why the app asks for
  the Sailjail `Secrets` permission. Logging out in the app revokes the token on Lichess; the app
  stays logged in until Lichess confirms that. You can also revoke it in your
  [Lichess settings](https://lichess.org/account/security).
- No analytics or other third-party services. Requests identify themselves to Lichess with a
  `harbour-salichess/<version>` user agent.

## Contributing

Bug reports and pull requests are welcome at <https://github.com/van-ess0/salichess>.

- Keep the style of the surrounding code, and add or update tests in `tests/` for changes below the
  QML layer. Run the test suite and `sfdk check` before sending a pull request.
- New files need an SPDX header (see any source file). The project follows
  [REUSE](https://reuse.software); `uvx reuse lint` checks this.
- **Translations:** `translations/harbour-salichess-<lang>.ts` files are regenerated from the
  sources on every build. To add a language, add the file to `TRANSLATIONS` in
  `harbour-salichess.pro`, build once, and translate it with Qt Linguist.

## License

salichess is free software: you can redistribute it and/or modify it under the terms of the
[GNU General Public License](LICENSE), version 3 or (at your option) any later version.

It includes:

- [chess-library](https://github.com/Disservin/chess-library) by Disservin (MIT),
  in `3rdparty/chess-library/`
- the "cburnett" chess pieces by Colin M.L. Burnett (GPL-2.0-or-later), as used on Lichess
- an app icon that combines the cburnett knight with the base shape from Jolla's Sailfish OS app icon
  template (CC BY 3.0)

Exact per-file licensing is in the SPDX headers and in [`REUSE.toml`](REUSE.toml). The license texts
are in [`LICENSES/`](LICENSES).

Lichess is a free, open-source chess server run by a non-profit. If you enjoy it, consider
[supporting Lichess](https://lichess.org/patron).
