// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pieceimageprovider.h"

#include <QMutexLocker>
#include <QPainter>
#include <QRegularExpression>
#include <QSvgRenderer>

PieceImageProvider::PieceImageProvider(const QString &piecesDir)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_piecesDir(piecesDir)
{
}

QImage PieceImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QSize imageSize(requestedSize.width() > 0 ? requestedSize.width() : 128,
                          requestedSize.height() > 0 ? requestedSize.height() : 128);
    if (size)
        *size = imageSize;

    QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    const QSharedPointer<QSvgRenderer> svg = renderer(id);
    if (svg) {
        QMutexLocker lock(&m_mutex); // QSvgRenderer is not thread-safe
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        svg->render(&painter);
    }
    return image;
}

QSharedPointer<QSvgRenderer> PieceImageProvider::renderer(const QString &id)
{
    // Only "<set>/<color><piece>" ids, so no path can escape the pieces dir.
    static const QRegularExpression validId(QStringLiteral("^[a-z0-9_-]+/[wb][KQRBNP]$"));
    if (!validId.match(id).hasMatch())
        return QSharedPointer<QSvgRenderer>();

    QMutexLocker lock(&m_mutex);
    auto it = m_renderers.constFind(id);
    if (it != m_renderers.constEnd())
        return it.value();

    QSharedPointer<QSvgRenderer> svg(new QSvgRenderer(m_piecesDir + QLatin1Char('/') + id + QStringLiteral(".svg")));
    if (!svg->isValid())
        svg.clear();
    m_renderers.insert(id, svg);
    return svg;
}
