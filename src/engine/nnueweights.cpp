// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "nnueweights.h"

#include "core/lichessapi.h"
#include "core/services.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>

#include "evaluate.h"          // EvalFileDefaultNameBig / ...Small
#include "nnue/nnue_common.h"  // the version every readable network starts with

namespace {

// Where Stockfish publishes the networks its releases were trained with.
const char NetworkUrlBase[] = "https://data.stockfishchess.org/nn/";

// A network is a few megabytes at the very least; anything smaller is an
// error page that came back with the wrong status.
const qint64 SmallestNetwork = 1024 * 1024;

// Only used to tell the user what the download will cost before it starts;
// the real sizes come from the server.
const qint64 ExpectedTotalBytes = 65 * 1024 * 1024;

QString bigName()
{
    return QString::fromLatin1(EvalFileDefaultNameBig);
}

QString smallName()
{
    return QString::fromLatin1(EvalFileDefaultNameSmall);
}

} // namespace

NnueWeights::NnueWeights(QObject *parent)
    : QObject(parent)
{
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

QString NnueWeights::bigPath() const
{
    return pathFor(bigName());
}

QString NnueWeights::smallPath() const
{
    return pathFor(smallName());
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
    return looksUsable(bigPath()) && looksUsable(smallPath());
}

qint64 NnueWeights::storedBytes() const
{
    return QFileInfo(bigPath()).size() + QFileInfo(smallPath()).size();
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
    if (!looksUsable(bigPath()))
        missing.append(bigName());
    if (!looksUsable(smallPath()))
        missing.append(smallName());
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
    QNetworkRequest request(QUrl(QString::fromLatin1(NetworkUrlBase) + fileName));
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    m_reply = nam->get(request);
    emit changed();

    connect(m_reply.data(), &QNetworkReply::readyRead, this, [this]() {
        if (m_file)
            m_file->write(m_reply->readAll());
    });
    connect(m_reply.data(), &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
        m_currentBytes = received;
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

        m_file->write(reply->readAll());
        const QString partPath = m_file->fileName();
        const bool complete = reply->error() == QNetworkReply::NoError;
        m_file->close();
        delete m_file;
        m_file = nullptr;

        if (!complete) {
            QFile::remove(partPath);
            finish(reply->errorString());
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
    QFile::remove(bigPath());
    QFile::remove(smallPath());
    finish(QString());
}
