.pragma library

// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

// The eight levels of the Lichess AI, which the engine on the phone follows,
// and roughly how strong each plays.
var levelRatings = [800, 1100, 1400, 1700, 2000, 2300, 2650, 2850]

function levelRating(level) {
    return levelRatings[Math.max(1, Math.min(8, level)) - 1]
}

// Time controls for the Lichess AI: seconds and increment.
var clocks = [
    { "seconds": 60, "increment": 0 },
    { "seconds": 180, "increment": 2 },
    { "seconds": 300, "increment": 3 },
    { "seconds": 600, "increment": 0 },
    { "seconds": 900, "increment": 10 },
    { "seconds": 1800, "increment": 0 }
]

function clockName(clock) {
    return (clock.seconds / 60) + "+" + clock.increment
}
