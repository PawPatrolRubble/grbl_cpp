#pragma once

#include <QStyledItemDelegate>

// ---------------------------------------------------------------------------
// LayerListDelegate — renders an eye icon in the decoration column to toggle
// layer visibility, replacing the built-in checkbox rendering.
// ---------------------------------------------------------------------------
class LayerListDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *p, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};