// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

// Not a .pragma library, so qsTr() can translate the units.

// Time left for a correspondence move: "2d 14h", "5h 03m", "12:34".
function formatTurnTime(ms) {
    var total = Math.max(0, Math.floor(ms / 1000))
    var days = Math.floor(total / 86400)
    var hours = Math.floor((total % 86400) / 3600)
    var minutes = Math.floor((total % 3600) / 60)
    var seconds = total % 60
    if (days > 0)
        return qsTr("%1d %2h").arg(days).arg(hours)
    if (hours > 0)
        return qsTr("%1h %2m").arg(hours).arg((minutes < 10 ? "0" : "") + minutes)
    return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
}
