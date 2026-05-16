#include <QCoreApplication>

#include <cmath>
#include <iostream>
#include <memory>

#include "../src/grbl/GraphicsDesign/CommandStack.h"
#include "../src/grbl/GraphicsDesign/Document.h"
#include "../src/grbl/GraphicsDesign/AngularDimensionEntity.h"
#include "../src/grbl/GraphicsDesign/LineEntity.h"
#include "../src/grbl/GraphicsDesign/PolylineEntity.h"
#include "../src/grbl/GraphicsDesign/RectangleEntity.h"
#include "../src/grbl/GraphicsDesign/Tools/AngularDimensionTool.h"
#include "../src/grbl/GraphicsDesign/Tools/SelectTool.h"
#include "../src/grbl/GraphicsDesign/Tools/TrimTool.h"
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

    const QPointF rawWorldPos(3.0, 10.0);
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

int runQuickTrimIntersectedLinesProbe()
{
    Document document;
    CommandStack commandStack(document);
    TrimTool trimTool(document, commandStack);

    document.addEntity(std::make_unique<LineEntity>(QPointF(-10.0, 0.0), QPointF(10.0, 0.0)));
    document.addEntity(std::make_unique<LineEntity>(QPointF(0.0, -10.0), QPointF(0.0, 10.0)));

    trimTool.finishCuttingEdgeSelection();
    trimTool.mouseMove(QPointF(-5.0, 0.0));
    trimTool.mousePress(QPointF(-5.0, 0.0));

    std::cout << "Quick trim intersected lines probe" << std::endl;
    std::cout << "  entity count: " << document.entityCount() << std::endl;

    if (document.entityCount() != 2) {
        std::cerr << "Expected trim to keep two entities after trimming one segment." << std::endl;
        return 1;
    }

    const auto* horizontal = dynamic_cast<const LineEntity*>(document.entityAt(0));
    const auto* vertical = dynamic_cast<const LineEntity*>(document.entityAt(1));
    if (!horizontal || !vertical) {
        std::cerr << "Expected both remaining entities to be lines." << std::endl;
        return 1;
    }

    std::cout << "  trimmed horizontal: (" << horizontal->startPoint().x() << ", "
              << horizontal->startPoint().y() << ") -> ("
              << horizontal->endPoint().x() << ", " << horizontal->endPoint().y() << ")" << std::endl;

    if (!nearlyEqual(horizontal->startPoint(), QPointF(0.0, 0.0)) ||
        !nearlyEqual(horizontal->endPoint(), QPointF(10.0, 0.0))) {
        std::cerr << "Expected the left half of the horizontal line to be removed." << std::endl;
        return 1;
    }

    return 0;
}

int runExcludedEndpointSnapProbe()
{
    Document document;
    auto excludedLine = std::make_unique<LineEntity>(QPointF(0.0, 0.0), QPointF(10.0, 0.0));
    const QUuid excludedId = excludedLine->id();
    document.addEntity(std::move(excludedLine));

    CoordinateTransform transforms;
    transforms.setZoom(1.0);
    transforms.setPanOffset(QPointF(0.0, 0.0));

    CanvasSnap snap(document, transforms, 10.0, 14.0);
    snap.setGridSnapEnabled(false);

    const auto result = snap.computeSnap(QPointF(1.0, 0.5), true, std::nullopt, QSet<QUuid>{excludedId});

    std::cout << "Excluded endpoint snap probe" << std::endl;
    std::cout << "  active: " << (result.active ? "true" : "false") << std::endl;

    if (result.active) {
        std::cerr << "Expected excluded entity endpoints to be ignored." << std::endl;
        return 1;
    }

    return 0;
}

int runExcludedIntersectionSnapProbe()
{
    Document document;
    auto horizontal = std::make_unique<LineEntity>(QPointF(-10.0, 0.0), QPointF(10.0, 0.0));
    auto vertical = std::make_unique<LineEntity>(QPointF(0.0, -10.0), QPointF(0.0, 10.0));
    const QUuid excludedId = vertical->id();
    document.addEntity(std::move(horizontal));
    document.addEntity(std::move(vertical));

    CoordinateTransform transforms;
    transforms.setZoom(1.0);
    transforms.setPanOffset(QPointF(0.0, 0.0));

    CanvasSnap snap(document, transforms, 10.0, 14.0);
    snap.setGridSnapEnabled(false);
    snap.setEndpointSnapEnabled(false);
    snap.setMidpointSnapEnabled(false);
    snap.setCenterSnapEnabled(false);
    snap.setNearestSnapEnabled(false);
    snap.setPerpendicularSnapEnabled(false);

    const auto result = snap.computeSnap(QPointF(0.5, 0.5), true, std::nullopt, QSet<QUuid>{excludedId});

    std::cout << "Excluded intersection snap probe" << std::endl;
    std::cout << "  active: " << (result.active ? "true" : "false") << std::endl;

    if (result.active) {
        std::cerr << "Expected intersections involving excluded entities to be ignored." << std::endl;
        return 1;
    }

    return 0;
}

int runHorizontalInferenceProbe()
{
    Document document;
    CoordinateTransform transforms;
    transforms.setZoom(1.0);
    transforms.setPanOffset(QPointF(0.0, 0.0));

    CanvasSnap snap(document, transforms, 10.0, 14.0);
    snap.setGridSnapEnabled(false);

    const QPointF rawWorldPos(12.0, 0.8);
    const QPointF referencePoint(0.0, 0.0);
    const auto inferred = snap.inferPoint(rawWorldPos, {}, referencePoint);

    std::cout << "Horizontal inference probe" << std::endl;
    std::cout << "  label:  " << inferred.label.toStdString() << std::endl;
    std::cout << "  point:  (" << inferred.worldPos.x() << ", " << inferred.worldPos.y() << ")" << std::endl;

    if (inferred.relation != CanvasSnap::InferredRelation::Horizontal ||
        !nearlyEqual(inferred.worldPos, QPointF(12.0, 0.0))) {
        std::cerr << "Expected horizontal inference to clamp Y to the anchor." << std::endl;
        return 1;
    }

    return 0;
}

int runVerticalInferenceProbe()
{
    Document document;
    CoordinateTransform transforms;
    transforms.setZoom(1.0);
    transforms.setPanOffset(QPointF(0.0, 0.0));

    CanvasSnap snap(document, transforms, 10.0, 14.0);
    snap.setGridSnapEnabled(false);

    const QPointF rawWorldPos(0.7, 14.0);
    const QPointF referencePoint(0.0, 0.0);
    const auto inferred = snap.inferPoint(rawWorldPos, {}, referencePoint);

    std::cout << "Vertical inference probe" << std::endl;
    std::cout << "  label:  " << inferred.label.toStdString() << std::endl;
    std::cout << "  point:  (" << inferred.worldPos.x() << ", " << inferred.worldPos.y() << ")" << std::endl;

    if (inferred.relation != CanvasSnap::InferredRelation::Vertical ||
        !nearlyEqual(inferred.worldPos, QPointF(0.0, 14.0))) {
        std::cerr << "Expected vertical inference to clamp X to the anchor." << std::endl;
        return 1;
    }

    return 0;
}

int runCoincidentPriorityProbe()
{
    Document document;
    document.addEntity(std::make_unique<LineEntity>(QPointF(10.0, 10.0), QPointF(20.0, 10.0)));

    CoordinateTransform transforms;
    transforms.setZoom(1.0);
    transforms.setPanOffset(QPointF(0.0, 0.0));

    CanvasSnap snap(document, transforms, 10.0, 14.0);
    snap.setGridSnapEnabled(false);

    const QPointF rawWorldPos(10.6, 10.2);
    const auto snapResult = snap.computeSnap(rawWorldPos, true);
    const auto inferred = snap.inferPoint(rawWorldPos, snapResult, QPointF(0.0, 10.0));

    std::cout << "Coincident priority probe" << std::endl;
    std::cout << "  label:  " << inferred.label.toStdString() << std::endl;
    std::cout << "  point:  (" << inferred.worldPos.x() << ", " << inferred.worldPos.y() << ")" << std::endl;

    if (inferred.relation != CanvasSnap::InferredRelation::Coincident ||
        !nearlyEqual(inferred.worldPos, QPointF(10.0, 10.0))) {
        std::cerr << "Expected coincident snap to take priority over horizontal inference." << std::endl;
        return 1;
    }

    return 0;
}

int runMidpointPriorityProbe()
{
    Document document;
    document.addEntity(std::make_unique<LineEntity>(QPointF(0.0, 5.0), QPointF(10.0, 5.0)));

    CoordinateTransform transforms;
    transforms.setZoom(1.0);
    transforms.setPanOffset(QPointF(0.0, 0.0));

    CanvasSnap snap(document, transforms, 10.0, 14.0);
    snap.setGridSnapEnabled(false);

    const QPointF rawWorldPos(5.3, 5.1);
    const auto snapResult = snap.computeSnap(rawWorldPos, true);
    const auto inferred = snap.inferPoint(rawWorldPos, snapResult, QPointF(0.0, 5.0));

    std::cout << "Midpoint priority probe" << std::endl;
    std::cout << "  label:  " << inferred.label.toStdString() << std::endl;
    std::cout << "  point:  (" << inferred.worldPos.x() << ", " << inferred.worldPos.y() << ")" << std::endl;

    if (inferred.relation != CanvasSnap::InferredRelation::Midpoint ||
        !nearlyEqual(inferred.worldPos, QPointF(5.0, 5.0))) {
        std::cerr << "Expected midpoint snap to take priority over horizontal inference." << std::endl;
        return 1;
    }

    return 0;
}

int runSelectToolLineHandleInferenceProbe()
{
    Document document;
    CommandStack commandStack(document);
    SelectTool selectTool(document, commandStack);

    auto line = std::make_unique<LineEntity>(QPointF(0.0, 0.0), QPointF(10.0, 0.0));
    const QUuid lineId = line->id();
    document.addEntity(std::move(line));
    document.select(lineId);

    selectTool.setHitHandleId("start");
    selectTool.mousePress(QPointF(0.0, 0.0));
    const auto startReference = selectTool.inferenceReferencePoint();

    std::cout << "SelectTool line handle inference probe" << std::endl;
    std::cout << "  start anchor: ("
              << (startReference ? startReference->x() : -999.0) << ", "
              << (startReference ? startReference->y() : -999.0) << ")" << std::endl;

    if (!startReference || !nearlyEqual(*startReference, QPointF(10.0, 0.0))) {
        std::cerr << "Expected dragging the start handle to anchor to the opposite endpoint." << std::endl;
        return 1;
    }
    if (!selectTool.snapExclusionIds().contains(lineId)) {
        std::cerr << "Expected the dragged line to be excluded from snapping." << std::endl;
        return 1;
    }

    selectTool.mouseRelease(QPointF(0.0, 0.0));
    selectTool.setHitHandleId("end");
    selectTool.mousePress(QPointF(10.0, 0.0));
    const auto endReference = selectTool.inferenceReferencePoint();

    std::cout << "  end anchor:   ("
              << (endReference ? endReference->x() : -999.0) << ", "
              << (endReference ? endReference->y() : -999.0) << ")" << std::endl;

    if (!endReference || !nearlyEqual(*endReference, QPointF(0.0, 0.0))) {
        std::cerr << "Expected dragging the end handle to anchor to the opposite endpoint." << std::endl;
        return 1;
    }

    return 0;
}

int runSelectToolPolylineHandleInferenceProbe()
{
    Document document;
    CommandStack commandStack(document);
    SelectTool selectTool(document, commandStack);

    QList<QPointF> points{QPointF(0.0, 0.0), QPointF(5.0, 5.0), QPointF(10.0, 0.0)};
    auto polyline = std::make_unique<PolylineEntity>(points);
    const QUuid polylineId = polyline->id();
    document.addEntity(std::move(polyline));
    document.select(polylineId);

    selectTool.setHitHandleId("pt_0");
    selectTool.mousePress(QPointF(0.0, 0.0));
    const auto startReference = selectTool.inferenceReferencePoint();

    std::cout << "SelectTool polyline handle inference probe" << std::endl;
    std::cout << "  first anchor: ("
              << (startReference ? startReference->x() : -999.0) << ", "
              << (startReference ? startReference->y() : -999.0) << ")" << std::endl;

    if (!startReference || !nearlyEqual(*startReference, QPointF(5.0, 5.0))) {
        std::cerr << "Expected dragging the first polyline vertex to anchor to its neighbor." << std::endl;
        return 1;
    }
    if (!selectTool.snapExclusionIds().contains(polylineId)) {
        std::cerr << "Expected the dragged polyline to be excluded from snapping." << std::endl;
        return 1;
    }

    selectTool.mouseRelease(QPointF(0.0, 0.0));
    selectTool.setHitHandleId("pt_2");
    selectTool.mousePress(QPointF(10.0, 0.0));
    const auto endReference = selectTool.inferenceReferencePoint();

    std::cout << "  last anchor:  ("
              << (endReference ? endReference->x() : -999.0) << ", "
              << (endReference ? endReference->y() : -999.0) << ")" << std::endl;

    if (!endReference || !nearlyEqual(*endReference, QPointF(5.0, 5.0))) {
        std::cerr << "Expected dragging the last polyline vertex to anchor to its neighbor." << std::endl;
        return 1;
    }

    selectTool.mouseRelease(QPointF(10.0, 0.0));
    selectTool.setHitHandleId("pt_1");
    selectTool.mousePress(QPointF(5.0, 5.0));
    const auto middleReference = selectTool.inferenceReferencePoint();

    std::cout << "  middle anchor present: " << (middleReference ? "true" : "false") << std::endl;

    if (middleReference) {
        std::cerr << "Expected interior polyline vertices to skip axis inference anchors." << std::endl;
        return 1;
    }

    return 0;
}

int runSelectToolEmptyClickClearsSelectionProbe()
{
    Document document;
    CommandStack commandStack(document);
    SelectTool selectTool(document, commandStack);

    auto line = std::make_unique<LineEntity>(QPointF(0.0, 0.0), QPointF(10.0, 0.0));
    const QUuid lineId = line->id();
    document.addEntity(std::move(line));
    document.select(lineId);

    selectTool.mousePress(QPointF(100.0, 100.0));
    selectTool.mouseRelease(QPointF(100.0, 100.0));

    std::cout << "SelectTool empty click clears selection probe" << std::endl;
    std::cout << "  selected count: " << document.selectedIds().size() << std::endl;

    if (!document.selectedIds().isEmpty()) {
        std::cerr << "Expected clicking empty canvas space to clear selection." << std::endl;
        return 1;
    }

    document.select(lineId);
    selectTool.setCtrlPressed(true);
    selectTool.mousePress(QPointF(100.0, 100.0));
    selectTool.mouseRelease(QPointF(100.0, 100.0));
    selectTool.setCtrlPressed(false);

    std::cout << "  ctrl-click selected count: " << document.selectedIds().size() << std::endl;

    if (!document.selectedIds().contains(lineId)) {
        std::cerr << "Expected Ctrl+clicking empty canvas space to preserve selection." << std::endl;
        return 1;
    }

    return 0;
}

int runAngularDimensionToolSegmentAngleProbe()
{
    Document document;
    CommandStack commandStack(document);
    AngularDimensionTool angleTool(document, commandStack);

    document.addEntity(std::make_unique<LineEntity>(QPointF(0.0, 0.0), QPointF(10.0, 0.0)));
    document.addEntity(std::make_unique<LineEntity>(QPointF(0.0, 0.0), QPointF(0.0, 10.0)));

    angleTool.mousePress(QPointF(5.0, 0.0));
    if (angleTool.hasMeasurement()) {
        std::cerr << "Expected no final measurement after selecting only the first segment." << std::endl;
        return 1;
    }

    angleTool.mousePress(QPointF(0.0, 5.0));

    std::cout << "AngularDimensionTool segment angle probe" << std::endl;
    std::cout << "  angle: " << angleTool.angle() << std::endl;
    std::cout << "  entity count: " << document.entityCount() << std::endl;

    if (!angleTool.hasMeasurement() || !nearlyEqual(angleTool.angle(), 90.0) || document.entityCount() != 3) {
        std::cerr << "Expected selecting perpendicular line segments to create a 90 degree angle annotation." << std::endl;
        return 1;
    }

    const auto* annotation = dynamic_cast<const AngularDimensionEntity*>(document.entityAt(2));
    if (!annotation || !nearlyEqual(annotation->measuredAngleDegrees(), 90.0)) {
        std::cerr << "Expected the created entity to be a 90 degree angular dimension annotation." << std::endl;
        return 1;
    }
    if (!document.selectedIds().contains(annotation->id())) {
        std::cerr << "Expected the created angle annotation to become selected." << std::endl;
        return 1;
    }

    Document reversedDocument;
    CommandStack reversedCommandStack(reversedDocument);
    AngularDimensionTool reversedAngleTool(reversedDocument, reversedCommandStack);

    reversedDocument.addEntity(std::make_unique<LineEntity>(QPointF(10.0, 0.0), QPointF(0.0, 0.0)));
    reversedDocument.addEntity(std::make_unique<LineEntity>(QPointF(0.0, 0.0), QPointF(5.0, 5.0)));
    reversedAngleTool.mousePress(QPointF(5.0, 0.0));
    reversedAngleTool.mousePress(QPointF(2.5, 2.5));

    const double sharedEndpointAngle = reversedAngleTool.angle();

    std::cout << "  shared-endpoint angle: " << sharedEndpointAngle << std::endl;

    if (!nearlyEqual(sharedEndpointAngle, 45.0)) {
        std::cerr << "Expected shared endpoint angle calculation to ignore stored segment direction." << std::endl;
        return 1;
    }

    Document mixedDocument;
    CommandStack mixedCommandStack(mixedDocument);
    AngularDimensionTool mixedAngleTool(mixedDocument, mixedCommandStack);

    mixedDocument.addEntity(std::make_unique<RectangleEntity>(QPointF(0.0, 0.0), QPointF(10.0, 10.0)));
    mixedDocument.addEntity(std::make_unique<PolylineEntity>(QList<QPointF>{
        QPointF(20.0, 0.0), QPointF(20.0, 10.0), QPointF(25.0, 10.0)
    }));
    mixedAngleTool.mousePress(QPointF(5.0, 0.0));
    mixedAngleTool.mousePress(QPointF(20.0, 5.0));

    std::cout << "  rectangle/polyline angle: " << mixedAngleTool.angle() << std::endl;

    if (!mixedAngleTool.hasMeasurement() || !nearlyEqual(mixedAngleTool.angle(), 90.0) || mixedDocument.entityCount() != 3) {
        std::cerr << "Expected rectangle edges and polyline segments to create an angle annotation." << std::endl;
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

    const int trimProbe = runQuickTrimIntersectedLinesProbe();
    if (trimProbe != 0) {
        return trimProbe;
    }

    const int selectToolEmptyClickProbe = runSelectToolEmptyClickClearsSelectionProbe();
    if (selectToolEmptyClickProbe != 0) {
        return selectToolEmptyClickProbe;
    }

    const int angularDimensionToolSegmentAngleProbe = runAngularDimensionToolSegmentAngleProbe();
    if (angularDimensionToolSegmentAngleProbe != 0) {
        return angularDimensionToolSegmentAngleProbe;
    }

    const int excludedEndpointProbe = runExcludedEndpointSnapProbe();
    if (excludedEndpointProbe != 0) {
        return excludedEndpointProbe;
    }

    const int excludedIntersectionProbe = runExcludedIntersectionSnapProbe();
    if (excludedIntersectionProbe != 0) {
        return excludedIntersectionProbe;
    }

    const int horizontalInferenceProbe = runHorizontalInferenceProbe();
    if (horizontalInferenceProbe != 0) {
        return horizontalInferenceProbe;
    }

    const int verticalInferenceProbe = runVerticalInferenceProbe();
    if (verticalInferenceProbe != 0) {
        return verticalInferenceProbe;
    }

    const int coincidentPriorityProbe = runCoincidentPriorityProbe();
    if (coincidentPriorityProbe != 0) {
        return coincidentPriorityProbe;
    }

    const int midpointPriorityProbe = runMidpointPriorityProbe();
    if (midpointPriorityProbe != 0) {
        return midpointPriorityProbe;
    }

    const int selectToolLineProbe = runSelectToolLineHandleInferenceProbe();
    if (selectToolLineProbe != 0) {
        return selectToolLineProbe;
    }

    const int selectToolPolylineProbe = runSelectToolPolylineHandleInferenceProbe();
    if (selectToolPolylineProbe != 0) {
        return selectToolPolylineProbe;
    }

    std::cout << "All CanvasSnap probes passed." << std::endl;
    return 0;
}