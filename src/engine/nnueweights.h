// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NNUEWEIGHTS_H
#define NNUEWEIGHTS_H

#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QString>

class QFile;

// Stockfish's neural networks, kept on the phone next to the app's data.
//
// They are more than 60 MB together, far too much to put in the package, so
// they are downloaded the first time the user switches the engine on, the
// same way the Lichess mobile app fetches them. Exposed to QML as
// "engineWeights".
//
// A file is only moved into place once it has arrived whole and looks like a
// network Stockfish can read: Stockfish ends the process rather than the
// search when a network fails to load, so a half-written file must never be
// handed to it.
class NnueWeights : public QObject
{
    Q_OBJECT
    // Both networks are in place and usable.
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool downloading READ downloading NOTIFY changed)
    // 0..1 over both files together.
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    // What a download will cost, in bytes, and what is already there.
    Q_PROPERTY(qint64 totalBytes READ totalBytes NOTIFY progressChanged)
    Q_PROPERTY(qint64 storedBytes READ storedBytes NOTIFY changed)
    Q_PROPERTY(QString errorString READ errorString NOTIFY changed)

public:
    explicit NnueWeights(QObject *parent = nullptr);
    ~NnueWeights() override;

    bool ready() const;
    bool downloading() const { return m_reply != nullptr; }
    qreal progress() const;
    qint64 totalBytes() const { return m_totalBytes; }
    qint64 storedBytes() const;
    QString errorString() const { return m_error; }

    // Where the files are, whether or not they exist yet.
    QString bigPath() const;
    QString smallPath() const;

    Q_INVOKABLE void download();
    Q_INVOKABLE void cancel();
    // Deletes the files again, to get the space back.
    Q_INVOKABLE void remove();

    // Whether |path| holds something Stockfish will accept: big enough, and
    // starting with the version its readers expect.
    static bool looksUsable(const QString &path);

signals:
    void changed();
    void progressChanged();
    void readyChanged();

private:
    void startNext();
    void fetch(const QString &fileName);
    void finish(const QString &error);
    QString directory() const;
    QString pathFor(const QString &fileName) const;
    QStringList missingFiles() const;

    QPointer<QNetworkReply> m_reply;
    QFile *m_file = nullptr;      // the part being written
    QString m_current;            // file name being downloaded
    QStringList m_queue;
    QString m_error;
    qint64 m_totalBytes = 0;      // of the download as a whole
    qint64 m_doneBytes = 0;       // of the files already finished
    qint64 m_currentBytes = 0;
    bool m_wasReady = false;
};

#endif // NNUEWEIGHTS_H
