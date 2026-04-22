#include "GraphicsDesignPanel.h"
#include "../DesignCanvas.h"
#include "../LineNumberPlainTextEdit.h"
#include "../../grbl/GraphicsDesign/Document.h"
#include "../../grbl/GraphicsDesign/Entity.h"
#include "../../grbl/GraphicsDesign/LineEntity.h"
#include "../../grbl/GraphicsDesign/RectangleEntity.h"
#include "../../grbl/GraphicsDesign/CircleEntity.h"
#include "../../grbl/GraphicsDesign/ArcEntity.h"
#include "../../grbl/GraphicsDesign/TextEntity.h"
#include "../../grbl/GraphicsDesign/CommandStack.h"
#include "../../grbl/GraphicsDesign/VectorFontCatalog.h"
#include "../../grbl/ImageVectorizer.h"
#include "../../grbl/GraphicsDesign/PolylineEntity.h"
#include "../../grbl/GraphicsDesign/LinearDimensionEntity.h"
#include "../../grbl/GraphicsDesign/AngularDimensionEntity.h"
#include "../../grbl/GraphicsDesign/RadiusDimensionEntity.h"
#include "../../grbl/DxfParser.h"

#include <cmath>

#include <QComboBox>
#include <QFontComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QStackedWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>
#include <QDialog>
#include <QJsonDocument>
#include <QSignalBlocker>
#include <QFontDatabase>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QApplication>
#include <QCursor>
#include <QColorDialog>

// ---------------------------------------------------------------------------
// LayerListDelegate — renders an eye icon in the decoration column to toggle
// layer visibility, replacing the built-in checkbox rendering.
// ---------------------------------------------------------------------------
static const int kEyeColW   = 22;   // pixels reserved on the left for the eye icon
static const int kLockColW  = 22;   // pixels for the lock icon next to eye
static const int kColorSwW  = 16;   // pixels for the color swatch
static const int kIconsW    = kEyeColW + kLockColW;  // total icon cols before text
static const int kSwatchOff = kIconsW + 2;           // swatch left edge offset

class LayerListDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *p, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        // Draw default selection background
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        opt.decorationSize = QSize(0, 0);   // hide normal decoration

        // Temporarily remove checkbox flag from display to avoid default checkbox
        opt.features &= ~QStyleOptionViewItem::HasCheckIndicator;

        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &opt, p);

        // Draw eye icon text on the left
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

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override
    {
        QSize s = QStyledItemDelegate::sizeHint(option, index);
        return QSize(s.width(), std::max(s.height(), 22));
    }
};


GraphicsDesignPanel::GraphicsDesignPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("graphicsDesignPanel");
    setupUi();
    setupConnections();
    refreshLayerList();
    updatePreview();
}

void GraphicsDesignPanel::setWorktableSize(const QSizeF &size)
{
    m_canvas->setWorktableSize(size);
}

void GraphicsDesignPanel::setupUi()
{
    auto *outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    // ---- Left: Vertical toolbar + Canvas ----
    auto *leftContainer = new QWidget();
    auto *leftLayout = new QHBoxLayout(leftContainer);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);

    // Vertical tool toolbar
    m_toolBar = createToolToolBar();
    leftLayout->addWidget(m_toolBar);

    m_canvas = new DesignCanvas();
    m_canvas->setMinimumWidth(300);
    m_canvas->setMinimumHeight(200);
    leftLayout->addWidget(m_canvas, 1);  // stretch=1 so canvas takes remaining space

    splitter->addWidget(leftContainer);

    // ---- Right: Operations panel ----
    auto *rightPanel = new QWidget();
    rightPanel->setMinimumWidth(260);
    rightPanel->setMaximumWidth(400);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(10, 10, 10, 10);
    rightLayout->setSpacing(8);

    // Controls bar (snap, grid, view controls)
    auto *ctrlBar = new QHBoxLayout();
    ctrlBar->setSpacing(6);

    m_snapBtn = new QToolButton();
    m_snapBtn->setText("Snap");
    m_snapBtn->setCheckable(true);
    m_snapBtn->setChecked(true);
    m_snapBtn->setToolTip("Toggle grid/entity snap (S)");
    m_snapBtn->setMinimumWidth(50);
    ctrlBar->addWidget(m_snapBtn);

    // Grid size spinbox
    ctrlBar->addWidget(new QLabel("Grid:"));
    m_gridSizeSpin = new QDoubleSpinBox();
    m_gridSizeSpin->setRange(0.1, 100.0);
    m_gridSizeSpin->setValue(10.0);
    m_gridSizeSpin->setSingleStep(1.0);
    m_gridSizeSpin->setDecimals(1);
    m_gridSizeSpin->setSuffix(" mm");
    m_gridSizeSpin->setToolTip("Grid size in mm");
    m_gridSizeSpin->setMaximumWidth(90);
    ctrlBar->addWidget(m_gridSizeSpin);

    // Fillet radius (shown only when Fillet tool active)
    m_filletRadiusLabel = new QLabel("Radius:");
    ctrlBar->addWidget(m_filletRadiusLabel);
    m_filletRadiusSpin = new QDoubleSpinBox();
    m_filletRadiusSpin->setRange(0.1, 500.0);
    m_filletRadiusSpin->setValue(5.0);
    m_filletRadiusSpin->setSingleStep(1.0);
    m_filletRadiusSpin->setDecimals(2);
    m_filletRadiusSpin->setSuffix(" mm");
    m_filletRadiusSpin->setToolTip("Fillet radius in mm");
    m_filletRadiusSpin->setMaximumWidth(100);
    ctrlBar->addWidget(m_filletRadiusSpin);
    m_filletRadiusLabel->setVisible(false);
    m_filletRadiusSpin->setVisible(false);

    // Chamfer distance (shown only when Chamfer tool active)
    m_chamferDistLabel = new QLabel("Dist:");
    ctrlBar->addWidget(m_chamferDistLabel);
    m_chamferDistSpin = new QDoubleSpinBox();
    m_chamferDistSpin->setRange(0.1, 500.0);
    m_chamferDistSpin->setValue(5.0);
    m_chamferDistSpin->setSingleStep(1.0);
    m_chamferDistSpin->setDecimals(2);
    m_chamferDistSpin->setSuffix(" mm");
    m_chamferDistSpin->setToolTip("Chamfer distance in mm");
    m_chamferDistSpin->setMaximumWidth(100);
    ctrlBar->addWidget(m_chamferDistSpin);
    m_chamferDistLabel->setVisible(false);
    m_chamferDistSpin->setVisible(false);

    // Offset distance (shown only when Offset tool active)
    m_offsetDistLabel = new QLabel("Dist:");
    ctrlBar->addWidget(m_offsetDistLabel);
    m_offsetDistSpin = new QDoubleSpinBox();
    m_offsetDistSpin->setRange(0.001, 500.0);
    m_offsetDistSpin->setValue(5.0);
    m_offsetDistSpin->setSingleStep(1.0);
    m_offsetDistSpin->setDecimals(3);
    m_offsetDistSpin->setSuffix(" mm");
    m_offsetDistSpin->setToolTip("Offset distance in mm");
    m_offsetDistSpin->setMaximumWidth(100);
    ctrlBar->addWidget(m_offsetDistSpin);
    m_offsetDistLabel->setVisible(false);
    m_offsetDistSpin->setVisible(false);

    auto *gcodeOverlayBtn = new QToolButton();
    gcodeOverlayBtn->setText("Path");
    gcodeOverlayBtn->setCheckable(true);
    gcodeOverlayBtn->setChecked(false);
    gcodeOverlayBtn->setToolTip("Show G-code toolpath overlay");
    gcodeOverlayBtn->setMinimumWidth(44);
    connect(gcodeOverlayBtn, &QToolButton::toggled, m_canvas, &DesignCanvas::setGcodeOverlayVisible);
    ctrlBar->addWidget(gcodeOverlayBtn);

    auto *fitBtn = new QToolButton();
    fitBtn->setText("Fit");
    fitBtn->setToolTip("Auto-fit worktable to view");
    fitBtn->setMinimumWidth(40);
    connect(fitBtn, &QToolButton::clicked, m_canvas, &DesignCanvas::autoFit);
    ctrlBar->addWidget(fitBtn);

    auto *fitSelBtn = new QToolButton();
    fitSelBtn->setText("Fit Sel");
    fitSelBtn->setToolTip("Fit view to selected entity");
    fitSelBtn->setMinimumWidth(55);
    connect(fitSelBtn, &QToolButton::clicked, m_canvas, &DesignCanvas::fitToSelection);
    ctrlBar->addWidget(fitSelBtn);

    auto *sepLabel = new QLabel("|");
    sepLabel->setStyleSheet("color: #555; margin: 0 2px;");
    ctrlBar->addWidget(sepLabel);

    auto *exportToolBtn = new QToolButton();
    exportToolBtn->setText("Export...");
    exportToolBtn->setToolTip("Export G-code to file (Ctrl+E)");
    exportToolBtn->setMinimumWidth(60);
    exportToolBtn->setShortcut(QKeySequence("Ctrl+E"));
    connect(exportToolBtn, &QToolButton::clicked, this, &GraphicsDesignPanel::onExportGcodeClicked);
    ctrlBar->addWidget(exportToolBtn);

    ctrlBar->addStretch();
    rightLayout->addLayout(ctrlBar);

    // Action buttons row
    auto *actionRow = new QHBoxLayout();
    actionRow->setSpacing(6);
    auto *undoBtn = new QPushButton("Undo");
    undoBtn->setObjectName("undoBtn");
    undoBtn->setShortcut(QKeySequence::Undo);
    actionRow->addWidget(undoBtn);

    auto *redoBtn = new QPushButton("Redo");
    redoBtn->setObjectName("redoBtn");
    redoBtn->setShortcut(QKeySequence::Redo);
    actionRow->addWidget(redoBtn);

    m_deleteBtn = new QPushButton("Delete");
    m_deleteBtn->setEnabled(false);
    actionRow->addWidget(m_deleteBtn);

    m_clearBtn = new QPushButton("Clear");
    actionRow->addWidget(m_clearBtn);
    rightLayout->addLayout(actionRow);

    m_propertyStack = new QStackedWidget();
    rightLayout->addWidget(m_propertyStack);

    buildEmptyPage();
    buildLinePage();
    buildRectPage();
    buildCirclePage();
    buildArcPage();
    buildTextPage();
    buildPolylinePage();
    buildDimensionPage();
    buildAngularDimensionPage();
    buildRadiusDimensionPage();
    buildTransformSection();
    rightLayout->addWidget(m_transformGroup);
    rightLayout->addWidget(new QLabel("Entities:"));
    m_entityList = new QListWidget();
    m_entityList->setMaximumHeight(120);
    m_entityList->setSelectionMode(QAbstractItemView::SingleSelection);
    rightLayout->addWidget(m_entityList);

    buildLayerSection();
    rightLayout->addWidget(m_layerGroup);

    // Options grid
    auto *optionsGrid = new QGridLayout();
    optionsGrid->setSpacing(6);
    optionsGrid->setColumnStretch(0, 0);
    optionsGrid->setColumnStretch(1, 1);

    int row = 0;
    auto *optionsToolLabel = new QLabel("Tool Mode:");
    optionsToolLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    optionsGrid->addWidget(optionsToolLabel, row, 0);
    m_toolModeCombo = new QComboBox();
    m_toolModeCombo->addItem("Pen Plotter");
    m_toolModeCombo->addItem("Laser");
    optionsGrid->addWidget(m_toolModeCombo, row, 1);

    row++;
    auto *feedLabel = new QLabel("Feed Speed:");
    feedLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    optionsGrid->addWidget(feedLabel, row, 0);
    m_feedSpeedSpinBox = new QDoubleSpinBox();
    m_feedSpeedSpinBox->setRange(0, 10000);
    m_feedSpeedSpinBox->setValue(2000);
    optionsGrid->addWidget(m_feedSpeedSpinBox, row, 1);

    row++;
    auto *plungeLabel = new QLabel("Plunge Speed:");
    plungeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    optionsGrid->addWidget(plungeLabel, row, 0);
    m_plungeSpeedSpinBox = new QDoubleSpinBox();
    m_plungeSpeedSpinBox->setRange(0, 10000);
    m_plungeSpeedSpinBox->setValue(1500);
    optionsGrid->addWidget(m_plungeSpeedSpinBox, row, 1);

    row++;
    auto *safeZLabel = new QLabel("Safe Z:");
    safeZLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    optionsGrid->addWidget(safeZLabel, row, 0);
    m_fastHeightSpinBox = new QDoubleSpinBox();
    m_fastHeightSpinBox->setRange(-100, 100);
    m_fastHeightSpinBox->setValue(0);
    optionsGrid->addWidget(m_fastHeightSpinBox, row, 1);

    row++;
    auto *workZLabel = new QLabel("Work Z:");
    workZLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    optionsGrid->addWidget(workZLabel, row, 0);
    m_workHeightSpinBox = new QDoubleSpinBox();
    m_workHeightSpinBox->setRange(-100, 100);
    m_workHeightSpinBox->setValue(-3);
    optionsGrid->addWidget(m_workHeightSpinBox, row, 1);

    rightLayout->addLayout(optionsGrid);

    // G-code preview
    rightLayout->addWidget(new QLabel("G-code Preview:"));
    m_gcodePreview = new LineNumberPlainTextEdit();
    m_gcodePreview->setReadOnly(true);
    m_gcodePreview->setFont(QFont("Consolas", 10));
    rightLayout->addWidget(m_gcodePreview, 1);

    // Status line
    m_lineCountLabel = new QLabel("0 entities, 0 G-code lines");
    m_lineCountLabel->setAlignment(Qt::AlignRight);
    rightLayout->addWidget(m_lineCountLabel);

    m_statusLabel = new QLabel();
    rightLayout->addWidget(m_statusLabel);

    // Bottom buttons (two rows)
    auto *buttonBar1 = new QHBoxLayout();
    buttonBar1->setSpacing(4);

    m_saveBtn = new QPushButton(QIcon(":/icons/icons/save.svg"), "Save");
    m_saveBtn->setObjectName("actionBtn");
    buttonBar1->addWidget(m_saveBtn);

    m_loadBtn = new QPushButton(QIcon(":/icons/icons/load.svg"), "Load");
    m_loadBtn->setObjectName("actionBtn");
    buttonBar1->addWidget(m_loadBtn);

    m_exportBtn = new QPushButton(QIcon(":/icons/icons/export.svg"), "Export");
    m_exportBtn->setObjectName("actionBtn");
    buttonBar1->addWidget(m_exportBtn);

    rightLayout->addLayout(buttonBar1);

    auto *buttonBar2 = new QHBoxLayout();
    buttonBar2->setSpacing(4);

    m_generateBtn = new QPushButton(QIcon(":/icons/icons/generate.svg"), "Generate");
    m_generateBtn->setObjectName("startBtn");
    buttonBar2->addWidget(m_generateBtn);

    auto *importImgBtn = new QPushButton(QIcon(":/icons/icons/import_image.svg"), "Image");
    importImgBtn->setObjectName("importBtn");
    connect(importImgBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::onImportImageClicked);
    buttonBar2->addWidget(importImgBtn);

    auto *importDxfBtn = new QPushButton(QIcon(":/icons/icons/import_dxf.svg"), "DXF");
    importDxfBtn->setObjectName("importBtn");
    connect(importDxfBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::onImportDxfClicked);
    buttonBar2->addWidget(importDxfBtn);

    rightLayout->addLayout(buttonBar2);

    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 3);  // Canvas gets 3x stretch
    splitter->setStretchFactor(1, 1);  // Panel gets 1x stretch

    outerLayout->addWidget(splitter);
}

QWidget* GraphicsDesignPanel::createToolToolBar()
{
    auto *toolbar = new QWidget();
    toolbar->setObjectName("sketchToolBar");
    toolbar->setFixedWidth(44);
    
    auto *layout = new QVBoxLayout(toolbar);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);
    
    // Tool definitions: icon text, tooltip
    struct ToolDef {
        const char *icon;
        const char *tooltip;
        const char *shortcut;
    };
    
    static const ToolDef tools[] = {
        {"↖", "Select entities (Esc)", "Esc"},
        {"∕", "Line tool (L)", "L"},
        {"⌇", "Polyline tool (P)", "P"},
        {"▭", "Rectangle tool (R)", "R"},
        {"○", "Circle tool (C)", "C"},
        {"◠", "Arc tool (A)", "A"},
        {"T", "Text tool (T)", "Ctrl+T"},
        {"↔", "Linear dimension (D)", "D"},
        {"∠", "Angular dimension", "Ctrl+D"},
        {"⦿", "Radius dimension", "Ctrl+R"},
        {"✂", "Trim tool (X)", "X"},
        {"⌁", "Extend tool (E)", "E"},
        {"∟", "Chamfer tool (H)", "H"},
        {"⌒", "Fillet tool (F)", "F"},
        {"∥", "Offset tool (O)", "O"},
    };
    
    for (int i = 0; i < 15; ++i) {
        auto *btn = new QToolButton();
        btn->setText(tools[i].icon);
        btn->setToolTip(QString("%1\n%2").arg(tools[i].tooltip, tools[i].shortcut));
        btn->setCheckable(true);
        btn->setAutoRaise(true);
        btn->setFixedSize(36, 36);
        btn->setStyleSheet(
            "QToolButton {"
            "  background: #ffffff;"
            "  border: 1px solid #d0d0d0;"
            "  border-radius: 4px;"
            "  font-size: 18px;"
            "}"
            "QToolButton:hover {"
            "  background: #e8f0f8;"
            "  border-color: #a0c4e0;"
            "}"
            "QToolButton:checked {"
            "  background: #dce8f4;"
            "  border: 2px solid #5090c0;"
            "  color: #205080;"
            "}"
        );
        
        connect(btn, &QToolButton::clicked, this, [this, i]() {
            onToolButtonClicked(i);
        });
        
        m_toolButtons[i] = btn;
        layout->addWidget(btn);
    }
    
    layout->addStretch();
    
    // Set first button (Select) as checked by default
    m_toolButtons[0]->setChecked(true);
    
    return toolbar;
}

void GraphicsDesignPanel::buildEmptyPage()
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    m_emptyPageLabel = new QLabel("No selection");
    m_emptyPageLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_emptyPageLabel);
    layout->addStretch();
    m_propertyStack->addWidget(page);
}

void GraphicsDesignPanel::buildLinePage()
{
    auto *page = new QWidget();
    auto *layout = new QFormLayout(page);

    m_lineCenterX = new QDoubleSpinBox();
    m_lineCenterX->setRange(-100000, 100000);
    m_lineCenterX->setDecimals(2);
    layout->addRow("Center X:", m_lineCenterX);

    m_lineCenterY = new QDoubleSpinBox();
    m_lineCenterY->setRange(-100000, 100000);
    m_lineCenterY->setDecimals(2);
    layout->addRow("Center Y:", m_lineCenterY);

    m_lineLength = new QDoubleSpinBox();
    m_lineLength->setRange(0, 100000);
    m_lineLength->setDecimals(2);
    layout->addRow("Length:", m_lineLength);

    m_lineAngle = new QDoubleSpinBox();
    m_lineAngle->setRange(-3600, 3600);
    m_lineAngle->setDecimals(2);
    layout->addRow("Angle:", m_lineAngle);

    m_propertyStack->addWidget(page);
}

void GraphicsDesignPanel::buildRectPage()
{
    auto *page = new QWidget();
    auto *layout = new QFormLayout(page);

    m_rectCenterX = new QDoubleSpinBox();
    m_rectCenterX->setRange(-100000, 100000);
    m_rectCenterX->setDecimals(2);
    layout->addRow("Center X:", m_rectCenterX);

    m_rectCenterY = new QDoubleSpinBox();
    m_rectCenterY->setRange(-100000, 100000);
    m_rectCenterY->setDecimals(2);
    layout->addRow("Center Y:", m_rectCenterY);

    m_rectWidth = new QDoubleSpinBox();
    m_rectWidth->setRange(0, 100000);
    m_rectWidth->setDecimals(2);
    layout->addRow("Width:", m_rectWidth);

    m_rectHeight = new QDoubleSpinBox();
    m_rectHeight->setRange(0, 100000);
    m_rectHeight->setDecimals(2);
    layout->addRow("Height:", m_rectHeight);

    m_rectAngle = new QDoubleSpinBox();
    m_rectAngle->setRange(-3600, 3600);
    m_rectAngle->setDecimals(2);
    layout->addRow("Angle:", m_rectAngle);

    m_propertyStack->addWidget(page);
}

void GraphicsDesignPanel::buildCirclePage()
{
    auto *page = new QWidget();
    auto *layout = new QFormLayout(page);

    m_circleCenterX = new QDoubleSpinBox();
    m_circleCenterX->setRange(-100000, 100000);
    m_circleCenterX->setDecimals(2);
    layout->addRow("Center X:", m_circleCenterX);

    m_circleCenterY = new QDoubleSpinBox();
    m_circleCenterY->setRange(-100000, 100000);
    m_circleCenterY->setDecimals(2);
    layout->addRow("Center Y:", m_circleCenterY);

    m_circleRadius = new QDoubleSpinBox();
    m_circleRadius->setRange(0, 100000);
    m_circleRadius->setDecimals(2);
    layout->addRow("Radius:", m_circleRadius);

    m_propertyStack->addWidget(page);
}

void GraphicsDesignPanel::buildArcPage()
{
    auto *page = new QWidget();
    auto *layout = new QFormLayout(page);

    m_arcCenterX = new QDoubleSpinBox();
    m_arcCenterX->setRange(-100000, 100000);
    m_arcCenterX->setDecimals(2);
    layout->addRow("Center X:", m_arcCenterX);

    m_arcCenterY = new QDoubleSpinBox();
    m_arcCenterY->setRange(-100000, 100000);
    m_arcCenterY->setDecimals(2);
    layout->addRow("Center Y:", m_arcCenterY);

    m_arcRadius = new QDoubleSpinBox();
    m_arcRadius->setRange(0, 100000);
    m_arcRadius->setDecimals(2);
    layout->addRow("Radius:", m_arcRadius);

    m_arcStartAngle = new QDoubleSpinBox();
    m_arcStartAngle->setRange(-3600, 3600);
    m_arcStartAngle->setDecimals(2);
    layout->addRow("Start Angle:", m_arcStartAngle);

    m_arcSweepAngle = new QDoubleSpinBox();
    m_arcSweepAngle->setRange(-3600, 3600);
    m_arcSweepAngle->setDecimals(2);
    layout->addRow("Sweep Angle:", m_arcSweepAngle);

    m_propertyStack->addWidget(page);
}

void GraphicsDesignPanel::buildTextPage()
{
    auto *page = new QWidget();
    auto *layout = new QFormLayout(page);

    m_textPosX = new QDoubleSpinBox();
    m_textPosX->setRange(-100000, 100000);
    m_textPosX->setDecimals(2);
    layout->addRow("Pos X:", m_textPosX);

    m_textPosY = new QDoubleSpinBox();
    m_textPosY->setRange(-100000, 100000);
    m_textPosY->setDecimals(2);
    layout->addRow("Pos Y:", m_textPosY);

    m_textEdit = new QPlainTextEdit();
    m_textEdit->setMaximumHeight(80);
    m_textEdit->setTabChangesFocus(true);
    layout->addRow("Text:", m_textEdit);

    m_textFontSize = new QDoubleSpinBox();
    m_textFontSize->setRange(1, 100000);
    m_textFontSize->setDecimals(2);
    layout->addRow("Font Size:", m_textFontSize);

    m_textLineHeight = new QDoubleSpinBox();
    m_textLineHeight->setRange(0.5, 5.0);
    m_textLineHeight->setDecimals(2);
    m_textLineHeight->setSingleStep(0.1);
    m_textLineHeight->setValue(1.5);
    layout->addRow("Line Height:", m_textLineHeight);

    m_textFontModeCombo = new QComboBox();
    m_textFontModeCombo->addItem("Vector Font");
    m_textFontModeCombo->addItem("System Font");
    layout->addRow("Font Mode:", m_textFontModeCombo);

    m_textVectorFontCombo = new QComboBox();
    auto fonts = builtinVectorFonts();
    for (const auto &f : fonts)
        m_textVectorFontCombo->addItem(f.name, f.path);
    for (const auto &shx : findInstalledShxFonts())
        m_textVectorFontCombo->addItem(shx.second, shx.first);
    m_textVectorFontCombo->addItem("Custom file...");

    m_textCustomFontPath = new QLineEdit();
    m_textCustomFontPath->setReadOnly(true);
    m_textCustomFontPath->setPlaceholderText("No file selected");
    m_textBrowseFontBtn = new QPushButton("Browse...");
    auto *customRow = new QHBoxLayout();
    customRow->addWidget(m_textCustomFontPath, 1);
    customRow->addWidget(m_textBrowseFontBtn);

    m_textVectorFontWidget = new QWidget();
    auto *vecLayout = new QVBoxLayout(m_textVectorFontWidget);
    vecLayout->setContentsMargins(0, 0, 0, 0);
    vecLayout->addWidget(m_textVectorFontCombo);
    vecLayout->addLayout(customRow);
    layout->addRow("Vector Font:", m_textVectorFontWidget);

    m_textSystemFontCombo = new QFontComboBox();
    m_textSystemFontWidget = new QWidget();
    auto *sysLayout = new QVBoxLayout(m_textSystemFontWidget);
    sysLayout->setContentsMargins(0, 0, 0, 0);
    sysLayout->addWidget(m_textSystemFontCombo);
    layout->addRow("System Font:", m_textSystemFontWidget);

    m_textSingleStrokeCheck = new QCheckBox("Enable single-stroke conversion");
    m_textSingleStrokeCheck->setToolTip(
        "Convert system font outlines to centerline strokes via thinning.\n"
        "Best for pen-plotter / CNC engraving with system fonts.");
    layout->addRow("Single Stroke:", m_textSingleStrokeCheck);

    // Single-stroke tuning parameters (shown only when checkbox is on)
    m_textSingleStrokeParams = new QWidget();
    auto *ssLayout = new QFormLayout(m_textSingleStrokeParams);
    ssLayout->setContentsMargins(0, 0, 0, 0);

    m_textSsResolution = new QDoubleSpinBox();
    m_textSsResolution->setRange(64, 512);
    m_textSsResolution->setDecimals(0);
    m_textSsResolution->setValue(256);
    m_textSsResolution->setSuffix(" px");
    m_textSsResolution->setToolTip("Render resolution per glyph. Higher = finer detail but slower.");
    ssLayout->addRow("Resolution:", m_textSsResolution);

    m_textSsSmoothing = new QDoubleSpinBox();
    m_textSsSmoothing->setRange(0.5, 10.0);
    m_textSsSmoothing->setDecimals(1);
    m_textSsSmoothing->setValue(1.5);
    m_textSsSmoothing->setSingleStep(0.5);
    m_textSsSmoothing->setToolTip("Path simplification tolerance (RDP epsilon). Higher = fewer points, less detail.");
    ssLayout->addRow("Smoothing:", m_textSsSmoothing);

    m_textSsCleanup = new QDoubleSpinBox();
    m_textSsCleanup->setRange(0, 30);
    m_textSsCleanup->setDecimals(0);
    m_textSsCleanup->setValue(8);
    m_textSsCleanup->setSuffix(" px");
    m_textSsCleanup->setToolTip("Remove skeleton spur branches shorter than this. Higher = cleaner but may lose small details.");
    ssLayout->addRow("Cleanup:", m_textSsCleanup);

    m_textSsMergeGap = new QDoubleSpinBox();
    m_textSsMergeGap->setRange(0, 20);
    m_textSsMergeGap->setDecimals(1);
    m_textSsMergeGap->setValue(4.0);
    m_textSsMergeGap->setSingleStep(1.0);
    m_textSsMergeGap->setSuffix(" px");
    m_textSsMergeGap->setToolTip("Merge chain endpoints closer than this distance. Higher = fewer toolpaths. 0 = off.");
    ssLayout->addRow("Merge Gap:", m_textSsMergeGap);

    m_textSsMinSegLen = new QDoubleSpinBox();
    m_textSsMinSegLen->setRange(0, 30);
    m_textSsMinSegLen->setDecimals(1);
    m_textSsMinSegLen->setValue(3.0);
    m_textSsMinSegLen->setSingleStep(1.0);
    m_textSsMinSegLen->setSuffix(" px");
    m_textSsMinSegLen->setToolTip("Remove segments with total length shorter than this. Eliminates tiny stray marks. 0 = off.");
    ssLayout->addRow("Min Segment:", m_textSsMinSegLen);

    m_textSingleStrokeParams->setVisible(false);
    layout->addRow("", m_textSingleStrokeParams);

    auto updateFontVisibility = [this]() {
        bool isVector = (m_textFontModeCombo->currentIndex() == 0);
        bool isCustom = (m_textVectorFontCombo->currentIndex() == m_textVectorFontCombo->count() - 1);
        m_textVectorFontWidget->setVisible(isVector);
        m_textCustomFontPath->setVisible(isVector && isCustom);
        m_textBrowseFontBtn->setVisible(isVector && isCustom);
        m_textSystemFontWidget->setVisible(!isVector);
        m_textSingleStrokeCheck->setVisible(!isVector);
        m_textSingleStrokeParams->setVisible(!isVector && m_textSingleStrokeCheck->isChecked());
    };

    connect(m_textSingleStrokeCheck, &QCheckBox::toggled, this, [updateFontVisibility]() {
        updateFontVisibility();
    });
    connect(m_textFontModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [updateFontVisibility, this]() { updateFontVisibility(); });
    connect(m_textVectorFontCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [updateFontVisibility, this]() { updateFontVisibility(); });
    updateFontVisibility();

    connect(m_textBrowseFontBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(
            this, "Select Vector Font", QString(),
            "Vector Fonts (*.jhf *.shx *.svg *.hpgl *.glf *.json);;All Files (*)");
        if (!path.isEmpty())
            m_textCustomFontPath->setText(path);
    });

    auto detectCJK = [this]() {
        if (m_updatingUi) return;
        QString txt = m_textEdit->toPlainText();
        if (textContainsCJK(txt)) {
            bool isVector = (m_textFontModeCombo->currentIndex() == 0);
            if (isVector) {
                int idx = m_textVectorFontCombo->currentIndex();
                QString path = m_textVectorFontCombo->itemData(idx).toString();
                if (!isBigfontShx(path)) {
                    m_textFontModeCombo->setCurrentIndex(1);
                    m_textSystemFontCombo->setCurrentFont(QFont("Microsoft YaHei"));
                }
            }
        }
    };
    connect(m_textEdit, &QPlainTextEdit::textChanged, this, detectCJK);

    m_propertyStack->addWidget(page);
}

void GraphicsDesignPanel::buildPolylinePage()
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_polylinePointCountLabel = new QLabel("Points: 0");
    layout->addWidget(m_polylinePointCountLabel);

    m_polylineTable = new QTableWidget(0, 2);
    m_polylineTable->setHorizontalHeaderLabels({"X", "Y"});
    m_polylineTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_polylineTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::AnyKeyPressed);
    layout->addWidget(m_polylineTable, 1);

    auto *btnRow = new QHBoxLayout();
    m_polylineInsertBtn = new QPushButton("Insert After");
    m_polylineDeleteBtn = new QPushButton("Delete Point");
    m_polylineDeleteBtn->setEnabled(false);
    btnRow->addWidget(m_polylineInsertBtn);
    btnRow->addWidget(m_polylineDeleteBtn);
    layout->addLayout(btnRow);

    m_propertyStack->addWidget(page);
}

void GraphicsDesignPanel::buildDimensionPage()
{
    auto *page = new QWidget();
    auto *layout = new QGridLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto makeSpinBox = [](double lo, double hi) {
        auto *sb = new QDoubleSpinBox();
        sb->setRange(lo, hi);
        sb->setDecimals(3);
        return sb;
    };

    layout->addWidget(new QLabel("P1 X:"), 0, 0);
    m_dimP1X = makeSpinBox(-100000, 100000);
    layout->addWidget(m_dimP1X, 0, 1);

    layout->addWidget(new QLabel("P1 Y:"), 1, 0);
    m_dimP1Y = makeSpinBox(-100000, 100000);
    layout->addWidget(m_dimP1Y, 1, 1);

    layout->addWidget(new QLabel("P2 X:"), 2, 0);
    m_dimP2X = makeSpinBox(-100000, 100000);
    layout->addWidget(m_dimP2X, 2, 1);

    layout->addWidget(new QLabel("P2 Y:"), 3, 0);
    m_dimP2Y = makeSpinBox(-100000, 100000);
    layout->addWidget(m_dimP2Y, 3, 1);

    layout->addWidget(new QLabel("Offset:"), 4, 0);
    m_dimOffset = makeSpinBox(-100000, 100000);
    layout->addWidget(m_dimOffset, 4, 1);

    auto *applyBtn = new QPushButton("Apply");
    layout->addWidget(applyBtn, 5, 0, 1, 2);

    connect(applyBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::applyDimensionProperties);

    layout->setRowStretch(6, 1);
    m_propertyStack->addWidget(page);
}

void GraphicsDesignPanel::applyDimensionProperties()
{
    if (m_updatingUi) return;
    const Entity *entity = m_canvas->selectedEntity();
    if (!entity || entity->kind() != Entity::Kind::LinearDimension) return;

    Document *doc = m_canvas->document();
    int idx = doc->indexOf(entity->id());
    if (idx < 0) return;

    auto newDim = std::make_unique<LinearDimensionEntity>(
        QPointF(m_dimP1X->value(), m_dimP1Y->value()),
        QPointF(m_dimP2X->value(), m_dimP2Y->value()),
        m_dimOffset->value()
    );
    applyProperties(idx, std::move(newDim));
}

void GraphicsDesignPanel::buildAngularDimensionPage()
{
    auto *page = new QWidget();
    auto *layout = new QGridLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto makeSpinBox = [](double lo, double hi) {
        auto *sb = new QDoubleSpinBox();
        sb->setRange(lo, hi);
        sb->setDecimals(3);
        return sb;
    };

    layout->addWidget(new QLabel("Vertex X:"), 0, 0);
    m_adimVertexX = makeSpinBox(-100000, 100000);
    layout->addWidget(m_adimVertexX, 0, 1);

    layout->addWidget(new QLabel("Vertex Y:"), 1, 0);
    m_adimVertexY = makeSpinBox(-100000, 100000);
    layout->addWidget(m_adimVertexY, 1, 1);

    layout->addWidget(new QLabel("Ray 1 X:"), 2, 0);
    m_adimP1X = makeSpinBox(-100000, 100000);
    layout->addWidget(m_adimP1X, 2, 1);

    layout->addWidget(new QLabel("Ray 1 Y:"), 3, 0);
    m_adimP1Y = makeSpinBox(-100000, 100000);
    layout->addWidget(m_adimP1Y, 3, 1);

    layout->addWidget(new QLabel("Ray 2 X:"), 4, 0);
    m_adimP2X = makeSpinBox(-100000, 100000);
    layout->addWidget(m_adimP2X, 4, 1);

    layout->addWidget(new QLabel("Ray 2 Y:"), 5, 0);
    m_adimP2Y = makeSpinBox(-100000, 100000);
    layout->addWidget(m_adimP2Y, 5, 1);

    layout->addWidget(new QLabel("Arc Radius:"), 6, 0);
    m_adimRadius = makeSpinBox(1.0, 100000);
    layout->addWidget(m_adimRadius, 6, 1);

    auto *applyBtn = new QPushButton("Apply");
    layout->addWidget(applyBtn, 7, 0, 1, 2);

    connect(applyBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::applyAngularDimensionProperties);

    layout->setRowStretch(8, 1);
    m_propertyStack->addWidget(page);
}

void GraphicsDesignPanel::applyAngularDimensionProperties()
{
    if (m_updatingUi) return;
    const Entity *entity = m_canvas->selectedEntity();
    if (!entity || entity->kind() != Entity::Kind::AngularDimension) return;

    Document *doc = m_canvas->document();
    int idx = doc->indexOf(entity->id());
    if (idx < 0) return;

    auto newE = std::make_unique<AngularDimensionEntity>(
        QPointF(m_adimVertexX->value(), m_adimVertexY->value()),
        QPointF(m_adimP1X->value(),     m_adimP1Y->value()),
        QPointF(m_adimP2X->value(),     m_adimP2Y->value()),
        m_adimRadius->value()
    );
    applyProperties(idx, std::move(newE));
}

void GraphicsDesignPanel::buildRadiusDimensionPage()
{
    auto *page = new QWidget();
    auto *layout = new QGridLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto makeSpinBox = [](double lo, double hi) {
        auto *sb = new QDoubleSpinBox();
        sb->setRange(lo, hi);
        sb->setDecimals(3);
        return sb;
    };

    layout->addWidget(new QLabel("Center X:"), 0, 0);
    m_rdimCX = makeSpinBox(-100000, 100000);
    layout->addWidget(m_rdimCX, 0, 1);

    layout->addWidget(new QLabel("Center Y:"), 1, 0);
    m_rdimCY = makeSpinBox(-100000, 100000);
    layout->addWidget(m_rdimCY, 1, 1);

    layout->addWidget(new QLabel("Radius:"), 2, 0);
    m_rdimRadius = makeSpinBox(0.001, 100000);
    layout->addWidget(m_rdimRadius, 2, 1);

    layout->addWidget(new QLabel("Angle (°):"), 3, 0);
    m_rdimAngle = makeSpinBox(-360, 360);
    layout->addWidget(m_rdimAngle, 3, 1);

    m_rdimDiameter = new QCheckBox("Diameter mode");
    layout->addWidget(m_rdimDiameter, 4, 0, 1, 2);

    auto *applyBtn = new QPushButton("Apply");
    layout->addWidget(applyBtn, 5, 0, 1, 2);

    connect(applyBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::applyRadiusDimensionProperties);

    layout->setRowStretch(6, 1);
    m_propertyStack->addWidget(page);
}

void GraphicsDesignPanel::applyRadiusDimensionProperties()
{
    if (m_updatingUi) return;
    const Entity *entity = m_canvas->selectedEntity();
    if (!entity || entity->kind() != Entity::Kind::RadiusDimension) return;

    Document *doc = m_canvas->document();
    int idx = doc->indexOf(entity->id());
    if (idx < 0) return;

    const auto *rdim = static_cast<const RadiusDimensionEntity*>(entity);
    auto newE = std::make_unique<RadiusDimensionEntity>(
        QPointF(m_rdimCX->value(), m_rdimCY->value()),
        m_rdimRadius->value(),
        m_rdimAngle->value(),
        m_rdimDiameter->isChecked()
    );
    applyProperties(idx, std::move(newE));
}

void GraphicsDesignPanel::buildTransformSection()
{
    m_transformGroup = new QGroupBox("Transform");
    auto *layout = new QGridLayout(m_transformGroup);
    layout->setSpacing(4);
    layout->setContentsMargins(6, 6, 6, 6);

    layout->addWidget(new QLabel("Move By:"), 0, 0);
    m_moveDx = new QDoubleSpinBox();
    m_moveDx->setRange(-100000, 100000);
    m_moveDx->setDecimals(3);
    m_moveDx->setPrefix("dx ");
    layout->addWidget(m_moveDx, 0, 1);

    m_moveDy = new QDoubleSpinBox();
    m_moveDy->setRange(-100000, 100000);
    m_moveDy->setDecimals(3);
    m_moveDy->setPrefix("dy ");
    layout->addWidget(m_moveDy, 0, 2);

    auto *moveBtn = new QPushButton("Apply");
    connect(moveBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::applyMoveBy);
    layout->addWidget(moveBtn, 0, 3);

    layout->addWidget(new QLabel("Rotate:"), 1, 0);
    m_rotateAngle = new QDoubleSpinBox();
    m_rotateAngle->setRange(-36000, 36000);
    m_rotateAngle->setDecimals(2);
    m_rotateAngle->setSuffix(" °");
    layout->addWidget(m_rotateAngle, 1, 1, 1, 2);

    auto *rotateBtn = new QPushButton("Apply");
    connect(rotateBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::applyRotateBy);
    layout->addWidget(rotateBtn, 1, 3);

    layout->addWidget(new QLabel("Mirror:"), 2, 0);
    auto *flipHBtn = new QPushButton("Flip H");
    flipHBtn->setToolTip("Mirror horizontally (about vertical axis)");
    connect(flipHBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::applyFlipH);
    layout->addWidget(flipHBtn, 2, 1);

    auto *flipVBtn = new QPushButton("Flip V");
    flipVBtn->setToolTip("Mirror vertically (about horizontal axis)");
    connect(flipVBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::applyFlipV);
    layout->addWidget(flipVBtn, 2, 2);

    // Row 3: Rectangular array
    layout->addWidget(new QLabel("Array:"), 3, 0);
    m_arrayRows = new QSpinBox();
    m_arrayRows->setRange(1, 100);
    m_arrayRows->setValue(2);
    m_arrayRows->setToolTip("Number of rows");
    m_arrayRows->setPrefix("R:");
    layout->addWidget(m_arrayRows, 3, 1);
    m_arrayCols = new QSpinBox();
    m_arrayCols->setRange(1, 100);
    m_arrayCols->setValue(2);
    m_arrayCols->setToolTip("Number of columns");
    m_arrayCols->setPrefix("C:");
    layout->addWidget(m_arrayCols, 3, 2);
    auto *arrayBtn = new QPushButton("Apply");
    connect(arrayBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::applyRectArray);
    layout->addWidget(arrayBtn, 3, 3);

    // Row 4: Array spacing
    layout->addWidget(new QLabel("Spacing:"), 4, 0);
    m_arraySpacingX = new QDoubleSpinBox();
    m_arraySpacingX->setRange(-10000, 10000);
    m_arraySpacingX->setValue(20.0);
    m_arraySpacingX->setSuffix(" X");
    m_arraySpacingX->setDecimals(2);
    m_arraySpacingX->setToolTip("Column spacing (X) in mm");
    layout->addWidget(m_arraySpacingX, 4, 1);
    m_arraySpacingY = new QDoubleSpinBox();
    m_arraySpacingY->setRange(-10000, 10000);
    m_arraySpacingY->setValue(20.0);
    m_arraySpacingY->setSuffix(" Y");
    m_arraySpacingY->setDecimals(2);
    m_arraySpacingY->setToolTip("Row spacing (Y) in mm");
    layout->addWidget(m_arraySpacingY, 4, 2);

    // Row 5: Scale
    layout->addWidget(new QLabel("Scale:"), 5, 0);
    m_scaleFactor = new QDoubleSpinBox();
    m_scaleFactor->setRange(0.001, 1000.0);
    m_scaleFactor->setValue(1.0);
    m_scaleFactor->setDecimals(4);
    m_scaleFactor->setSingleStep(0.1);
    m_scaleFactor->setToolTip("Scale factor (1.0 = no change)");
    layout->addWidget(m_scaleFactor, 5, 1, 1, 2);
    auto *scaleBtn = new QPushButton("Apply");
    connect(scaleBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::applyScaleBy);
    layout->addWidget(scaleBtn, 5, 3);

    // Row 6: Polar array
    layout->addWidget(new QLabel("Polar:"), 6, 0);
    m_polarCount = new QSpinBox();
    m_polarCount->setRange(2, 360);
    m_polarCount->setValue(6);
    m_polarCount->setToolTip("Number of copies (including original)");
    m_polarCount->setPrefix("N:");
    layout->addWidget(m_polarCount, 6, 1);
    m_polarSpan = new QDoubleSpinBox();
    m_polarSpan->setRange(0.1, 360.0);
    m_polarSpan->setValue(360.0);
    m_polarSpan->setSuffix("°");
    m_polarSpan->setDecimals(1);
    m_polarSpan->setToolTip("Total arc span for the array (360 = full circle)");
    layout->addWidget(m_polarSpan, 6, 2);
    auto *polarBtn = new QPushButton("Apply");
    connect(polarBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::applyPolarArray);
    layout->addWidget(polarBtn, 6, 3);

    // Row 7: Polar center
    layout->addWidget(new QLabel("Center:"), 7, 0);
    m_polarCenterX = new QDoubleSpinBox();
    m_polarCenterX->setRange(-100000, 100000);
    m_polarCenterX->setValue(0.0);
    m_polarCenterX->setDecimals(2);
    m_polarCenterX->setPrefix("X:");
    m_polarCenterX->setToolTip("Polar array center X (0 = selection center)");
    layout->addWidget(m_polarCenterX, 7, 1);
    m_polarCenterY = new QDoubleSpinBox();
    m_polarCenterY->setRange(-100000, 100000);
    m_polarCenterY->setValue(0.0);
    m_polarCenterY->setDecimals(2);
    m_polarCenterY->setPrefix("Y:");
    m_polarCenterY->setToolTip("Polar array center Y (0 = selection center)");
    layout->addWidget(m_polarCenterY, 7, 2);

    m_transformGroup->setVisible(false);
}

void GraphicsDesignPanel::buildLayerSection()
{
    m_layerGroup = new QGroupBox("Layers");
    auto *vlay = new QVBoxLayout(m_layerGroup);
    vlay->setContentsMargins(6, 6, 6, 6);
    vlay->setSpacing(4);

    m_layerList = new QListWidget();
    m_layerList->setMaximumHeight(110);
    m_layerList->setItemDelegate(new LayerListDelegate(m_layerList));
    // Handle eye-icon clicks to toggle visibility
    connect(m_layerList, &QListWidget::pressed, this, [this](const QModelIndex &idx) {
        QPoint localPos = m_layerList->viewport()->mapFromGlobal(QCursor::pos());
        int x = localPos.x();
        QListWidgetItem *item = m_layerList->item(idx.row());
        if (!item) return;
        if (x < kEyeColW) {
            // Toggle visibility
            item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
        } else if (x < kIconsW) {
            // Toggle lock
            bool nowLocked = !item->data(Qt::UserRole + 1).toBool();
            item->setData(Qt::UserRole + 1, nowLocked);
            // Trigger onLayerItemChanged to propagate to Document
            emit m_layerList->itemChanged(item);
        } else if (x < kSwatchOff + kColorSwW) {
            // Color swatch: open color dialog
            QColor current = item->data(Qt::UserRole + 2).value<QColor>();
            QColor chosen = QColorDialog::getColor(current, this, "Layer Color");
            if (chosen.isValid()) {
                item->setData(Qt::UserRole + 2, chosen);
                // Sync to document
                int row = idx.row();
                Document *doc = m_canvas->document();
                if (row >= 0 && row < doc->layerCount())
                    doc->setLayerColor(row, chosen);
                m_canvas->update();
            }
        }
    });
    vlay->addWidget(m_layerList);

    auto *btnRow = new QHBoxLayout();
    m_addLayerBtn = new QPushButton("Add");
    m_deleteLayerBtn = new QPushButton("Delete");
    m_deleteLayerBtn->setEnabled(false);
    m_lockLayerBtn = new QPushButton("Lock");
    m_lockLayerBtn->setEnabled(false);
    m_moveToLayerBtn = new QPushButton("Move Sel.");
    m_moveToLayerBtn->setToolTip("Move selected entity to this layer");
    m_moveToLayerBtn->setEnabled(false);
    btnRow->addWidget(m_addLayerBtn);
    btnRow->addWidget(m_deleteLayerBtn);
    btnRow->addWidget(m_lockLayerBtn);
    btnRow->addWidget(m_moveToLayerBtn);
    vlay->addLayout(btnRow);
}

void GraphicsDesignPanel::setupConnections()
{
    // Sync toolbar buttons when canvas changes tool internally (e.g. auto-select after draw)
    connect(m_canvas, &DesignCanvas::toolChanged, this, &GraphicsDesignPanel::onCanvasToolChanged);

    // Snap toggle (bidirectional: button ↔ canvas)
    connect(m_snapBtn, &QToolButton::toggled, m_canvas, &DesignCanvas::setSnapEnabled);
    connect(m_canvas, &DesignCanvas::snapEnabledChanged, m_snapBtn, &QToolButton::setChecked);
    connect(m_gridSizeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            m_canvas, &DesignCanvas::setGridSize);
    connect(m_canvas, &DesignCanvas::gridSizeChanged, m_gridSizeSpin, &QDoubleSpinBox::setValue);
    connect(m_filletRadiusSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            m_canvas, &DesignCanvas::setFilletRadius);
    connect(m_chamferDistSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            m_canvas, &DesignCanvas::setChamferDistance);
    connect(m_offsetDistSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            m_canvas, &DesignCanvas::setOffsetDistance);
    connect(m_canvas, &DesignCanvas::exportGcodeRequested, this, &GraphicsDesignPanel::onExportGcodeClicked);

    // Undo/Redo buttons
    auto *undoBtn = findChild<QPushButton*>("undoBtn");
    auto *redoBtn = findChild<QPushButton*>("redoBtn");
    if (undoBtn) connect(undoBtn, &QPushButton::clicked, m_canvas, &DesignCanvas::undo);
    if (redoBtn) connect(redoBtn, &QPushButton::clicked, m_canvas, &DesignCanvas::redo);

    // Document changes
    connect(m_canvas, &DesignCanvas::documentStateChanged, this, &GraphicsDesignPanel::updatePreview);
    connect(m_canvas, &DesignCanvas::documentStateChanged, this, &GraphicsDesignPanel::refreshEntityList);
    connect(m_canvas, &DesignCanvas::selectedEntityChanged, this, &GraphicsDesignPanel::onSelectionChanged);

    // Entity list checkbox
    connect(m_entityList, &QListWidget::itemChanged, this, &GraphicsDesignPanel::onEntityItemChanged);

    // Click entity list item to select on canvas
    connect(m_entityList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (m_updatingUi) return;
        Document *doc = m_canvas->document();
        if (row >= 0 && row < doc->entityCount()) {
            Entity *e = doc->entityAt(row);
            doc->setSelected(e->id(), true);
            m_canvas->update();
        }
    });

    // Buttons
    connect(m_deleteBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::onDeleteSelectedClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::onClearAllClicked);
    connect(m_saveBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::onSaveDesignClicked);
    connect(m_loadBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::onLoadDesignClicked);
    connect(m_exportBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::onExportGcodeClicked);
    connect(m_generateBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::onGenerateClicked);

    // Auto-update G-code preview when options change
    connect(m_toolModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GraphicsDesignPanel::updatePreview);
    connect(m_feedSpeedSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::updatePreview);
    connect(m_plungeSpeedSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::updatePreview);
    connect(m_fastHeightSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::updatePreview);
    connect(m_workHeightSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::updatePreview);

    connect(m_lineCenterX, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyLineProperties);
    connect(m_lineCenterY, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyLineProperties);
    connect(m_lineLength, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyLineProperties);
    connect(m_lineAngle, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyLineProperties);

    connect(m_rectCenterX, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyRectProperties);
    connect(m_rectCenterY, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyRectProperties);
    connect(m_rectWidth, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyRectProperties);
    connect(m_rectHeight, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyRectProperties);
    connect(m_rectAngle, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyRectProperties);

    connect(m_circleCenterX, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyCircleProperties);
    connect(m_circleCenterY, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyCircleProperties);
    connect(m_circleRadius, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyCircleProperties);

    connect(m_arcCenterX, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyArcProperties);
    connect(m_arcCenterY, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyArcProperties);
    connect(m_arcRadius, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyArcProperties);
    connect(m_arcStartAngle, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyArcProperties);
    connect(m_arcSweepAngle, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyArcProperties);

    connect(m_textPosX, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyTextProperties);
    connect(m_textPosY, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyTextProperties);
    connect(m_textEdit, &QPlainTextEdit::textChanged,
            this, &GraphicsDesignPanel::applyTextProperties);
    connect(m_textFontSize, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyTextProperties);
    connect(m_textLineHeight, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyTextProperties);
    connect(m_textFontModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GraphicsDesignPanel::applyTextProperties);
    connect(m_textVectorFontCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GraphicsDesignPanel::applyTextProperties);
    connect(m_textSystemFontCombo, &QFontComboBox::currentFontChanged,
            this, &GraphicsDesignPanel::applyTextProperties);
    connect(m_textSingleStrokeCheck, &QCheckBox::toggled,
            this, &GraphicsDesignPanel::applyTextProperties);
    connect(m_textSsResolution, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyTextProperties);
    connect(m_textSsSmoothing, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyTextProperties);
    connect(m_textSsCleanup, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyTextProperties);
    connect(m_textSsMergeGap, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyTextProperties);
    connect(m_textSsMinSegLen, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GraphicsDesignPanel::applyTextProperties);

    connect(m_polylineTable, &QTableWidget::cellChanged,
            this, &GraphicsDesignPanel::applyPolylineProperties);

    connect(m_polylineInsertBtn, &QPushButton::clicked,
            this, &GraphicsDesignPanel::polylineInsertVertex);
    connect(m_polylineDeleteBtn, &QPushButton::clicked,
            this, &GraphicsDesignPanel::polylineDeleteVertex);

    connect(m_polylineTable, &QTableWidget::itemSelectionChanged, this, [this]() {
        bool hasSelection = !m_polylineTable->selectedItems().isEmpty();
        bool canDelete = hasSelection && m_polylineTable->rowCount() > 2;
        m_polylineDeleteBtn->setEnabled(canDelete);
    });

    // Layer management
    connect(m_addLayerBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::onAddLayerClicked);
    connect(m_deleteLayerBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::onDeleteLayerClicked);
    connect(m_lockLayerBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::onLockLayerClicked);
    connect(m_moveToLayerBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::onMoveEntityToLayerClicked);
    connect(m_layerList, &QListWidget::itemChanged, this, &GraphicsDesignPanel::onLayerItemChanged);
    connect(m_layerList, &QListWidget::currentRowChanged, this, &GraphicsDesignPanel::onLayerCurrentRowChanged);
    connect(m_canvas->document(), &Document::layersChanged, this, &GraphicsDesignPanel::refreshLayerList);
}

void GraphicsDesignPanel::onToolChanged(int index)
{
    DesignCanvas::ToolType tool = static_cast<DesignCanvas::ToolType>(index);
    m_canvas->setTool(tool);

    bool isFillet  = (tool == DesignCanvas::ToolType::Fillet);
    bool isChamfer = (tool == DesignCanvas::ToolType::Chamfer);
    bool isOffset  = (tool == DesignCanvas::ToolType::Offset);
    m_filletRadiusLabel->setVisible(isFillet);
    m_filletRadiusSpin->setVisible(isFillet);
    m_chamferDistLabel->setVisible(isChamfer);
    m_chamferDistSpin->setVisible(isChamfer);
    m_offsetDistLabel->setVisible(isOffset);
    m_offsetDistSpin->setVisible(isOffset);
}

void GraphicsDesignPanel::onToolButtonClicked(int toolIndex)
{
    // Update all button checked states
    for (int i = 0; i < 15; ++i) {
        m_toolButtons[i]->setChecked(i == toolIndex);
    }
    
    // Call the core tool change logic
    onToolChanged(toolIndex);
}

void GraphicsDesignPanel::onCanvasToolChanged()
{
    // Sync toolbar buttons when canvas changes tool internally (e.g. auto-select after drawing)
    int index = static_cast<int>(m_canvas->currentToolType());
    
    // Update button checked state
    for (int i = 0; i < 15; ++i) {
        m_toolButtons[i]->setChecked(i == index);
    }
    
    // Update tool-specific parameter widgets
    DesignCanvas::ToolType tool = static_cast<DesignCanvas::ToolType>(index);
    bool isFillet  = (tool == DesignCanvas::ToolType::Fillet);
    bool isChamfer = (tool == DesignCanvas::ToolType::Chamfer);
    bool isOffset  = (tool == DesignCanvas::ToolType::Offset);
    m_filletRadiusLabel->setVisible(isFillet);
    m_filletRadiusSpin->setVisible(isFillet);
    m_chamferDistLabel->setVisible(isChamfer);
    m_chamferDistSpin->setVisible(isChamfer);
    m_offsetDistLabel->setVisible(isOffset);
    m_offsetDistSpin->setVisible(isOffset);
}

void GraphicsDesignPanel::onSelectionChanged()
{
    const Entity *entity = m_canvas->selectedEntity();

    m_updatingUi = true;

    if (!entity) {
        m_propertyStack->setCurrentIndex(0);
        m_emptyPageLabel->setText("No selection");
        m_deleteBtn->setEnabled(false);
        m_transformGroup->setVisible(false);
        m_updatingUi = false;
        return;
    }

    m_deleteBtn->setEnabled(true);

    switch (entity->kind()) {
    case Entity::Kind::Line: {
        const auto *line = static_cast<const LineEntity*>(entity);
        {
            QSignalBlocker b1(m_lineCenterX);
            QSignalBlocker b2(m_lineCenterY);
            QSignalBlocker b3(m_lineLength);
            QSignalBlocker b4(m_lineAngle);
            m_lineCenterX->setValue(line->center().x());
            m_lineCenterY->setValue(line->center().y());
            m_lineLength->setValue(line->length());
            m_lineAngle->setValue(line->angleDegrees());
        }
        m_propertyStack->setCurrentIndex(1);
        break;
    }
    case Entity::Kind::Polygon: {
        const auto *polyline = static_cast<const PolylineEntity*>(entity);
        {
            QSignalBlocker b(m_polylineTable);
            const auto &pts = polyline->points();
            m_polylineTable->setRowCount(pts.size());
            for (int i = 0; i < pts.size(); ++i) {
                auto *xItem = new QTableWidgetItem(QString::number(pts[i].x(), 'f', 3));
                auto *yItem = new QTableWidgetItem(QString::number(pts[i].y(), 'f', 3));
                m_polylineTable->setItem(i, 0, xItem);
                m_polylineTable->setItem(i, 1, yItem);
            }
            m_polylinePointCountLabel->setText(QString("Points: %1").arg(pts.size()));
        }
        m_propertyStack->setCurrentIndex(6);
        break;
    }
    case Entity::Kind::Rectangle: {
        const auto *rect = static_cast<const RectangleEntity*>(entity);
        {
            QSignalBlocker b1(m_rectCenterX);
            QSignalBlocker b2(m_rectCenterY);
            QSignalBlocker b3(m_rectWidth);
            QSignalBlocker b4(m_rectHeight);
            QSignalBlocker b5(m_rectAngle);
            m_rectCenterX->setValue(rect->center().x());
            m_rectCenterY->setValue(rect->center().y());
            m_rectWidth->setValue(rect->width());
            m_rectHeight->setValue(rect->height());
            m_rectAngle->setValue(rect->angleDegrees());
        }
        m_propertyStack->setCurrentIndex(2);
        break;
    }
    case Entity::Kind::Circle: {
        const auto *circle = static_cast<const CircleEntity*>(entity);
        {
            QSignalBlocker b1(m_circleCenterX);
            QSignalBlocker b2(m_circleCenterY);
            QSignalBlocker b3(m_circleRadius);
            m_circleCenterX->setValue(circle->center().x());
            m_circleCenterY->setValue(circle->center().y());
            m_circleRadius->setValue(circle->radius());
        }
        m_propertyStack->setCurrentIndex(3);
        break;
    }
    case Entity::Kind::Arc: {
        const auto *arc = static_cast<const ArcEntity*>(entity);
        {
            QSignalBlocker b1(m_arcCenterX);
            QSignalBlocker b2(m_arcCenterY);
            QSignalBlocker b3(m_arcRadius);
            QSignalBlocker b4(m_arcStartAngle);
            QSignalBlocker b5(m_arcSweepAngle);
            m_arcCenterX->setValue(arc->center().x());
            m_arcCenterY->setValue(arc->center().y());
            m_arcRadius->setValue(arc->radius());
            m_arcStartAngle->setValue(arc->startAngleDegrees());
            m_arcSweepAngle->setValue(arc->sweepAngleDegrees());
        }
        m_propertyStack->setCurrentIndex(4);
        break;
    }
    case Entity::Kind::Text: {
        const auto *text = static_cast<const TextEntity*>(entity);
        {
            QSignalBlocker b1(m_textPosX);
            QSignalBlocker b2(m_textPosY);
            QSignalBlocker b3(m_textFontSize);
            QSignalBlocker b4(m_textFontModeCombo);
            QSignalBlocker b5(m_textVectorFontCombo);
            QSignalBlocker b6(m_textSystemFontCombo);
            QSignalBlocker b7(m_textLineHeight);
            m_textPosX->setValue(text->position().x());
            m_textPosY->setValue(text->position().y());
            m_textEdit->blockSignals(true);
            m_textEdit->setPlainText(text->text());
            m_textEdit->blockSignals(false);
            m_textFontSize->setValue(text->fontSize());
            m_textLineHeight->setValue(text->lineHeight());

            bool isVector = (text->fontMode() == TextEntity::FontMode::VectorFont);
            m_textFontModeCombo->setCurrentIndex(isVector ? 0 : 1);
            m_textVectorFontWidget->setVisible(isVector);
            m_textSystemFontWidget->setVisible(!isVector);
            m_textSingleStrokeCheck->setVisible(!isVector);
            {
                QSignalBlocker bss(m_textSingleStrokeCheck);
                QSignalBlocker bres(m_textSsResolution);
                QSignalBlocker bsmo(m_textSsSmoothing);
                QSignalBlocker bcln(m_textSsCleanup);
                QSignalBlocker bmrg(m_textSsMergeGap);
                QSignalBlocker bmin(m_textSsMinSegLen);
                m_textSingleStrokeCheck->setChecked(text->singleStroke());
                m_textSsResolution->setValue(text->ssResolution());
                m_textSsSmoothing->setValue(text->ssSmoothing());
                m_textSsCleanup->setValue(text->ssCleanup());
                m_textSsMergeGap->setValue(text->ssMergeGap());
                m_textSsMinSegLen->setValue(text->ssMinSegLen());
                m_textSingleStrokeParams->setVisible(!isVector && text->singleStroke());
            }

            if (isVector) {
                int idx = -1;
                for (int i = 0; i < m_textVectorFontCombo->count() - 1; ++i) {
                    if (m_textVectorFontCombo->itemData(i).toString() == text->vectorFontPath()) {
                        idx = i;
                        break;
                    }
                }
                if (idx >= 0) {
                    m_textVectorFontCombo->setCurrentIndex(idx);
                    m_textCustomFontPath->setVisible(false);
                    m_textBrowseFontBtn->setVisible(false);
                } else {
                    m_textVectorFontCombo->setCurrentIndex(m_textVectorFontCombo->count() - 1);
                    m_textCustomFontPath->setText(text->vectorFontPath());
                    m_textCustomFontPath->setVisible(true);
                    m_textBrowseFontBtn->setVisible(true);
                }
            } else {
                m_textSystemFontCombo->setCurrentFont(QFont(text->fontFamily()));
            }
        }
        m_propertyStack->setCurrentIndex(5);
        break;
    }
    case Entity::Kind::LinearDimension: {
        const auto *dim = static_cast<const LinearDimensionEntity*>(entity);
        {
            QSignalBlocker b1(m_dimP1X); QSignalBlocker b2(m_dimP1Y);
            QSignalBlocker b3(m_dimP2X); QSignalBlocker b4(m_dimP2Y);
            QSignalBlocker b5(m_dimOffset);
            m_dimP1X->setValue(dim->p1().x());
            m_dimP1Y->setValue(dim->p1().y());
            m_dimP2X->setValue(dim->p2().x());
            m_dimP2Y->setValue(dim->p2().y());
            m_dimOffset->setValue(dim->offsetDistance());
        }
        m_propertyStack->setCurrentIndex(7);
        break;
    }
    case Entity::Kind::AngularDimension: {
        const auto *adim = static_cast<const AngularDimensionEntity*>(entity);
        {
            QSignalBlocker b1(m_adimVertexX); QSignalBlocker b2(m_adimVertexY);
            QSignalBlocker b3(m_adimP1X);     QSignalBlocker b4(m_adimP1Y);
            QSignalBlocker b5(m_adimP2X);     QSignalBlocker b6(m_adimP2Y);
            QSignalBlocker b7(m_adimRadius);
            m_adimVertexX->setValue(adim->vertex().x());
            m_adimVertexY->setValue(adim->vertex().y());
            m_adimP1X->setValue(adim->p1().x());
            m_adimP1Y->setValue(adim->p1().y());
            m_adimP2X->setValue(adim->p2().x());
            m_adimP2Y->setValue(adim->p2().y());
            m_adimRadius->setValue(adim->radius());
        }
        m_propertyStack->setCurrentIndex(8);
        break;
    }
    case Entity::Kind::RadiusDimension: {
        const auto *rdim = static_cast<const RadiusDimensionEntity*>(entity);
        {
            QSignalBlocker b1(m_rdimCX);    QSignalBlocker b2(m_rdimCY);
            QSignalBlocker b3(m_rdimRadius); QSignalBlocker b4(m_rdimAngle);
            QSignalBlocker b5(m_rdimDiameter);
            m_rdimCX->setValue(rdim->circleCenter().x());
            m_rdimCY->setValue(rdim->circleCenter().y());
            m_rdimRadius->setValue(rdim->circleRadius());
            m_rdimAngle->setValue(rdim->angleDeg());
            m_rdimDiameter->setChecked(rdim->isDiameter());
        }
        m_propertyStack->setCurrentIndex(9);
        break;
    }
    default:
        m_propertyStack->setCurrentIndex(0);
        m_emptyPageLabel->setText("Unknown entity type");
        break;
    }

    // Show transform section and reset its inputs
    m_transformGroup->setVisible(true);
    m_moveDx->setValue(0.0);
    m_moveDy->setValue(0.0);
    m_rotateAngle->setValue(0.0);

    // Sync entity list selection
    if (entity) {
        Document *doc = m_canvas->document();
        for (int i = 0; i < doc->entityCount(); ++i) {
            if (doc->entityAt(i)->id() == entity->id()) {
                m_entityList->blockSignals(true);
                m_entityList->setCurrentRow(i);
                m_entityList->blockSignals(false);
                break;
            }
        }
    }

    m_updatingUi = false;
}

void GraphicsDesignPanel::refreshEntityList()
{
    m_entityList->blockSignals(true);
    m_entityList->clear();

    Document *doc = m_canvas->document();
    for (int i = 0; i < doc->entityCount(); ++i) {
        Entity *e = doc->entityAt(i);
        auto *item = new QListWidgetItem(e->displayName());
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(e->isEnabled() ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, QVariant::fromValue(e->id()));
        m_entityList->addItem(item);
    }

    m_entityList->blockSignals(false);
}

void GraphicsDesignPanel::onEntityItemChanged(QListWidgetItem *item)
{
    int row = m_entityList->row(item);
    Document *doc = m_canvas->document();
    if (row >= 0 && row < doc->entityCount()) {
        Entity *e = doc->entityAt(row);
        e->setEnabled(item->checkState() == Qt::Checked);
        m_canvas->update();
        updatePreview();
    }
}

void GraphicsDesignPanel::updatePreview()
{
    Document *doc = m_canvas->document();
    m_generatedLines = doc->generateGcode(
        m_feedSpeedSpinBox->value(),
        m_plungeSpeedSpinBox->value(),
        m_fastHeightSpinBox->value(),
        m_workHeightSpinBox->value());

    QString previewText = QStringList(m_generatedLines.mid(0, 100)).join("\n");
    if (m_generatedLines.size() > 100) {
        previewText += "\n... (more lines)";
    }
    m_gcodePreview->setPlainText(previewText);
    int visibleLayers = 0;
    int lockedLayers  = 0;
    for (int i = 0; i < doc->layerCount(); ++i) {
        if (doc->layerAt(i).visible) ++visibleLayers;
        if (doc->layerAt(i).locked)  ++lockedLayers;
    }
    QString layerSummary = QString("%1 layer%2")
        .arg(doc->layerCount())
        .arg(doc->layerCount() == 1 ? "" : "s");
    if (visibleLayers < doc->layerCount())
        layerSummary += QString(" (%1 hidden)").arg(doc->layerCount() - visibleLayers);
    if (lockedLayers > 0)
        layerSummary += QString(" (%1 locked)").arg(lockedLayers);

    m_lineCountLabel->setText(QString("%1 entit%2 · %3 · %4 G-code lines")
                              .arg(doc->entityCount())
                              .arg(doc->entityCount() == 1 ? "y" : "ies")
                              .arg(layerSummary)
                              .arg(m_generatedLines.size()));
}

void GraphicsDesignPanel::onGenerateClicked()
{
    if (m_generatedLines.isEmpty() || m_generatedLines.size() < 3) {
        QMessageBox::warning(this, "No Design",
            "Please create some shapes before generating G-code.");
        return;
    }

    emit gcodeGenerated(m_generatedLines);
    m_statusLabel->setText("G-code loaded to main window.");
}

void GraphicsDesignPanel::onSaveDesignClicked()
{
    QString file = QFileDialog::getSaveFileName(this, "Save Design",
        QString(), "Design Files (*.json)");
    if (!file.isEmpty()) {
        QJsonDocument doc = m_canvas->document()->toJson();
        QFile f(file);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(doc.toJson());
            f.close();
            m_statusLabel->setText("Design saved.");
        } else {
            QMessageBox::warning(this, "Save Failed", "Could not save file.");
        }
    }
}

void GraphicsDesignPanel::onLoadDesignClicked()
{
    QString file = QFileDialog::getOpenFileName(this, "Load Design",
        QString(), "Design Files (*.json)");
    if (!file.isEmpty()) {
        QFile f(file);
        if (f.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            f.close();
            m_canvas->document()->loadFromJson(doc);
            m_statusLabel->setText("Design loaded.");
            updatePreview();
        } else {
            QMessageBox::warning(this, "Load Failed", "Could not load file.");
        }
    }
}

void GraphicsDesignPanel::onDeleteSelectedClicked()
{
    m_canvas->deleteSelectedEntity();
    updatePreview();
}

void GraphicsDesignPanel::onClearAllClicked()
{
    m_canvas->clearDocument();
    updatePreview();
    m_propertyStack->setCurrentIndex(0);
    m_emptyPageLabel->setText("No selection");
}

void GraphicsDesignPanel::onExportGcodeClicked()
{
    QString file = QFileDialog::getSaveFileName(this, "Export G-code",
        QString(), "G-code Files (*.nc *.ngc *.gcode *.txt)");
    if (!file.isEmpty()) {
        QFile f(file);
        if (f.open(QIODevice::WriteOnly)) {
            for (const QString &line : m_generatedLines) {
                f.write(line.toUtf8());
                f.write("\r\n");
            }
            f.close();
            m_statusLabel->setText(QString("Exported %1 lines to %2").arg(m_generatedLines.size()).arg(file));
        } else {
            QMessageBox::warning(this, "Export Failed", "Could not write file.");
        }
    }
}

void GraphicsDesignPanel::onImportImageClicked()
{
    QString file = QFileDialog::getOpenFileName(this, "Import Image",
        QString(), "Image Files (*.png *.jpg *.jpeg *.bmp *.tif *.tiff)");
    if (file.isEmpty()) return;

    ImageVectorizer vectorizer;
    if (!vectorizer.loadImage(file)) {
        QMessageBox::warning(this, "Import Failed", "Could not load image: " + file);
        return;
    }

    // --- Settings dialog with preview ---
    QDialog dlg(this);
    dlg.setWindowTitle("Image Import Settings");
    dlg.resize(800, 600);

    auto *mainLayout = new QHBoxLayout(&dlg);

    // Left side: preview
    auto *previewLabel = new QLabel;
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setMinimumSize(400, 400);
    previewLabel->setStyleSheet("background: #f5f5f5; border: 1px solid #ccc;");
    mainLayout->addWidget(previewLabel, 1);

    // Right side: settings
    auto *settingsWidget = new QWidget;
    auto *form = new QFormLayout(settingsWidget);
    mainLayout->addWidget(settingsWidget);

    // --- Mode selection ---
    auto *modeCombo = new QComboBox;
    modeCombo->addItem("Threshold (binarize)");
    modeCombo->addItem("Edge detection (Canny)");
    form->addRow("Mode:", modeCombo);

    // --- Threshold settings ---
    auto *adaptiveCheck = new QCheckBox("Adaptive threshold");
    adaptiveCheck->setChecked(true);
    form->addRow(adaptiveCheck);

    auto *thresholdSpin = new QSpinBox;
    thresholdSpin->setRange(0, 255);
    thresholdSpin->setValue(128);
    form->addRow("Threshold:", thresholdSpin);

    auto *blockSizeSpin = new QSpinBox;
    blockSizeSpin->setRange(3, 99);
    blockSizeSpin->setSingleStep(2);
    blockSizeSpin->setValue(15);
    form->addRow("Adaptive block size:", blockSizeSpin);

    // --- Canny settings ---
    auto *cannyLowSpin = new QSpinBox;
    cannyLowSpin->setRange(1, 500);
    cannyLowSpin->setValue(50);
    form->addRow("Canny low threshold:", cannyLowSpin);

    auto *cannyHighSpin = new QSpinBox;
    cannyHighSpin->setRange(1, 500);
    cannyHighSpin->setValue(150);
    form->addRow("Canny high threshold:", cannyHighSpin);

    auto *blurSpin = new QSpinBox;
    blurSpin->setRange(1, 31);
    blurSpin->setSingleStep(2);
    blurSpin->setValue(3);
    form->addRow("Gaussian blur:", blurSpin);

    // --- Common settings ---
    auto *epsilonSpin = new QDoubleSpinBox;
    epsilonSpin->setRange(0.05, 20.0);
    epsilonSpin->setSingleStep(0.1);
    epsilonSpin->setValue(0.2);
    epsilonSpin->setDecimals(1);
    form->addRow("Simplify epsilon (px):", epsilonSpin);

    auto *widthSpin = new QDoubleSpinBox;
    widthSpin->setRange(1.0, 1000.0);
    widthSpin->setSingleStep(10.0);
    widthSpin->setValue(100.0);
    widthSpin->setSuffix(" mm");
    form->addRow("Target width:", widthSpin);

    auto *invertCheck = new QCheckBox("Invert image");
    form->addRow(invertCheck);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);

    // --- Update visibility and preview ---
    auto updateVisibility = [&]() {
        bool isThreshold = (modeCombo->currentIndex() == 0);
        adaptiveCheck->setVisible(isThreshold);
        thresholdSpin->setEnabled(isThreshold && !adaptiveCheck->isChecked());
        blockSizeSpin->setEnabled(isThreshold && adaptiveCheck->isChecked());
        // Show/hide threshold vs canny labels
        form->labelForField(thresholdSpin)->setVisible(isThreshold);
        thresholdSpin->setVisible(isThreshold);
        form->labelForField(blockSizeSpin)->setVisible(isThreshold);
        blockSizeSpin->setVisible(isThreshold);
        form->labelForField(cannyLowSpin)->setVisible(!isThreshold);
        cannyLowSpin->setVisible(!isThreshold);
        form->labelForField(cannyHighSpin)->setVisible(!isThreshold);
        cannyHighSpin->setVisible(!isThreshold);
        form->labelForField(blurSpin)->setVisible(!isThreshold);
        blurSpin->setVisible(!isThreshold);
    };

    auto updatePreviewFn = [&]() {
        ImageVectorizer::Settings s;
        s.useEdgeDetection = (modeCombo->currentIndex() == 1);
        s.adaptiveThreshold = adaptiveCheck->isChecked();
        s.threshold = thresholdSpin->value();
        s.adaptiveBlockSize = blockSizeSpin->value();
        s.cannyLow = cannyLowSpin->value();
        s.cannyHigh = cannyHighSpin->value();
        s.gaussianBlur = blurSpin->value();
        s.invertImage = invertCheck->isChecked();
        s.simplifyEpsilon = epsilonSpin->value();
        s.targetWidth = widthSpin->value();

        vectorizer.setSettings(s);
        vectorizer.processForPreview();
        QImage preview = vectorizer.previewImage();
        if (!preview.isNull()) {
            QPixmap pm = QPixmap::fromImage(preview);
            previewLabel->setPixmap(pm.scaled(previewLabel->size(),
                Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    };

    // Connect all controls to update preview
    connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, [&](int) {
        updateVisibility();
        updatePreviewFn();
    });
    connect(adaptiveCheck, &QCheckBox::toggled, &dlg, [&](bool) {
        updateVisibility();
        updatePreviewFn();
    });
    connect(thresholdSpin, QOverload<int>::of(&QSpinBox::valueChanged), &dlg, [&](int) { updatePreviewFn(); });
    connect(blockSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), &dlg, [&](int) { updatePreviewFn(); });
    connect(cannyLowSpin, QOverload<int>::of(&QSpinBox::valueChanged), &dlg, [&](int) { updatePreviewFn(); });
    connect(cannyHighSpin, QOverload<int>::of(&QSpinBox::valueChanged), &dlg, [&](int) { updatePreviewFn(); });
    connect(blurSpin, QOverload<int>::of(&QSpinBox::valueChanged), &dlg, [&](int) { updatePreviewFn(); });
    connect(invertCheck, &QCheckBox::toggled, &dlg, [&](bool) { updatePreviewFn(); });

    // Initial state
    updateVisibility();
    updatePreviewFn();

    if (dlg.exec() != QDialog::Accepted) return;

    // Apply final settings and process
    ImageVectorizer::Settings settings;
    settings.useEdgeDetection = (modeCombo->currentIndex() == 1);
    settings.adaptiveThreshold = adaptiveCheck->isChecked();
    settings.threshold = thresholdSpin->value();
    settings.adaptiveBlockSize = blockSizeSpin->value();
    settings.cannyLow = cannyLowSpin->value();
    settings.cannyHigh = cannyHighSpin->value();
    settings.gaussianBlur = blurSpin->value();
    settings.simplifyEpsilon = epsilonSpin->value();
    settings.targetWidth = widthSpin->value();
    settings.invertImage = invertCheck->isChecked();

    vectorizer.setSettings(settings);
    auto paths = vectorizer.process();
    if (paths.isEmpty()) {
        QMessageBox::information(this, "Import", "No lines detected in image.");
        return;
    }

    // Convert polylines to PolylineEntities (continuous toolpaths)
    Document *doc = m_canvas->document();
    for (const auto& path : paths) {
        if (path.size() >= 2) {
            auto polyEntity = std::make_unique<PolylineEntity>(path);
            doc->addEntity(std::move(polyEntity));
        }
    }

    m_canvas->update();
    updatePreview();
    m_statusLabel->setText(QString("Imported %1 polylines from image").arg(paths.size()));
}

void GraphicsDesignPanel::onImportDxfClicked()
{
    QString file = QFileDialog::getOpenFileName(this, "Import DXF",
        QString(), "DXF Files (*.dxf)");
    if (file.isEmpty()) return;

    DxfParser parser;
    auto entities = parser.parse(file);

    if (entities.empty()) {
        QMessageBox::warning(this, "Import Failed",
            parser.errorMessage().isEmpty() ? "No entities found in DXF file." : parser.errorMessage());
        return;
    }

    Document *doc = m_canvas->document();
    int count = entities.size();
    doc->beginBatch();
    for (auto& entity : entities) {
        doc->addEntity(std::move(entity));
    }
    doc->endBatch();

    m_canvas->update();
    updatePreview();
    m_statusLabel->setText(QString("Imported %1 entities from DXF").arg(count));
}

void GraphicsDesignPanel::applyProperties(int index, std::unique_ptr<Entity> newEntity)
{
    Document *doc = m_canvas->document();
    const Entity *oldEntity = doc->entityAt(index);
    if (!oldEntity) return;

    auto *cmd = new UpdateEntityCommand(index, oldEntity->clone(), std::move(newEntity));
    doc->commandStack()->execute(std::unique_ptr<Command>(cmd));
}

void GraphicsDesignPanel::applyLineProperties()
{
    if (m_updatingUi) return;
    const Entity *entity = m_canvas->selectedEntity();
    if (!entity || entity->kind() != Entity::Kind::Line) return;

    Document *doc = m_canvas->document();
    int index = doc->indexOf(entity->id());
    if (index < 0) return;

    const auto *line = static_cast<const LineEntity*>(entity);
    auto newLine = std::make_unique<LineEntity>(line->startPoint(), line->endPoint());
    newLine->setCenterLengthAngle(
        QPointF(m_lineCenterX->value(), m_lineCenterY->value()),
        m_lineLength->value(),
        m_lineAngle->value());

    applyProperties(index, std::move(newLine));
}

void GraphicsDesignPanel::applyRectProperties()
{
    if (m_updatingUi) return;
    const Entity *entity = m_canvas->selectedEntity();
    if (!entity || entity->kind() != Entity::Kind::Rectangle) return;

    Document *doc = m_canvas->document();
    int index = doc->indexOf(entity->id());
    if (index < 0) return;

    auto newRect = std::make_unique<RectangleEntity>(
        QPointF(m_rectCenterX->value(), m_rectCenterY->value()),
        m_rectWidth->value(),
        m_rectHeight->value(),
        m_rectAngle->value());

    applyProperties(index, std::move(newRect));
}

void GraphicsDesignPanel::applyCircleProperties()
{
    if (m_updatingUi) return;
    const Entity *entity = m_canvas->selectedEntity();
    if (!entity || entity->kind() != Entity::Kind::Circle) return;

    Document *doc = m_canvas->document();
    int index = doc->indexOf(entity->id());
    if (index < 0) return;

    auto newCircle = std::make_unique<CircleEntity>(
        QPointF(m_circleCenterX->value(), m_circleCenterY->value()),
        m_circleRadius->value());

    applyProperties(index, std::move(newCircle));
}

void GraphicsDesignPanel::applyArcProperties()
{
    if (m_updatingUi) return;
    const Entity *entity = m_canvas->selectedEntity();
    if (!entity || entity->kind() != Entity::Kind::Arc) return;

    Document *doc = m_canvas->document();
    int index = doc->indexOf(entity->id());
    if (index < 0) return;

    auto newArc = std::make_unique<ArcEntity>(
        QPointF(m_arcCenterX->value(), m_arcCenterY->value()),
        m_arcRadius->value(),
        m_arcStartAngle->value(),
        m_arcSweepAngle->value());

    applyProperties(index, std::move(newArc));
}

void GraphicsDesignPanel::applyTextProperties()
{
    if (m_updatingUi) return;
    const Entity *entity = m_canvas->selectedEntity();
    if (!entity || entity->kind() != Entity::Kind::Text) return;

    Document *doc = m_canvas->document();
    int index = doc->indexOf(entity->id());
    if (index < 0) return;

    const auto *text = static_cast<const TextEntity*>(entity);
    auto newText = std::make_unique<TextEntity>(
        QPointF(m_textPosX->value(), m_textPosY->value()),
        m_textEdit->toPlainText(),
        m_textFontSize->value());
    newText->setLetterSpacing(text->letterSpacing());
    newText->setLineHeight(m_textLineHeight->value());
    newText->setMaxWidth(text->maxWidth());

    bool isVector = (m_textFontModeCombo->currentIndex() == 0);
    if (isVector) {
        int vecIdx = m_textVectorFontCombo->currentIndex();
        int lastIdx = m_textVectorFontCombo->count() - 1;
        if (vecIdx == lastIdx) {
            QString customPath = m_textCustomFontPath->text();
            if (!customPath.isEmpty())
                newText->setVectorFont(customPath);
            else
                newText->setVectorFont(text->vectorFontPath());
        } else {
            QString vecPath = m_textVectorFontCombo->itemData(vecIdx).toString();
            if (!vecPath.isEmpty())
                newText->setVectorFont(vecPath);
        }
    } else {
        newText->setFontFamily(m_textSystemFontCombo->currentFont().family());
        newText->setSingleStroke(m_textSingleStrokeCheck->isChecked());
        if (m_textSingleStrokeCheck->isChecked()) {
            newText->setSsResolution(static_cast<int>(m_textSsResolution->value()));
            newText->setSsSmoothing(m_textSsSmoothing->value());
            newText->setSsCleanup(static_cast<int>(m_textSsCleanup->value()));
            newText->setSsMergeGap(m_textSsMergeGap->value());
            newText->setSsMinSegLen(m_textSsMinSegLen->value());
        }
    }

    applyProperties(index, std::move(newText));
}

void GraphicsDesignPanel::applyPolylineProperties()
{
    if (m_updatingUi) return;
    const Entity *entity = m_canvas->selectedEntity();
    if (!entity || entity->kind() != Entity::Kind::Polygon) return;

    Document *doc = m_canvas->document();
    int index = doc->indexOf(entity->id());
    if (index < 0) return;

    QList<QPointF> points;
    int rows = m_polylineTable->rowCount();
    for (int i = 0; i < rows; ++i) {
        QTableWidgetItem *xItem = m_polylineTable->item(i, 0);
        QTableWidgetItem *yItem = m_polylineTable->item(i, 1);
        if (!xItem || !yItem) continue;
        bool xOk = false, yOk = false;
        double x = xItem->text().toDouble(&xOk);
        double y = yItem->text().toDouble(&yOk);
        if (xOk && yOk)
            points.append(QPointF(x, y));
    }

    if (points.size() < 2) return;

    auto newPolyline = std::make_unique<PolylineEntity>(points);
    applyProperties(index, std::move(newPolyline));
}

void GraphicsDesignPanel::polylineInsertVertex()
{
    if (m_updatingUi) return;
    const Entity *entity = m_canvas->selectedEntity();
    if (!entity || entity->kind() != Entity::Kind::Polygon) return;

    Document *doc = m_canvas->document();
    int index = doc->indexOf(entity->id());
    if (index < 0) return;

    const auto *poly = static_cast<const PolylineEntity*>(entity);
    QList<QPointF> pts = poly->points();

    int row = m_polylineTable->currentRow();
    if (row < 0) row = pts.size() - 1;  // append after last if nothing selected

    QPointF newPt;
    if (row + 1 < pts.size())
        newPt = (pts[row] + pts[row + 1]) / 2.0;  // midpoint between row and next
    else
        newPt = pts[row];  // duplicate last point (user can edit it)

    pts.insert(row + 1, newPt);

    auto newPolyline = std::make_unique<PolylineEntity>(pts);
    applyProperties(index, std::move(newPolyline));
}

void GraphicsDesignPanel::polylineDeleteVertex()
{
    if (m_updatingUi) return;
    const Entity *entity = m_canvas->selectedEntity();
    if (!entity || entity->kind() != Entity::Kind::Polygon) return;

    Document *doc = m_canvas->document();
    int index = doc->indexOf(entity->id());
    if (index < 0) return;

    const auto *poly = static_cast<const PolylineEntity*>(entity);
    QList<QPointF> pts = poly->points();
    if (pts.size() <= 2) return;  // must keep at least 2 points

    int row = m_polylineTable->currentRow();
    if (row < 0 || row >= pts.size()) return;

    pts.removeAt(row);

    auto newPolyline = std::make_unique<PolylineEntity>(pts);
    applyProperties(index, std::move(newPolyline));
}

// ---------------------------------------------------------------------------
// Transform helpers
// ---------------------------------------------------------------------------

static QPointF rotatePoint(const QPointF &pt, const QPointF &pivot, double angleDeg)
{
    const double rad = qDegreesToRadians(angleDeg);
    const double c = qCos(rad), s = qSin(rad);
    const double dx = pt.x() - pivot.x(), dy = pt.y() - pivot.y();
    return { pivot.x() + dx * c - dy * s, pivot.y() + dx * s + dy * c };
}

static std::unique_ptr<Entity> makeRotatedEntity(const Entity *e, double angleDeg)
{
    const QPointF pivot = e->center();
    switch (e->kind()) {
    case Entity::Kind::Line: {
        const auto *line = static_cast<const LineEntity*>(e);
        return std::make_unique<LineEntity>(
            rotatePoint(line->startPoint(), pivot, angleDeg),
            rotatePoint(line->endPoint(), pivot, angleDeg));
    }
    case Entity::Kind::Rectangle: {
        const auto *rect = static_cast<const RectangleEntity*>(e);
        return std::make_unique<RectangleEntity>(
            pivot, rect->width(), rect->height(), rect->angleDegrees() + angleDeg);
    }
    case Entity::Kind::Circle:
        return e->clone();  // Circle is symmetric; rotation has no visible effect
    case Entity::Kind::Arc: {
        const auto *arc = static_cast<const ArcEntity*>(e);
        return std::make_unique<ArcEntity>(
            pivot, arc->radius(),
            arc->startAngleDegrees() + angleDeg,
            arc->sweepAngleDegrees());
    }
    case Entity::Kind::Polygon: {
        const auto *poly = static_cast<const PolylineEntity*>(e);
        QList<QPointF> pts;
        pts.reserve(poly->points().size());
        for (const QPointF &pt : poly->points())
            pts.append(rotatePoint(pt, pivot, angleDeg));
        return std::make_unique<PolylineEntity>(pts);
    }
    default:
        return e->clone();
    }
}

static std::unique_ptr<Entity> makeScaledEntity(const Entity *e, const QPointF &pivot, double factor)
{
    auto sp = [&](const QPointF &pt) -> QPointF {
        return pivot + (pt - pivot) * factor;
    };
    switch (e->kind()) {
    case Entity::Kind::Line: {
        const auto *line = static_cast<const LineEntity*>(e);
        return std::make_unique<LineEntity>(sp(line->startPoint()), sp(line->endPoint()));
    }
    case Entity::Kind::Circle: {
        const auto *c = static_cast<const CircleEntity*>(e);
        return std::make_unique<CircleEntity>(sp(c->center()), c->radius() * factor);
    }
    case Entity::Kind::Arc: {
        const auto *arc = static_cast<const ArcEntity*>(e);
        return std::make_unique<ArcEntity>(sp(arc->center()), arc->radius() * factor,
                                           arc->startAngleDegrees(), arc->sweepAngleDegrees());
    }
    case Entity::Kind::Rectangle: {
        const auto *rect = static_cast<const RectangleEntity*>(e);
        return std::make_unique<RectangleEntity>(sp(rect->center()),
                                                  rect->width()  * factor,
                                                  rect->height() * factor,
                                                  rect->angleDegrees());
    }
    case Entity::Kind::Polygon: {
        const auto *poly = static_cast<const PolylineEntity*>(e);
        QList<QPointF> pts;
        pts.reserve(poly->points().size());
        for (const QPointF &pt : poly->points())
            pts.append(sp(pt));
        return std::make_unique<PolylineEntity>(pts);
    }
    default:
        return e->clone();
    }
}

void GraphicsDesignPanel::applyMoveBy(){
    Document *doc = m_canvas->document();
    if (doc->selectedIds().isEmpty()) return;

    const QPointF delta(m_moveDx->value(), m_moveDy->value());
    for (const QUuid &id : doc->selectedIds()) {
        int idx = doc->indexOf(id);
        if (idx < 0) continue;
        const Entity *e = doc->entityAt(idx);
        if (!e) continue;
        auto copy = e->clone();
        copy->translate(delta);
        applyProperties(idx, std::move(copy));
    }

    // Reset inputs after apply
    m_moveDx->setValue(0.0);
    m_moveDy->setValue(0.0);
}

void GraphicsDesignPanel::applyRotateBy()
{
    Document *doc = m_canvas->document();
    if (doc->selectedIds().isEmpty()) return;

    // Compute selection bounding box center as pivot
    QRectF bb;
    for (const QUuid &id : doc->selectedIds()) {
        int i = doc->indexOf(id);
        if (i >= 0 && doc->entityAt(i)) bb = bb.united(doc->entityAt(i)->boundingRect());
    }
    const QPointF pivot = bb.center();
    const double angle  = m_rotateAngle->value();
    for (const QUuid &id : doc->selectedIds()) {
        int idx = doc->indexOf(id);
        if (idx < 0) continue;
        const Entity *e = doc->entityAt(idx);
        if (!e) continue;
        // Rotate entity around global selection center
        // For single selection, pivot == entity center so existing makeRotatedEntity still works.
        // For multi-select, we need to rotate around shared pivot.
        // Rotate around the shared pivot
        auto rotated = [&]() -> std::unique_ptr<Entity> {
            auto rp = [&](const QPointF &pt) { return rotatePoint(pt, pivot, angle); };
            switch (e->kind()) {
            case Entity::Kind::Line: {
                const auto *ln = static_cast<const LineEntity*>(e);
                return std::make_unique<LineEntity>(rp(ln->startPoint()), rp(ln->endPoint()));
            }
            case Entity::Kind::Circle: {
                const auto *c = static_cast<const CircleEntity*>(e);
                return std::make_unique<CircleEntity>(rp(c->center()), c->radius());
            }
            case Entity::Kind::Arc: {
                const auto *arc = static_cast<const ArcEntity*>(e);
                return std::make_unique<ArcEntity>(rp(arc->center()), arc->radius(),
                    arc->startAngleDegrees() + angle, arc->sweepAngleDegrees());
            }
            case Entity::Kind::Rectangle: {
                const auto *rect = static_cast<const RectangleEntity*>(e);
                return std::make_unique<RectangleEntity>(rp(rect->center()),
                    rect->width(), rect->height(), rect->angleDegrees() + angle);
            }
            case Entity::Kind::Polygon: {
                const auto *poly = static_cast<const PolylineEntity*>(e);
                QList<QPointF> pts;
                pts.reserve(poly->points().size());
                for (const QPointF &pt : poly->points()) pts.append(rp(pt));
                return std::make_unique<PolylineEntity>(pts);
            }
            default: return e->clone();
            }
        }();
        applyProperties(idx, std::move(rotated));
    }
    m_rotateAngle->setValue(0.0);
}

// ---------------------------------------------------------------------------
// Mirror / flip helpers
// ---------------------------------------------------------------------------

// Normalize angle to [0, 360)
static double normAngle(double a) {
    a = std::fmod(a, 360.0);
    if (a < 0.0) a += 360.0;
    return a;
}

static QPointF flipH(const QPointF &pt, const QPointF &pivot) {
    return QPointF(2.0 * pivot.x() - pt.x(), pt.y());
}
static QPointF flipV(const QPointF &pt, const QPointF &pivot) {
    return QPointF(pt.x(), 2.0 * pivot.y() - pt.y());
}

enum class FlipAxis { H, V };

static std::unique_ptr<Entity> makeMirroredEntity(const Entity *e, const QPointF &pivot, FlipAxis axis)
{
    using F = FlipAxis;
    auto fp = [&](const QPointF &pt) {
        return axis == F::H ? flipH(pt, pivot) : flipV(pt, pivot);
    };

    switch (e->kind()) {
    case Entity::Kind::Line: {
        const auto *line = static_cast<const LineEntity*>(e);
        return std::make_unique<LineEntity>(fp(line->startPoint()), fp(line->endPoint()));
    }
    case Entity::Kind::Circle: {
        const auto *c = static_cast<const CircleEntity*>(e);
        return std::make_unique<CircleEntity>(fp(c->center()), c->radius());
    }
    case Entity::Kind::Arc: {
        const auto *arc = static_cast<const ArcEntity*>(e);
        QPointF newC = fp(arc->center());
        double start = arc->startAngleDegrees();
        double sweep = arc->sweepAngleDegrees();
        double newStart, newSweep = sweep;
        if (axis == F::H) {
            // Mirror about vertical axis: angle θ → 180° - θ
            // Arc [start, start+sweep] maps to [180-(start+sweep), 180-start] CCW
            newStart = normAngle(180.0 - (start + sweep));
        } else {
            // Mirror about horizontal axis: angle θ → -θ (= 360-θ)
            // Arc [start, start+sweep] maps to [360-(start+sweep), 360-start] CCW
            newStart = normAngle(360.0 - (start + sweep));
        }
        return std::make_unique<ArcEntity>(newC, arc->radius(), newStart, newSweep);
    }
    case Entity::Kind::Rectangle: {
        const auto *rect = static_cast<const RectangleEntity*>(e);
        QPointF newC = fp(rect->center());
        double newAngle = axis == F::H ? (180.0 - rect->angleDegrees()) : (-rect->angleDegrees());
        return std::make_unique<RectangleEntity>(newC, rect->width(), rect->height(), newAngle);
    }
    case Entity::Kind::Polygon: {
        const auto *poly = static_cast<const PolylineEntity*>(e);
        QList<QPointF> pts;
        pts.reserve(poly->points().size());
        for (const QPointF &pt : poly->points())
            pts.append(fp(pt));
        return std::make_unique<PolylineEntity>(pts);
    }
    default:
        return e->clone();  // Text, dimensions: just flip center position
    }
}

/// Compute combined bounding-box center of all selected entities.
static QPointF selectionCenter(Document *doc)
{
    QRectF bb;
    for (const QUuid &id : doc->selectedIds()) {
        int idx = doc->indexOf(id);
        if (idx >= 0 && doc->entityAt(idx))
            bb = bb.united(doc->entityAt(idx)->boundingRect());
    }
    return bb.center();
}

void GraphicsDesignPanel::applyFlipH()
{
    Document *doc = m_canvas->document();
    if (doc->selectedIds().isEmpty()) return;
    const QPointF pivot = selectionCenter(doc);
    for (const QUuid &id : doc->selectedIds()) {
        int idx = doc->indexOf(id);
        if (idx < 0) continue;
        const Entity *e = doc->entityAt(idx);
        if (!e) continue;
        applyProperties(idx, makeMirroredEntity(e, pivot, FlipAxis::H));
    }
}

void GraphicsDesignPanel::applyFlipV()
{
    Document *doc = m_canvas->document();
    if (doc->selectedIds().isEmpty()) return;
    const QPointF pivot = selectionCenter(doc);
    for (const QUuid &id : doc->selectedIds()) {
        int idx = doc->indexOf(id);
        if (idx < 0) continue;
        const Entity *e = doc->entityAt(idx);
        if (!e) continue;
        applyProperties(idx, makeMirroredEntity(e, pivot, FlipAxis::V));
    }
}

void GraphicsDesignPanel::applyRectArray()
{
    Document *doc = m_canvas->document();
    if (doc->selectedIds().isEmpty()) return;

    int rows  = m_arrayRows->value();
    int cols  = m_arrayCols->value();
    double dx = m_arraySpacingX->value();
    double dy = m_arraySpacingY->value();

    // Collect originals first to avoid index drift as we add entities
    std::vector<const Entity*> originals;
    originals.reserve(doc->selectedIds().size());
    for (const QUuid &id : doc->selectedIds()) {
        int idx = doc->indexOf(id);
        if (idx >= 0 && doc->entityAt(idx))
            originals.push_back(doc->entityAt(idx));
    }
    if (originals.empty()) return;

    // (row=0, col=0) is the original — skip it; start at (r,c) != (0,0)
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (r == 0 && c == 0) continue;  // original position
            QPointF offset(c * dx, r * dy);
            for (const Entity *e : originals) {
                auto copy = e->clone();
                copy->translate(offset);
                doc->commandStack()->execute(
                    std::make_unique<AddEntityCommand>(std::move(copy)));
            }
        }
    }
}

void GraphicsDesignPanel::applyScaleBy()
{
    Document *doc = m_canvas->document();
    if (doc->selectedIds().isEmpty()) return;
    double factor = m_scaleFactor->value();
    if (qFuzzyCompare(factor, 1.0)) return;

    const QPointF pivot = selectionCenter(doc);
    for (const QUuid &id : doc->selectedIds()) {
        int idx = doc->indexOf(id);
        if (idx < 0) continue;
        const Entity *e = doc->entityAt(idx);
        if (!e) continue;
        applyProperties(idx, makeScaledEntity(e, pivot, factor));
    }
    m_scaleFactor->setValue(1.0);
}

void GraphicsDesignPanel::applyPolarArray()
{
    Document *doc = m_canvas->document();
    if (doc->selectedIds().isEmpty()) return;

    int    n    = m_polarCount->value();
    double span = m_polarSpan->value();
    if (n < 2) return;

    // Determine center: if both are 0, use selection center
    QPointF center(m_polarCenterX->value(), m_polarCenterY->value());
    if (qFuzzyIsNull(center.x()) && qFuzzyIsNull(center.y()))
        center = selectionCenter(doc);

    // Collect originals
    std::vector<const Entity*> originals;
    for (const QUuid &id : doc->selectedIds()) {
        int idx = doc->indexOf(id);
        if (idx >= 0 && doc->entityAt(idx))
            originals.push_back(doc->entityAt(idx));
    }
    if (originals.empty()) return;

    // Angle step: for full circle (360°) with N copies, step = 360/N
    // For partial span, step = span / (N-1)
    const bool fullCircle = qFuzzyCompare(span, 360.0);
    double step = fullCircle ? (span / n) : (span / (n - 1));

    // Start at step (skip 0 — that's the original)
    for (int i = 1; i < n; ++i) {
        double angle = step * i;
        for (const Entity *e : originals) {
            // Rotate each entity around shared center
            auto rotated = [&]() -> std::unique_ptr<Entity> {
                auto rp = [&](const QPointF &pt) { return rotatePoint(pt, center, angle); };
                switch (e->kind()) {
                case Entity::Kind::Line: {
                    const auto *ln = static_cast<const LineEntity*>(e);
                    return std::make_unique<LineEntity>(rp(ln->startPoint()), rp(ln->endPoint()));
                }
                case Entity::Kind::Circle: {
                    const auto *c = static_cast<const CircleEntity*>(e);
                    return std::make_unique<CircleEntity>(rp(c->center()), c->radius());
                }
                case Entity::Kind::Arc: {
                    const auto *arc = static_cast<const ArcEntity*>(e);
                    return std::make_unique<ArcEntity>(rp(arc->center()), arc->radius(),
                        arc->startAngleDegrees() + angle, arc->sweepAngleDegrees());
                }
                case Entity::Kind::Rectangle: {
                    const auto *rect = static_cast<const RectangleEntity*>(e);
                    return std::make_unique<RectangleEntity>(rp(rect->center()),
                        rect->width(), rect->height(), rect->angleDegrees() + angle);
                }
                case Entity::Kind::Polygon: {
                    const auto *poly = static_cast<const PolylineEntity*>(e);
                    QList<QPointF> pts;
                    pts.reserve(poly->points().size());
                    for (const QPointF &pt : poly->points()) pts.append(rp(pt));
                    return std::make_unique<PolylineEntity>(pts);
                }
                default: return e->clone();
                }
            }();
            doc->commandStack()->execute(std::make_unique<AddEntityCommand>(std::move(rotated)));
        }
    }
}

// ---------------------------------------------------------------------------
// Layer management slots
// ---------------------------------------------------------------------------

void GraphicsDesignPanel::refreshLayerList()
{
    Document *doc = m_canvas->document();

    m_layerList->blockSignals(true);
    m_layerList->clear();

    for (int i = 0; i < doc->layerCount(); ++i) {
        const Layer &layer = doc->layerAt(i);
        auto *item = new QListWidgetItem(layer.name);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
        // Checkbox = visible
        item->setCheckState(layer.visible ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole + 1, layer.locked);
        item->setData(Qt::UserRole + 2, layer.color);
        // Dim text if locked (keep as fallback for non-delegate views)
        if (layer.locked)
            item->setForeground(Qt::gray);
        // Bold = active layer
        if (layer.id == doc->activeLayerId()) {
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
        }
        m_layerList->addItem(item);
    }

    m_layerList->blockSignals(false);
}

void GraphicsDesignPanel::onLayerItemChanged(QListWidgetItem *item)
{
    int row = m_layerList->row(item);
    Document *doc = m_canvas->document();
    if (row < 0 || row >= doc->layerCount()) return;

    // Visibility toggle (checkbox)
    doc->setLayerVisible(row, item->checkState() == Qt::Checked);

    // Lock toggle (UserRole + 1)
    bool locked = item->data(Qt::UserRole + 1).toBool();
    doc->setLayerLocked(row, locked);

    // Name change (inline edit)
    QString newName = item->text().trimmed();
    if (!newName.isEmpty() && newName != doc->layerAt(row).name)
        doc->setLayerName(row, newName);
}

void GraphicsDesignPanel::onLayerCurrentRowChanged(int row)
{
    Document *doc = m_canvas->document();
    bool canDelete = (row > 0 && row < doc->layerCount());  // cannot delete layer 0
    bool canAct    = (row >= 0 && row < doc->layerCount());
    m_deleteLayerBtn->setEnabled(canDelete);
    m_lockLayerBtn->setEnabled(canAct);
    m_moveToLayerBtn->setEnabled(canAct && !m_canvas->document()->selectedIds().isEmpty());

    if (canAct) {
        doc->setActiveLayerId(doc->layerAt(row).id);
        refreshLayerList();  // update bold marker
    }
}

void GraphicsDesignPanel::onAddLayerClicked()
{
    Document *doc = m_canvas->document();
    doc->addLayer(QString("Layer %1").arg(doc->layerCount()));
    refreshLayerList();
    m_layerList->setCurrentRow(m_layerList->count() - 1);
}

void GraphicsDesignPanel::onDeleteLayerClicked()
{
    int row = m_layerList->currentRow();
    if (row <= 0) return;
    m_canvas->document()->removeLayerAt(row);
    refreshLayerList();
}

void GraphicsDesignPanel::onLockLayerClicked()
{
    int row = m_layerList->currentRow();
    Document *doc = m_canvas->document();
    if (row < 0 || row >= doc->layerCount()) return;
    bool nowLocked = !doc->layerAt(row).locked;
    doc->setLayerLocked(row, nowLocked);
    m_lockLayerBtn->setText(nowLocked ? "Unlock" : "Lock");
    refreshLayerList();
}

void GraphicsDesignPanel::onMoveEntityToLayerClicked()
{
    int row = m_layerList->currentRow();
    Document *doc = m_canvas->document();
    if (row < 0 || row >= doc->layerCount()) return;

    const QUuid targetLayerId = doc->layerAt(row).id;
    const QSet<QUuid> ids = doc->selectedIds();
    for (const QUuid &id : ids) {
        int entityIndex = doc->indexOf(id);
        if (entityIndex < 0) continue;
        const Entity *entity = doc->entityAt(entityIndex);
        if (!entity) continue;
        auto newEntity = entity->clone();
        newEntity->setLayerId(targetLayerId);
        applyProperties(entityIndex, std::move(newEntity));
    }
}