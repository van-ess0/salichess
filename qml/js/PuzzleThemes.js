// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

// Lichess puzzle themes ("angles"). Keys are the ones the API accepts; see
// https://github.com/lichess-org/lila/blob/master/translation/source/puzzleTheme.xml

function categories() {
    return [
        { title: qsTr("Phases"), themes: ["opening", "middlegame", "endgame", "rookEndgame", "bishopEndgame",
                                          "pawnEndgame", "knightEndgame", "queenEndgame", "queenRookEndgame"] },
        { title: qsTr("Motifs"), themes: ["advancedPawn", "attackingF2F7", "capturingDefender", "discoveredAttack",
                                          "doubleCheck", "exposedKing", "fork", "hangingPiece", "kingsideAttack",
                                          "pin", "queensideAttack", "sacrifice", "skewer", "trappedPiece"] },
        { title: qsTr("Advanced"), themes: ["attraction", "clearance", "defensiveMove", "deflection",
                                            "interference", "intermezzo", "quietMove", "xRayAttack", "zugzwang"] },
        { title: qsTr("Mates"), themes: ["mate", "mateIn1", "mateIn2", "mateIn3", "mateIn4", "mateIn5",
                                         "anastasiaMate", "arabianMate", "backRankMate", "bodenMate",
                                         "doubleBishopMate", "dovetailMate", "hookMate", "smotheredMate"] },
        { title: qsTr("Special moves"), themes: ["castling", "enPassant", "promotion", "underPromotion"] },
        { title: qsTr("Goals"), themes: ["equality", "advantage", "crushing"] },
        { title: qsTr("Length"), themes: ["oneMove", "short", "long", "veryLong"] },
        { title: qsTr("Origin"), themes: ["master", "masterVsMaster", "superGM"] }
    ]
}

function name(key) {
    switch (key) {
    case "mix": return qsTr("Healthy mix")
    case "opening": return qsTr("Opening")
    case "middlegame": return qsTr("Middlegame")
    case "endgame": return qsTr("Endgame")
    case "rookEndgame": return qsTr("Rook endgame")
    case "bishopEndgame": return qsTr("Bishop endgame")
    case "pawnEndgame": return qsTr("Pawn endgame")
    case "knightEndgame": return qsTr("Knight endgame")
    case "queenEndgame": return qsTr("Queen endgame")
    case "queenRookEndgame": return qsTr("Queen and rook")
    case "advancedPawn": return qsTr("Advanced pawn")
    case "attackingF2F7": return qsTr("Attacking f2 or f7")
    case "capturingDefender": return qsTr("Capture the defender")
    case "discoveredAttack": return qsTr("Discovered attack")
    case "doubleCheck": return qsTr("Double check")
    case "exposedKing": return qsTr("Exposed king")
    case "fork": return qsTr("Fork")
    case "hangingPiece": return qsTr("Hanging piece")
    case "kingsideAttack": return qsTr("Kingside attack")
    case "pin": return qsTr("Pin")
    case "queensideAttack": return qsTr("Queenside attack")
    case "sacrifice": return qsTr("Sacrifice")
    case "skewer": return qsTr("Skewer")
    case "trappedPiece": return qsTr("Trapped piece")
    case "attraction": return qsTr("Attraction")
    case "clearance": return qsTr("Clearance")
    case "defensiveMove": return qsTr("Defensive move")
    case "deflection": return qsTr("Deflection")
    case "interference": return qsTr("Interference")
    case "intermezzo": return qsTr("Intermezzo")
    case "quietMove": return qsTr("Quiet move")
    case "xRayAttack": return qsTr("X-Ray attack")
    case "zugzwang": return qsTr("Zugzwang")
    case "mate": return qsTr("Checkmate")
    case "mateIn1": return qsTr("Mate in 1")
    case "mateIn2": return qsTr("Mate in 2")
    case "mateIn3": return qsTr("Mate in 3")
    case "mateIn4": return qsTr("Mate in 4")
    case "mateIn5": return qsTr("Mate in 5 or more")
    case "anastasiaMate": return qsTr("Anastasia's mate")
    case "arabianMate": return qsTr("Arabian mate")
    case "backRankMate": return qsTr("Back rank mate")
    case "bodenMate": return qsTr("Boden's mate")
    case "doubleBishopMate": return qsTr("Double bishop mate")
    case "dovetailMate": return qsTr("Dovetail mate")
    case "hookMate": return qsTr("Hook mate")
    case "smotheredMate": return qsTr("Smothered mate")
    case "castling": return qsTr("Castling")
    case "enPassant": return qsTr("En passant")
    case "promotion": return qsTr("Promotion")
    case "underPromotion": return qsTr("Underpromotion")
    case "equality": return qsTr("Equality")
    case "advantage": return qsTr("Advantage")
    case "crushing": return qsTr("Crushing")
    case "oneMove": return qsTr("One-move puzzle")
    case "short": return qsTr("Short puzzle")
    case "long": return qsTr("Long puzzle")
    case "veryLong": return qsTr("Very long puzzle")
    case "master": return qsTr("Master games")
    case "masterVsMaster": return qsTr("Master vs Master games")
    case "superGM": return qsTr("Super GM games")
    case "middlegameTheme": return qsTr("Middlegame")
    default:
        // Unknown keys: "someTheme" -> "Some theme"
        var spaced = key.replace(/([A-Z])/g, " $1").toLowerCase()
        return spaced.charAt(0).toUpperCase() + spaced.slice(1)
    }
}

function difficultyKeys() {
    return ["easiest", "easier", "normal", "harder", "hardest"]
}

function difficultyName(key) {
    switch (key) {
    case "easiest": return qsTr("Easiest")
    case "easier": return qsTr("Easier")
    case "harder": return qsTr("Harder")
    case "hardest": return qsTr("Hardest")
    default: return qsTr("Normal")
    }
}
