// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "nnueweights.h"
#include "core/logging.h"

#include "core/lichessapi.h"
#include "core/services.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

#include "evaluate.h"          // EvalFileDefaultName
#include "nnue/nnue_common.h"  // the version every readable network starts with

namespace {

// Where Stockfish publishes the networks its releases were trained with.
const char NetworkUrlBase[] = "https://data.stockfishchess.org/nn/";

// A network is a few megabytes at the very least; anything smaller is an
// error page that came back with the wrong status.
const qint64 SmallestNetwork = 1024 * 1024;

// No network comes anywhere near this; a download growing past it is not one.
const qint64 LargestNetwork = 160LL * 1024 * 1024;

// Where the networks may come from, redirects included.
const char NetworkHost[] = "data.stockfishchess.org";

// Stockfish names a network "nn-<first 12 hex digits of its SHA-256>.nnue".
// Empty if the name does not follow that.
QString expectedHashPrefix(const QString &fileName)
{
    static const QRegularExpression pattern(QStringLiteral("^nn-([0-9a-f]{12})\\.nnue$"));
    const QRegularExpressionMatch match = pattern.match(fileName);
    return match.hasMatch() ? match.captured(1) : QString();
}

// Only used to tell the user what the download will cost before it starts;
// the real sizes come from the server.
const qint64 ExpectedTotalBytes = 94 * 1024 * 1024;

QString networkName()
{
    return QString::fromLatin1(EvalFileDefaultName);
}

} // namespace

NnueWeights::NnueWeights(QObject *parent)
    : QObject(parent)
{
    removeObsolete();
    m_wasReady = ready();
}

NnueWeights::~NnueWeights()
{
    cancel();
}

QString NnueWeights::directory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/nnue");
}

QString NnueWeights::pathFor(const QString &fileName) const
{
    return directory() + QLatin1Char('/') + fileName;
}

QString NnueWeights::networkPath() const
{
    return pathFor(networkName());
}

void NnueWeights::removeObsolete()
{
    // Networks of earlier Stockfish versions, and downloads that never
    // finished: nothing can use them, and each is tens of megabytes.
    const QDir dir(directory());
    const QStringList names = dir.entryList({ QStringLiteral("nn-*.nnue"), QStringLiteral("nn-*.nnue.part") },
                                            QDir::Files);
    for (const QString &name : names) {
        if (name != networkName())
            QFile::remove(dir.filePath(name));
    }
}

bool NnueWeights::looksUsable(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    if (file.size() < SmallestNetwork)
        return false;
    // Stockfish reads the version as a little-endian 32-bit word and refuses
    // anything else; checking it here keeps a wrong file away from it.
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    quint32 version = 0;
    stream >> version;
    return stream.status() == QDataStream::Ok
            && version == quint32(Stockfish::Eval::NNUE::Version);
}

bool NnueWeights::ready() const
{
    return looksUsable(networkPath());
}

qint64 NnueWeights::storedBytes() const
{
    return QFileInfo(networkPath()).size();
}

qreal NnueWeights::progress() const
{
    if (!downloading() || m_totalBytes <= 0)
        return 0;
    return qBound(qreal(0), qreal(m_doneBytes + m_currentBytes) / qreal(m_totalBytes), qreal(1));
}

QStringList NnueWeights::missingFiles() const
{
    QStringList missing;
    if (!looksUsable(networkPath()))
        missing.append(networkName());
    return missing;
}

void NnueWeights::download()
{
    if (downloading())
        return;
    m_queue = missingFiles();
    if (m_queue.isEmpty()) {
        emit changed();
        return;
    }
    qCDebug(lcEngine, "downloading %d network file(s)", int(m_queue.size()));
    m_error.clear();
    m_doneBytes = 0;
    m_currentBytes = 0;
    m_totalBytes = ExpectedTotalBytes;
    startNext();
}

void NnueWeights::startNext()
{
    if (m_queue.isEmpty()) {
        finish(QString());
        return;
    }
    fetch(m_queue.takeFirst());
}

void NnueWeights::fetch(const QString &fileName)
{
    QNetworkAccessManager *nam = Services::api() ? Services::api()->networkAccessManager() : nullptr;
    if (!nam) {
        finish(tr("No network access"));
        return;
    }
    QDir().mkpath(directory());

    // Written next to the real file and moved into place only once it is
    // whole: Stockfish must never see a partial network.
    m_file = new QFile(pathFor(fileName) + QStringLiteral(".part"), this);
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        delete m_file;
        m_file = nullptr;
        finish(tr("Cannot write to %1").arg(directory()));
        return;
    }

    m_current = fileName;
    m_currentBytes = 0;
    m_redirectRefused = false;
    m_tooLarge = false;
    m_hash.reset();
    QNetworkRequest request(QUrl(QString::fromLatin1(NetworkUrlBase) + fileName));
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    m_reply = nam->get(request);
    emit changed();

    connect(m_reply.data(), &QNetworkReply::readyRead, this, [this]() {
        if (!m_file)
            return;
        const QByteArray chunk = m_reply->readAll();
        m_hash.addData(chunk);
        m_file->write(chunk);
    });
    // The token is never sent here, but a network must still only come from
    // the Stockfish server, over https.
    connect(m_reply.data(), &QNetworkReply::redirected, this, [this](const QUrl &url) {
        if (url.scheme() != QLatin1String("https") || url.host() != QLatin1String(NetworkHost)) {
            m_redirectRefused = true;
            if (m_reply)
                m_reply->abort();
        }
    });
    connect(m_reply.data(), &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
        m_currentBytes = received;
        if (received > LargestNetwork && m_reply) {
            m_tooLarge = true;
            m_reply->abort();
            return;
        }
        // The first answer says how big this file really is; the other one
        // is guessed at until its turn comes.
        if (total > 0 && m_queue.isEmpty() && m_doneBytes + total > 0)
            m_totalBytes = qMax(m_totalBytes, m_doneBytes + total);
        emit progressChanged();
    });
    connect(m_reply.data(), &QNetworkReply::finished, this, [this]() {
        QNetworkReply *reply = m_reply;
        m_reply.clear();
        reply->deleteLater();
        if (!m_file)
            return; // cancelled

        const QByteArray rest = reply->readAll();
        m_hash.addData(rest);
        m_file->write(rest);
        const QString partPath = m_file->fileName();
        const bool complete = reply->error() == QNetworkReply::NoError
                && !m_redirectRefused && !m_tooLarge;
        m_file->close();
        delete m_file;
        m_file = nullptr;

        if (!complete) {
            QFile::remove(partPath);
            finish(m_redirectRefused ? tr("The download was sent somewhere else")
                   : m_tooLarge ? tr("The downloaded network is too large")
                   : reply->errorString());
            return;
        }
        const QString wanted = expectedHashPrefix(m_current);
        if (!wanted.isEmpty()
                && !QString::fromLatin1(m_hash.result().toHex()).startsWith(wanted)) {
            QFile::remove(partPath);
            finish(tr("The downloaded network is corrupt"));
            return;
        }
        if (!looksUsable(partPath)) {
            QFile::remove(partPath);
            finish(tr("The downloaded network is not usable"));
            return;
        }

        const QString path = pathFor(m_current);
        QFile::remove(path);
        if (!QFile::rename(partPath, path)) {
            QFile::remove(partPath);
            finish(tr("Cannot write to %1").arg(directory()));
            return;
        }
        m_doneBytes += QFileInfo(path).size();
        m_currentBytes = 0;
        startNext();
    });
}

void NnueWeights::finish(const QString &error)
{
    if (!error.isEmpty())
        qCWarning(lcEngine, "network download failed: %s", qPrintable(error));
    else
        qCDebug(lcEngine, "network download finished or stopped");
    m_error = error;
    m_queue.clear();
    m_currentBytes = 0;
    const bool nowReady = ready();
    emit changed();
    emit progressChanged();
    if (nowReady != m_wasReady) {
        m_wasReady = nowReady;
        emit readyChanged();
    }
}

void NnueWeights::cancel()
{
    if (m_reply) {
        QNetworkReply *reply = m_reply;
        m_reply.clear();
        reply->abort();
        reply->deleteLater();
    }
    if (m_file) {
        const QString partPath = m_file->fileName();
        m_file->close();
        delete m_file;
        m_file = nullptr;
        QFile::remove(partPath);
    }
    finish(QString());
}

void NnueWeights::remove()
{
    cancel();
    QFile::remove(networkPath());
    finish(QString());
}
