#pragma once

#include <QWidget>
#include <QGroupBox>
#include <QStyledItemDelegate>

class QListWidget;
class QListWidgetItem;
class QPushButton;
class QPainter;
class QStyleOptionViewItem;
class QModelIndex;

/// Custom delegate for layer list items in LayerPanel.
/// Paints visibility icon, lock icon, and color swatch on left side of each row.
/// Note: Separate from GDP's LayerListDelegate to avoid linker conflicts during extraction.
class LayerItemDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *p, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};

/// Layer management panel.
/// Displays a list of layers with visibility/lock/color indicators.
/// Buttons: Add, Delete, Lock, Move Selected.
///
/// Emits signals for layer operations. Parent panel handles actual operations.
class LayerPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LayerPanel(QWidget *parent = nullptr);

    /// Refresh the layer list from document data
    void refreshFromDocument();

    /// Enable/disable buttons based on selection state
    void updateButtonStates(bool hasSelection, int currentLayerIndex, bool isLocked);

signals:
    void addLayerRequested();
    void deleteLayerRequested(int layerIndex);
    void lockLayerRequested(int layerIndex);
    void moveSelectionToLayerRequested(int layerIndex);
    void layerSelectionChanged(int layerIndex);
    void layerVisibilityChanged(int layerIndex, bool visible);
    void layerLockChanged(int layerIndex, bool locked);

private slots:
    void onCurrentRowChanged(int row);
    void onItemChanged(QListWidgetItem *item);

private:
    QGroupBox *m_group;
    QListWidget *m_list;
    QPushButton *m_addBtn;
    QPushButton *m_deleteBtn;
    QPushButton *m_lockBtn;
    QPushButton *m_moveBtn;
};