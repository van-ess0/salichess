.pragma library

// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

// Formats a clock value in milliseconds: "1:02:03", "4:05" or "9.4".
function formatClock(ms) {
    if (ms < 10000)
        return (Math.floor(Math.max(ms, 0) / 100) / 10).toFixed(1)
    return formatClockSeconds(ms)
}

// Like formatClock(), but in whole seconds throughout: "1:02:03", "4:05" or
// "0:09".
function formatClockSeconds(ms) {
    var total = Math.floor(Math.max(ms, 0) / 1000)
    var h = Math.floor(total / 3600)
    var m = Math.floor((total % 3600) / 60)
    var s = total % 60
    var ss = (s < 10 ? "0" : "") + s
    if (h > 0)
        return h + ":" + (m < 10 ? "0" : "") + m + ":" + ss
    return m + ":" + ss
}

// "GM Magnus" / "Magnus"
function playerName(name, title) {
    return title ? title + " " + name : name
}

function escapeHtml(text) {
    return String(text).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;")
}

// Parses the piece placement of a FEN into 64 entries ("wK", "bp" or ""),
// a1 first.
function fenToPieces(fen) {
    var cells = []
    for (var i = 0; i < 64; ++i)
        cells.push("")
    if (!fen)
        return cells
    var rows = fen.split(" ")[0].split("/")
    for (var r = 0; r < rows.length && r < 8; ++r) {
        var rank = 7 - r
        var file = 0
        var row = rows[r]
        for (var c = 0; c < row.length && file < 8; ++c) {
            var ch = row.charAt(c)
            if (ch >= "1" && ch <= "8") {
                file += parseInt(ch, 10)
            } else {
                var white = ch === ch.toUpperCase()
                cells[rank * 8 + file] = (white ? "w" : "b") + ch.toUpperCase()
                ++file
            }
        }
    }
    return cells
}

function squareIndex(name) {
    if (!name || name.length < 2)
        return -1
    var file = name.charCodeAt(0) - 97
    var rank = name.charCodeAt(1) - 49
    if (file < 0 || file > 7 || rank < 0 || rank > 7)
        return -1
    return rank * 8 + file
}

// Colours of the board themes; "ambience" is derived from the Silica theme.
function boardColors(theme, Theme) {
    switch (theme) {
    case "blue":
        return { light: "#dee3e6", dark: "#8ca2ad" }
    case "green":
        return { light: "#ffffdd", dark: "#86a666" }
    case "ambience":
        return {
            light: Qt.tint(Theme.overlayBackgroundColor, Theme.rgba(Theme.highlightColor, 0.45)),
            dark: Qt.tint(Theme.overlayBackgroundColor, Theme.rgba(Theme.highlightDimmerColor, 0.9))
        }
    default:
        return { light: "#f0d9b5", dark: "#b58863" }
    }
}

// Lichess marks a move it faulted with "?!", "?" or "??".
function judgmentGlyph(judgment) {
    switch (judgment) {
    case "Inaccuracy": return "?!"
    case "Mistake": return "?"
    case "Blunder": return "??"
    default: return ""
    }
}

// The colours lichess.org gives those marks, which stand out on any ambience.
function judgmentColor(judgment) {
    switch (judgment) {
    case "Inaccuracy": return "#56b4e9"
    case "Mistake": return "#e69f00"
    case "Blunder": return "#df5353"
    default: return ""
    }
}

// An evaluation in centipawns as Lichess writes it: "+1.25", "-0.40", "#3".
function formatEval(centipawns, mate) {
    if (mate)
        return (mate > 0 ? "#" : "-#") + Math.abs(mate)
    var pawns = centipawns / 100
    return (pawns > 0 ? "+" : pawns < 0 ? "\u2212" : "") + Math.abs(pawns).toFixed(2)
}

// The day a game was played; a game from today is given as a time instead.
function formatGameDate(ms) {
    if (!ms)
        return ""
    var date = new Date(ms)
    var today = new Date()
    var days = Math.floor((today.setHours(0, 0, 0, 0) - new Date(ms).setHours(0, 0, 0, 0)) / 86400000)
    if (days === 0)
        return Qt.formatTime(date, Qt.DefaultLocaleShortDate)
    return Qt.formatDate(date, days < 365 ? "d MMM" : "d MMM yyyy")
}
