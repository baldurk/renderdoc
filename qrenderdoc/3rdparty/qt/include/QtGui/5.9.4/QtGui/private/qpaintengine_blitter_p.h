/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QPAINTENGINE_BLITTER_P_H
#define QPAINTENGINE_BLITTER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtGui/private/qtguiglobal_p.h>
#include "private/qpaintengine_raster_p.h"

#ifndef QT_NO_BLITTABLE
QT_BEGIN_NAMESPACE

class QBlitterPaintEnginePrivate;
class QBlittablePlatformPixmap;
class QBlittable;

class Q_GUI_EXPORT QBlitterPaintEngine : public QRasterPaintEngine
{
    Q_DECLARE_PRIVATE(QBlitterPaintEngine)
public:
    QBlitterPaintEngine(QBlittablePlatformPixmap *p);

    virtual QPaintEngine::Type type() const Q_DECL_OVERRIDE
    { return Blitter; }

    virtual bool begin(QPaintDevice *pdev) Q_DECL_OVERRIDE;
    virtual bool end() Q_DECL_OVERRIDE;

    // Call down into QBlittable
    void fill(const QVectorPath &path, const QBrush &brush) Q_DECL_OVERRIDE;
    void fillRect(const QRectF &rect, const QBrush &brush) Q_DECL_OVERRIDE;
    void fillRect(const QRectF &rect, const QColor &color) Q_DECL_OVERRIDE;
    void drawRects(const QRect *rects, int rectCount) Q_DECL_OVERRIDE;
    void drawRects(const QRectF *rects, int rectCount) Q_DECL_OVERRIDE;
    void drawPixmap(const QPointF &p, const QPixmap &pm) Q_DECL_OVERRIDE;
    void drawPixmap(const QRectF &r, const QPixmap &pm, const QRectF &sr) Q_DECL_OVERRIDE;

    // State tracking
    void setState(QPainterState *s) Q_DECL_OVERRIDE;
    virtual void clipEnabledChanged() Q_DECL_OVERRIDE;
    virtual void penChanged() Q_DECL_OVERRIDE;
    virtual void brushChanged() Q_DECL_OVERRIDE;
    virtual void opacityChanged() Q_DECL_OVERRIDE;
    virtual void compositionModeChanged() Q_DECL_OVERRIDE;
    virtual void renderHintsChanged() Q_DECL_OVERRIDE;
    virtual void transformChanged() Q_DECL_OVERRIDE;

    // Override to lock the QBlittable before using raster
    void drawPolygon(const QPointF *points, int pointCount, PolygonDrawMode mode) Q_DECL_OVERRIDE;
    void drawPolygon(const QPoint *points, int pointCount, PolygonDrawMode mode) Q_DECL_OVERRIDE;
    void fillPath(const QPainterPath &path, QSpanData *fillData) Q_DECL_OVERRIDE;
    void fillPolygon(const QPointF *points, int pointCount, PolygonDrawMode mode) Q_DECL_OVERRIDE;
    void drawEllipse(const QRectF &rect) Q_DECL_OVERRIDE;
    void drawImage(const QPointF &p, const QImage &img) Q_DECL_OVERRIDE;
    void drawImage(const QRectF &r, const QImage &pm, const QRectF &sr,
                   Qt::ImageConversionFlags flags = Qt::AutoColor) Q_DECL_OVERRIDE;
    void drawTiledPixmap(const QRectF &r, const QPixmap &pm, const QPointF &sr) Q_DECL_OVERRIDE;
    void drawTextItem(const QPointF &p, const QTextItem &textItem) Q_DECL_OVERRIDE;
    void drawPoints(const QPointF *points, int pointCount) Q_DECL_OVERRIDE;
    void drawPoints(const QPoint *points, int pointCount) Q_DECL_OVERRIDE;
    void stroke(const QVectorPath &path, const QPen &pen) Q_DECL_OVERRIDE;
    void drawStaticTextItem(QStaticTextItem *) Q_DECL_OVERRIDE;
    bool drawCachedGlyphs(int numGlyphs, const glyph_t *glyphs, const QFixedPoint *positions,
                          QFontEngine *fontEngine) Q_DECL_OVERRIDE;
};

QT_END_NAMESPACE
#endif //QT_NO_BLITTABLE
#endif // QPAINTENGINE_BLITTER_P_H

