// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PIECEIMAGEPROVIDER_H
#define PIECEIMAGEPROVIDER_H

#include <QHash>
#include <QMutex>
#include <QQuickImageProvider>
#include <QSharedPointer>

class QSvgRenderer;

// Renders the SVG chess pieces at exactly the size the board needs:
//   image://pieces/<set>/<piece>, e.g. image://pieces/cburnett/wN
// Using QtSvg directly avoids depending on the SVG image-format plugin.
class PieceImageProvider : public QQuickImageProvider
{
public:
    explicit PieceImageProvider(const QString &piecesDir);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    QSharedPointer<QSvgRenderer> renderer(const QString &id);

    QString m_piecesDir;
    QMutex m_mutex;
    QHash<QString, QSharedPointer<QSvgRenderer>> m_renderers;
};

#endif // PIECEIMAGEPROVIDER_H
