#include "PolylinePage.h"
#include "../../../grbl/GraphicsDesign/PolylineEntity.h"
#include "../../../grbl/GraphicsDesign/Entity.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QPushButton>
#include <QLabel>
#include <QTableWidgetItem>
#include <QSignalBlocker>

PolylinePage::PolylinePage(QWidget *parent)
    : PropertyPage(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_pointCountLabel = new QLabel("Points: 0");
    layout->addWidget(m_pointCountLabel);

    m_table = new QTableWidget(0, 2);
    m_table->setHorizontalHeaderLabels({"X", "Y"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::AnyKeyPressed);
    layout->addWidget(m_table, 1);

    auto *btnRow = new QHBoxLayout();
    m_insertBtn = new QPushButton("Insert After");
    m_deleteBtn = new QPushButton("Delete Point");
    m_deleteBtn->setEnabled(false);
    btnRow->addWidget(m_insertBtn);
    btnRow->addWidget(m_deleteBtn);
    layout->addLayout(btnRow);

    // Emit propertyChanged when table content changes
    connect(m_table, &QTableWidget::cellChanged, this, &PolylinePage::propertyChanged);
    connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this]() {
        m_deleteBtn->setEnabled(m_table->currentRow() >= 0);
    });
    connect(m_insertBtn, &QPushButton::clicked, this, &PolylinePage::insertAfterRequested);
    connect(m_deleteBtn, &QPushButton::clicked, this, &PolylinePage::deletePointRequested);
}

void PolylinePage::bind(const Entity *entity)
{
    if (!entity || entity->kind() != Entity::Kind::Polygon) return;

    const auto *polyline = static_cast<const PolylineEntity*>(entity);

    m_updating = true;
    QSignalBlocker b(m_table);

    const auto &pts = polyline->points();
    m_table->setRowCount(pts.size());
    for (int i = 0; i < pts.size(); ++i) {
        auto *xItem = new QTableWidgetItem(QString::number(pts[i].x(), 'f', 3));
        auto *yItem = new QTableWidgetItem(QString::number(pts[i].y(), 'f', 3));
        m_table->setItem(i, 0, xItem);
        m_table->setItem(i, 1, yItem);
    }
    m_pointCountLabel->setText(QString("Points: %1").arg(pts.size()));

    m_updating = false;
}

std::unique_ptr<Entity> PolylinePage::apply() const
{
    QList<QPointF> points;
    int rows = m_table->rowCount();
    for (int i = 0; i < rows; ++i) {
        QTableWidgetItem *xItem = m_table->item(i, 0);
        QTableWidgetItem *yItem = m_table->item(i, 1);
        if (!xItem || !yItem) continue;
        bool xOk = false, yOk = false;
        double x = xItem->text().toDouble(&xOk);
        double y = yItem->text().toDouble(&yOk);
        if (xOk && yOk)
            points.append(QPointF(x, y));
    }

    if (points.size() < 2) return nullptr;
    return std::make_unique<PolylineEntity>(points);
}

int PolylinePage::currentRow() const
{
    return m_table->currentRow();
}

int PolylinePage::entityType() const
{
    return static_cast<int>(Entity::Kind::Polygon);
}