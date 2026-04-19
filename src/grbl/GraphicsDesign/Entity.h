#pragma once

#include "../GcodeParser.h"

#include <QJsonObject>
#include <QPointF>
#include <QUuid>
#include <QPainter>
#include <memory>
#include <vector>
#include <optional>

/// Abstract base class for all drawable entities.
/// Merges tinyCad's Entity (editHandles, snapPoints, distanceTo) with
/// grbl_cpp's Shape (toGcodeMoves, toJson).
class Entity
{
public:
    enum class Kind {
        Line,
        Rectangle,
        Circle,
        Arc,
        Polygon,
        Text,
        LinearDimension
    };

    enum class HandleRole {
        Resize,
        Rotate
    };

    struct EditHandle {
        QString id;                     // Unique handle identifier (e.g., "start", "corner_tl")
        QPointF worldPos;               // Position in world coordinates
        HandleRole role = HandleRole::Resize;
        double angleDegrees = 0.0;      // Handle rotation for rendering
        bool hasLeader = false;         // Whether handle has leader line to anchor
        QPointF leaderAnchor;           // Anchor point for leader line
    };

    explicit Entity(Kind kind);
    virtual ~Entity() = default;

    // Identity
    QUuid id() const { return m_id; }
    void setId(const QUuid& id) { m_id = id; }
    virtual Kind kind() const { return m_kind; }

    // Geometry
    virtual QRectF boundingRect() const = 0;
    virtual QPointF center() const = 0;

    // Rendering
    virtual void draw(QPainter& painter) const = 0;

    // Interaction
    virtual double distanceTo(const QPointF& point) const = 0;   // For hit-testing
    virtual void translate(const QPointF& delta) = 0;           // For moving
    virtual std::unique_ptr<Entity> clone() const = 0;          // Deep copy

    // Handle editing
    virtual std::vector<EditHandle> editHandles() const = 0;
    virtual std::unique_ptr<Entity> withDraggedHandle(const QString& handleId, const QPointF& worldPos) const = 0;

    // Snap points for constraint system
    virtual std::vector<QPointF> snapPoints() const = 0;

    // G-code generation (for CNC output)
    virtual QList<GcodeMove> toGcodeMoves(double zSafe, double zCut, double feedRate) const = 0;

    // Serialization
    virtual QJsonObject toJson() const = 0;
    static std::unique_ptr<Entity> fromJson(const QJsonObject& json);

    // Rotation (optional, some entities support it)
    double rotation() const { return m_rotation; }
    void setRotation(double degrees) { m_rotation = degrees; }

protected:
    QUuid m_id;
    Kind m_kind;
    double m_rotation = 0.0;
};