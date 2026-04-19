#pragma once

#include <QWidget>
#include <QVector3D>
#include <QTransform>
#include <QTimer>
#include <QElapsedTimer>
#include <QPainterPath>
#include "grbl/GcodeParser.h"

class ToolpathCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit ToolpathCanvas(QWidget *parent = nullptr);

    void setMoves(const QList<GcodeMove> &moves);
    void setMovesNoFit(const QList<GcodeMove> &moves);  // No autoFit - preserve zoom/pan
    void setExecutingLine(int lineNumber);
    void setCutterPosition(const QVector3D &pos);
    void highlightSegment(int lineNumber);
    void setWorktableSize(const QSizeF &size);

signals:
    void segmentClicked(int lineNumber);
    void movesTransformed(const QList<GcodeMove> &newMoves);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    // Coordinate helpers
    void autoFit();
    QPointF machineToScreen(const QPointF &pt) const;
    QPointF screenToMachine(const QPointF &pt) const;

    // Drawing
    void drawGridAndRulers(QPainter &painter);
    void drawWorktable(QPainter &painter);
    void drawAxisOrigin(QPainter &painter);
    void drawMoves(QPainter &painter);
    void drawCutter(QPainter &painter);
    void drawBoundingBox(QPainter &painter);
    void drawCrosshair(QPainter &painter);
    void rebuildPaths();

    // Hit testing
    double distanceToSegment(const QPointF &p, const QPointF &a, const QPointF &b) const;
    QVector<QPointF> approximateArc(const GcodeMove &move) const;
    int findClickedSegment(const QPointF &screenPos) const;

    // Bounding box interaction
    enum class HandleId {
        None, TopLeft, TopCenter, TopRight,
        MiddleLeft, MiddleRight,
        BottomLeft, BottomCenter, BottomRight,
        Rotation
    };
    enum class DragMode { None, Pan, Translate, Scale, Rotate };

    QRectF computeBoundingBox() const;
    QPointF handleScreenPos(HandleId id, const QRectF &bbox) const;
    HandleId hitTestHandles(const QPointF &screenPos) const;
    bool isInsideBBox(const QPointF &screenPos) const;
    Qt::CursorShape cursorForHandle(HandleId id) const;

    // Throttled update helper
    void scheduleUpdate();

    // Move data
    QList<GcodeMove> m_moves;
    int m_executingLine = -1;
    int m_highlightedLine = -1;
    QVector3D m_cutterPos;
    QSizeF m_worktableSize = QSizeF(300, 300);

    // View transform (pan/zoom)
    double m_scale = 1.0;
    QPointF m_offset;

    // Drag state
    DragMode m_dragMode = DragMode::None;
    HandleId m_activeHandle = HandleId::None;
    QPointF m_dragStart;         // screen coords at press
    QPointF m_dragStartMachine;  // machine coords at press
    QRectF  m_dragStartBBox;     // bbox at press
    double  m_dragStartAngle = 0;
    QList<GcodeMove> m_preDragMoves; // moves snapshot at press

    // Repaint throttle
    QTimer m_repaintTimer;
    bool m_repaintPending = false;

    // Cached bounding box
    mutable QRectF m_cachedBBox;
    mutable bool m_bboxDirty = true;

    // Cached line vectors (rebuilt when moves/view change)
    QVector<QLineF> m_rapidLines;
    QVector<QLineF> m_linearLines;
    QVector<QLineF> m_arcLines;
    bool m_pathsDirty = true;
    double m_pathScale = 0.0;
    QPointF m_pathOffset;

    // Cached past-move segments for efficient batch drawing
    QVector<QLineF> m_pastSegments;
    int m_pastBuiltLine = -1;   // executing line when cache was last built

    // Mouse crosshair tracking
    QPointF m_mouseScreenPos;
    bool m_mouseInWidget = false;

    // lineNumber → move index lookup (O(1) highlight)
    QHash<int, int> m_lineToIndex;
};
