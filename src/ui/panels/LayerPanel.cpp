#include "LayerPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QApplication>
#include <QStyle>
#include <QPainter>
#include <QColor>
#include <QFont>

// Layout constants for layer icons
static const int kEyeColW   = 22;   // pixels reserved for eye icon
static const int kLockColW  = 22;   // pixels for lock icon
static const int kColorSwW  = 16;   // pixels for color swatch
static const int kIconsW    = kEyeColW + kLockColW;
static const int kSwatchOff = kIconsW + 2;

// -------------------------------------------------------------------------
// LayerItemDelegate implementation
// -------------------------------------------------------------------------

void LayerItemDelegate::paint(QPainter *p, const QStyleOptionViewItem &option,
                               const QModelIndex &index) const
{
    // Draw default selection background
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.decorationSize = QSize(0, 0);   // hide normal decoration
    opt.features &= ~QStyleOptionViewItem::HasCheckIndicator;

    // Inset the text rect so it starts after our custom icons
    int textLeftMargin = kSwatchOff + kColorSwW + 4;
    opt.rect = opt.rect.adjusted(textLeftMargin, 0, 0, 0);

    QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &opt, p);

    // Draw icons on the LEFT side of the ORIGINAL rectangle
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

QSize LayerItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const
{
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    return QSize(s.width(), std::max(s.height(), 22));
}

// -------------------------------------------------------------------------
// LayerPanel implementation
// -------------------------------------------------------------------------

LayerPanel::LayerPanel(QWidget *parent)
    : QWidget(parent)
{
    m_group = new QGroupBox("Layers");
    auto *vlay = new QVBoxLayout(m_group);
    vlay->setContentsMargins(6, 6, 6, 6);
    vlay->setSpacing(4);

    m_list = new QListWidget();
    m_list->setMaximumHeight(110);
    m_list->setItemDelegate(new LayerItemDelegate(m_list));
    vlay->addWidget(m_list);

    auto *btnRow = new QHBoxLayout();
    m_addBtn = new QPushButton("Add");
    m_addBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_deleteBtn = new QPushButton("Delete");
    m_deleteBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_deleteBtn->setEnabled(false);
    m_lockBtn = new QPushButton("Lock");
    m_lockBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_lockBtn->setEnabled(false);
    m_moveBtn = new QPushButton("Move Sel.");
    m_moveBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_moveBtn->setToolTip("Move selected entity to this layer");
    m_moveBtn->setEnabled(false);
    btnRow->addWidget(m_addBtn, 1);
    btnRow->addWidget(m_deleteBtn, 1);
    btnRow->addWidget(m_lockBtn, 1);
    btnRow->addWidget(m_moveBtn, 1);
    vlay->addLayout(btnRow);

    // Add group to widget
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(m_group);

    // Connect signals
    connect(m_addBtn, &QPushButton::clicked, this, &LayerPanel::addLayerRequested);
    connect(m_deleteBtn, &QPushButton::clicked, this, [this]() {
        emit deleteLayerRequested(m_list->currentRow());
    });
    connect(m_lockBtn, &QPushButton::clicked, this, [this]() {
        emit lockLayerRequested(m_list->currentRow());
    });
    connect(m_moveBtn, &QPushButton::clicked, this, [this]() {
        emit moveSelectionToLayerRequested(m_list->currentRow());
    });
    connect(m_list, &QListWidget::currentRowChanged, this, &LayerPanel::onCurrentRowChanged);
    connect(m_list, &QListWidget::itemChanged, this, &LayerPanel::onItemChanged);
}

void LayerPanel::refreshFromDocument()
{
    // This will be called by parent with actual document data
    // Placeholder - parent should populate m_list directly
}

void LayerPanel::updateButtonStates(bool hasSelection, int currentLayerIndex, bool isLocked)
{
    m_deleteBtn->setEnabled(currentLayerIndex >= 0);
    m_lockBtn->setEnabled(currentLayerIndex >= 0);
    m_moveBtn->setEnabled(hasSelection && currentLayerIndex >= 0);
    if (currentLayerIndex >= 0) {
        m_lockBtn->setText(isLocked ? "Unlock" : "Lock");
    }
}

void LayerPanel::onCurrentRowChanged(int row)
{
    emit layerSelectionChanged(row);
}

void LayerPanel::onItemChanged(QListWidgetItem *item)
{
    int row = m_list->row(item);
    bool visible = item->checkState() == Qt::Checked;
    emit layerVisibilityChanged(row, visible);
}