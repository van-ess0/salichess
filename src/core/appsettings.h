// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QObject>
#include <QSettings>

// User preferences, exposed to QML as "appSettings".
class AppSettings : public QObject
{
    Q_OBJECT
    // "brown", "blue", "green" or "ambience"
    Q_PROPERTY(QString boardTheme READ boardTheme WRITE setBoardTheme NOTIFY boardThemeChanged)
    Q_PROPERTY(QString pieceSet READ pieceSet WRITE setPieceSet NOTIFY pieceSetChanged)
    Q_PROPERTY(bool showCoordinates READ showCoordinates WRITE setShowCoordinates NOTIFY showCoordinatesChanged)
    Q_PROPERTY(bool showLegalMoves READ showLegalMoves WRITE setShowLegalMoves NOTIFY showLegalMovesChanged)
    Q_PROPERTY(bool animatePieces READ animatePieces WRITE setAnimatePieces NOTIFY animatePiecesChanged)
    Q_PROPERTY(bool keepScreenOn READ keepScreenOn WRITE setKeepScreenOn NOTIFY keepScreenOnChanged)
    Q_PROPERTY(bool notifications READ notifications WRITE setNotifications NOTIFY notificationsChanged)
    // "easiest", "easier", "normal", "harder", "hardest"
    Q_PROPERTY(QString puzzleDifficulty READ puzzleDifficulty WRITE setPuzzleDifficulty NOTIFY puzzleDifficultyChanged)
    // How many puzzles PuzzleStore keeps on the phone for offline play.
    Q_PROPERTY(int offlinePuzzles READ offlinePuzzles WRITE setOfflinePuzzles NOTIFY offlinePuzzlesChanged)

public:
    explicit AppSettings(QObject *parent = nullptr);

    QString boardTheme() const;
    void setBoardTheme(const QString &theme);
    QString pieceSet() const;
    void setPieceSet(const QString &set);
    bool showCoordinates() const;
    void setShowCoordinates(bool show);
    bool showLegalMoves() const;
    void setShowLegalMoves(bool show);
    bool animatePieces() const;
    void setAnimatePieces(bool animate);
    bool keepScreenOn() const;
    void setKeepScreenOn(bool keep);
    bool notifications() const;
    void setNotifications(bool enabled);
    QString puzzleDifficulty() const;
    void setPuzzleDifficulty(const QString &difficulty);
    int offlinePuzzles() const;
    void setOfflinePuzzles(int count);

    // The most puzzles that can be kept, so a slip of the finger cannot ask
    // Lichess for thousands.
    static int maxOfflinePuzzles() { return 100; }

signals:
    void boardThemeChanged();
    void pieceSetChanged();
    void showCoordinatesChanged();
    void showLegalMovesChanged();
    void animatePiecesChanged();
    void keepScreenOnChanged();
    void notificationsChanged();
    void puzzleDifficultyChanged();
    void offlinePuzzlesChanged();

private:
    bool store(const char *key, const QVariant &value);

    QSettings m_settings;
};

#endif // APPSETTINGS_H
