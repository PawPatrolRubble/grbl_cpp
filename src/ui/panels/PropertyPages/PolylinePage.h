#pragma once

#include "PropertyPage.h"

class PolylineEntity;
class QLabel;
class QTableWidget;
class QPushButton;

/// Property editor page for PolylineEntity.
/// Uses a table widget for editing X/Y coordinates.
/// Note: This page needs special handling for insert/delete operations
/// which require the Document context (handled by panel).
class PolylinePage : public PropertyPage
{
    Q_OBJECT

public:
    explicit PolylinePage(QWidget *parent = nullptr);

    void bind(const Entity *entity) override;
    std::unique_ptr<Entity> apply() const override;
    int entityType() const override;

    /// Returns the current row index (for insert/delete operations)
    int currentRow() const;

signals:
    /// Emitted when user requests to insert a point after current row
    void insertAfterRequested();
    /// Emitted when user requests to delete the selected point
    void deletePointRequested();

private:
    QLabel *m_pointCountLabel;
    QTableWidget *m_table;
    QPushButton *m_insertBtn;
    QPushButton *m_deleteBtn;
};