#pragma once

#include <QWidget>
#include <QDoubleSpinBox>
#include <memory>

class Entity;
class Document;

/// Base class for entity property editor pages.
/// Each entity type (Line, Rectangle, Circle, etc.) has its own PropertyPage subclass.
/// The page is responsible for:
///   - Creating UI widgets for that entity type's properties
///   - Loading entity values into widgets (bind)
///   - Creating a modified entity from widget values (apply)
///
/// Usage pattern:
///   1. When selection changes, panel calls bind(entity) to populate widgets
///   2. User edits widgets, propertyChanged signal fires
///   3. Panel calls apply() to get modified entity, commits to Document
class PropertyPage : public QWidget
{
    Q_OBJECT

public:
    explicit PropertyPage(QWidget *parent = nullptr);
    virtual ~PropertyPage() = default;

    /// Load entity properties into widgets.
    /// Called when user selects an entity of this type.
    /// @param entity The entity to display (must be correct type for this page)
    virtual void bind(const Entity *entity) = 0;

    /// Create a new entity with modified properties from widget values.
    /// Called when user changes any property widget.
    /// @return A new entity with the modified properties (caller owns it)
    virtual std::unique_ptr<Entity> apply() const = 0;

    /// Returns the entity type this page handles.
    /// Used for routing selection changes to correct page.
    virtual int entityType() const = 0;

signals:
    /// Emitted when any property widget changes.
    /// Panel should call apply() and commit the change.
    void propertyChanged();

protected:
    /// Helper to create a standard double spinbox with common defaults.
    /// @param rangeMin Minimum value
    /// @param rangeMax Maximum value
    /// @param decimals Number of decimal places (default 2)
    /// @return A new QDoubleSpinBox configured for CAD coordinate entry
    static QDoubleSpinBox *createCoordSpinBox(double rangeMin, double rangeMax, int decimals = 2);

    /// Helper to create a standard angle spinbox.
    /// Range -3600 to 3600 degrees, 2 decimals.
    static QDoubleSpinBox *createAngleSpinBox();

    /// Flag to prevent recursive updates.
    /// Set true during bind(), check in signal handlers to avoid apply() loops.
    bool m_updating = false;
};