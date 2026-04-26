#include <QCoreApplication>

#include <cmath>
#include <iostream>
#include <memory>

#include "../src/grbl/GraphicsDesign/Document.h"
#include "../src/grbl/GraphicsDesign/LineEntity.h"
#include "../src/grbl/GraphicsDesign/RectangleEntity.h"
#include "../src/ui/canvas/CanvasSnap.h"
#include "../src/ui/canvas/CoordinateTransform.h"

namespace {

bool nearlyEqual(double a, double b, double epsilon = 1e-6)
{
    return std::abs(a - b) <= epsilon;
}

bool nearlyEqual(const QPointF& a, const QPointF& b, double epsilon = 1e-6)
{
    return nearlyEqual(a.x(), b.x(), epsilon) && nearlyEqual(a.y(), b.y(), epsilon);
}

int runRectanglePerpendicularOverrideTest()
{
    Document document;
    document.addEntity(std::make_unique<RectangleEntity>(QPointF(0.0, 0.0), QPointF(100.0, 80.0)));

    CoordinateTransform transforms;
    transforms.setZoom(1.0);
    transforms.setPanOffset(QPointF(0.0, 0.0));

    CanvasSnap snap(document, transforms, 10.0, 14.0);

    const QPointF rawWorldPos(3.0, 3.0);
    const QPointF referencePoint(-50.0, 10.0);
    const QPointF expectedPerpendicular(0.0, 10.0);

    const auto result = snap.computeSnap(rawWorldPos, true, referencePoint);

    std::cout << "Rectangle perpendicular probe" << std::endl;
    std::cout << "  active: " << (result.active ? "true" : "false") << std::endl;
    std::cout << "  label:  " << result.label.toStdString() << std::endl;
    std::cout << "  point:  (" << result.worldPos.x() << ", " << result.worldPos.y() << ")" << std::endl;

    if (!result.active) {
        std::cerr << "Expected an active snap result." << std::endl;
        return 1;
    }
    if (result.label != "Perpendicular") {
        std::cerr << "Expected label Perpendicular but got " << result.label.toStdString() << std::endl;
        return 1;
    }
    if (!nearlyEqual(result.worldPos, expectedPerpendicular)) {
        std::cerr << "Expected point (" << expectedPerpendicular.x() << ", " << expectedPerpendicular.y()
                  << ") but got (" << result.worldPos.x() << ", " << result.worldPos.y() << ")" << std::endl;
        return 1;
    }

    return 0;
}

int runAxisAlignedLineCullingProbe()
{
    const LineEntity verticalLine(QPointF(0.0, 0.0), QPointF(0.0, 10.0));
    const QRectF visibleRect(QPointF(-20.0, -20.0), QPointF(20.0, 20.0));
    const QRectF lineBounds = verticalLine.boundingRect();
    const bool intersects = visibleRect.intersects(lineBounds);

    std::cout << "Axis-aligned line culling probe" << std::endl;
    std::cout << "  bounds: (" << lineBounds.left() << ", " << lineBounds.top()
              << ") - (" << lineBounds.right() << ", " << lineBounds.bottom() << ")" << std::endl;
    std::cout << "  empty:  " << (lineBounds.isEmpty() ? "true" : "false") << std::endl;
    std::cout << "  visible intersects bounds: " << (intersects ? "true" : "false") << std::endl;

    if (!intersects) {
        std::cerr << "Expected a visible vertical line to intersect the visible rect." << std::endl;
        return 1;
    }

    return 0;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const int rectangleProbe = runRectanglePerpendicularOverrideTest();
    if (rectangleProbe != 0) {
        return rectangleProbe;
    }

    const int cullingProbe = runAxisAlignedLineCullingProbe();
    if (cullingProbe != 0) {
        return cullingProbe;
    }

    std::cout << "All CanvasSnap probes passed." << std::endl;
    return 0;
}