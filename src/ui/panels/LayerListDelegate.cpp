#include "LayerListDelegate.h"

#include <QPainter>
#include <QApplication>
#include <QStyle>
#include <QColor>

// Layout constants for icon columns
static const int kEyeColW   = 22;   // pixels reserved on the left for the eye icon
static const int kLockColW  = 22;   // pixels for the lock icon next to eye
static const int kColorSwW  = 16;   // pixels for the color swatch
static const int kIconsW    = kEyeColW + kLockColW;  // total icon cols before text
static const int kSwatchOff = kIconsW + 2;           // swatch left edge offset

void LayerListDelegate::paint(QPainter *p, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    // Draw default selection background
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.decorationSize = QSize(0, 0);   // hide normal decoration
    opt.features &= ~QStyleOptionViewItem::HasCheckIndicator;

    // Inset the text rect so it starts after our custom icons
    // kSwatchOff + kColorSwW = left edge of text starts here
    int textLeftMargin = kSwatchOff + kColorSwW + 4;
    opt.rect = opt.rect.adjusted(textLeftMargin, 0, 0, 0);

    QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &opt, p);

    // Draw icons on the LEFT side of the ORIGINAL rectangle (not inset)
    bool visible = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
    bool locked  = index.data(Qt::UserRole + 1).toBool();
    QColor layerColor = index.data(Qt::UserRole + 2).value<QColor>();
    if (!layerColor.isValid()) layerColor = QColor("#374151");
    QRect eyeRect( option.rect.left(),          option.rect.top(), kEyeColW,  option.rect.height());
    QRect lockRect(option.rect.left()+kEyeColW, option.rect.top(), kLockColW, option.rect.height());
    p->save();
    p->setFont(QFont("Segoe UI Emoji", 11));
    p->setPen(visible ? QColor("#374151") : QColor("#6b7280"));
    p->drawText(eyeRect, Qt::AlignCenter, visible ? "\U0001F441" : "\u2298");
    p->setPen(locked ? QColor("#dc2626") : QColor("#6b7280"));
    p->drawText(lockRect, Qt::AlignCenter, locked ? "\U0001F512" : "\U0001F513");
    // Color swatch
    QRect swRect(option.rect.left() + kSwatchOff,
                 option.rect.top() + (option.rect.height() - kColorSwW) / 2,
                 kColorSwW, kColorSwW);
    p->setPen(QColor("#6b7280"));
    p->setBrush(layerColor);
    p->drawRect(swRect);
    p->restore();
}

QSize LayerListDelegate::sizeHint(const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const
{
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    return QSize(s.width(), std::max(s.height(), 22));
}