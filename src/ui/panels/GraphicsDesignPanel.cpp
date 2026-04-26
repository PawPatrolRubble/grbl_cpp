#include "GraphicsDesignPanel.h"
#include "ToolBarWidget.h"
#include "LayerListDelegate.h"
#include "EntityTransform.h"
#include "TransformPanel.h"
#include "PropertyPages/PropertyPage.h"
#include "PropertyPages/EmptyPage.h"
#include "PropertyPages/LinePage.h"
#include "PropertyPages/RectanglePage.h"
#include "PropertyPages/CirclePage.h"
#include "PropertyPages/ArcPage.h"
#include "PropertyPages/PolylinePage.h"
#include "PropertyPages/DimensionPage.h"
#include "PropertyPages/AngularDimensionPage.h"
#include "PropertyPages/RadiusDimensionPage.h"
#include "PropertyPages/SplinePage.h"
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
#include "../../grbl/GraphicsDesign/SplineEntity.h"
#include "../../grbl/DxfParser.h"

#include <cmath>

// Layer list viewport constants (shared with LayerListDelegate)
static const int kEyeColW   = 22;
static const int kLockColW  = 22;
static const int kColorSwW  = 16;
static const int kIconsW    = kEyeColW + kLockColW;
static const int kSwatchOff = kIconsW + 2;

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
#include <QMouseEvent>

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

    // Vertical tool toolbar (ToolBarWidget extracted component)
    m_toolBar = new ToolBarWidget();
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

    // Measure readout group (shown only when Measure tool active)
    m_measureGroup = new QGroupBox("Measurement");
    m_measureGroup->setObjectName("measureGroup");
    auto *mgLay = new QGridLayout(m_measureGroup);
    mgLay->setContentsMargins(6, 4, 6, 4);
    mgLay->setSpacing(3);
    mgLay->addWidget(new QLabel("Dist:"),  0, 0);
    m_measureDistLabel = new QLabel("—");
    m_measureDistLabel->setStyleSheet("font-weight: bold; color: #ffc832;");
    mgLay->addWidget(m_measureDistLabel, 0, 1);
    mgLay->addWidget(new QLabel("Δx:"),   0, 2);
    m_measureDxLabel = new QLabel("—");
    mgLay->addWidget(m_measureDxLabel,   0, 3);
    mgLay->addWidget(new QLabel("Δy:"),   1, 0);
    m_measureDyLabel = new QLabel("—");
    mgLay->addWidget(m_measureDyLabel,   1, 1);
    mgLay->addWidget(new QLabel("Angle:"), 1, 2);
    m_measureAngleLabel = new QLabel("—");
    mgLay->addWidget(m_measureAngleLabel, 1, 3);
    m_measureGroup->setVisible(false);
    ctrlBar->addWidget(m_measureGroup);

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

    // Snap type toggle bar (like AutoCAD OSNAP toolbar)
    auto *snapBar = new QHBoxLayout();
    snapBar->setSpacing(3);

    auto makeSnapBtn = [](const QString& text, const QString& tip) {
        auto *btn = new QToolButton();
        btn->setText(text);
        btn->setCheckable(true);
        btn->setChecked(true);
        btn->setToolTip(tip);
        btn->setMaximumWidth(40);
        btn->setStyleSheet("QToolButton { padding: 1px 3px; font-size: 11px; }"
                           "QToolButton:checked { background: #2d6a4f; color: #fff; border: 1px solid #40916c; }");
        return btn;
    };

    snapBar->addWidget(new QLabel("Osnap:"));
    m_snapGridBtn = makeSnapBtn("Grid", "Snap to grid points");
    m_snapEndBtn  = makeSnapBtn("End",  "Snap to endpoints and corners");
    m_snapMidBtn  = makeSnapBtn("Mid",  "Snap to midpoints");
    m_snapCtrBtn  = makeSnapBtn("Ctr",  "Snap to centers and quadrants");
    m_snapIntBtn  = makeSnapBtn("Int",  "Snap to intersections");
    m_snapPerpBtn = makeSnapBtn("Perp", "Snap to perpendicular points");
    m_snapNearBtn = makeSnapBtn("Near", "Snap to nearest point on curve");
    snapBar->addWidget(m_snapGridBtn);
    snapBar->addWidget(m_snapEndBtn);
    snapBar->addWidget(m_snapMidBtn);
    snapBar->addWidget(m_snapCtrBtn);
    snapBar->addWidget(m_snapIntBtn);
    snapBar->addWidget(m_snapPerpBtn);
    snapBar->addWidget(m_snapNearBtn);
    snapBar->addStretch();
    rightLayout->addLayout(snapBar);

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

    // ---- Floating Property Panel (overlay on canvas) ----
    m_floatingPropertyPanel = new QWidget(m_canvas);
    m_floatingPropertyPanel->setObjectName("floatingPropertyPanel");
    m_floatingPropertyPanel->setStyleSheet(
        "#floatingPropertyPanel {"
        "  background: rgba(255, 255, 255, 230);"
        "  border: 1px solid #c0c0c0;"
        "  border-radius: 6px;"
        "}"
    );
    auto *floatingLayout = new QVBoxLayout(m_floatingPropertyPanel);
    floatingLayout->setContentsMargins(8, 8, 8, 8);
    floatingLayout->setSpacing(6);

    // Opacity control header
    auto *opacityHeader = new QHBoxLayout();
    auto *opacityLabel = new QLabel("Opacity:");
    opacityLabel->setStyleSheet("font-size: 10px; color: #666;");
    opacityHeader->addWidget(opacityLabel);
    m_opacitySlider = new QSlider(Qt::Horizontal);
    m_opacitySlider->setRange(30, 100);
    m_opacitySlider->setValue(90);
    m_opacitySlider->setMaximumWidth(80);
    m_opacitySlider->setToolTip("Adjust property panel transparency");
    connect(m_opacitySlider, &QSlider::valueChanged, this, [this](int value) {
        // Update opacity via stylesheet background alpha
        int alpha = static_cast<int>(value * 2.55);  // Convert 0-100 to 0-255
        m_floatingPropertyPanel->setStyleSheet(
            QString("#floatingPropertyPanel {"
                    "  background: rgba(255, 255, 255, %1);"
                    "  border: 1px solid #c0c0c0;"
                    "  border-radius: 6px;"
                    "}").arg(alpha)
        );
    });
    opacityHeader->addWidget(m_opacitySlider);
    floatingLayout->addLayout(opacityHeader);

    // Property stack (moved from right panel)
    m_propertyStack = new QStackedWidget();
    m_propertyStack->setMaximumHeight(280);  // Allow more height for floating panel
    floatingLayout->addWidget(m_propertyStack);

    // Position at top-left of canvas (sizing happens after pages are built)
    m_floatingPropertyPanel->setFixedWidth(350);

    // Property pages �?each PropertyPage subclass owns its own UI
    m_propertyStack->addWidget(new EmptyPage());               // index 0: no selection
    m_propertyStack->addWidget(new LinePage());                // index 1
    m_propertyStack->addWidget(new RectanglePage());           // index 2
    m_propertyStack->addWidget(new CirclePage());              // index 3
    m_propertyStack->addWidget(new ArcPage());                 // index 4
    buildTextPage();                                           // index 5 (inline, no PropertyPage yet)
    m_polylinePage = new PolylinePage();
    m_propertyStack->addWidget(m_polylinePage);                // index 6
    m_propertyStack->addWidget(new DimensionPage());           // index 7
    m_propertyStack->addWidget(new AngularDimensionPage());    // index 8
    m_propertyStack->addWidget(new RadiusDimensionPage());     // index 9
    m_propertyStack->addWidget(new SplinePage());              // index 10

    m_transformPanel = new TransformPanel();
    m_transformPanel->setVisible(false);
    rightLayout->addWidget(m_transformPanel);
    rightLayout->addWidget(new QLabel("Entities:"));
    m_entityList = new QListWidget();
    m_entityList->setMaximumHeight(120);
    m_entityList->setSelectionMode(QAbstractItemView::SingleSelection);
    rightLayout->addWidget(m_entityList);

    buildLayerSection();
    m_floatingPropertyPanel->layout()->addWidget(m_layerGroup);  // Add layers to floating panel

    // Now size and position the floating panel after all content is added
    m_floatingPropertyPanel->adjustSize();
    m_floatingPropertyPanel->move(10, 10);  // Position at top-left with margin
    m_floatingPropertyPanel->show();

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

void GraphicsDesignPanel::buildLayerSection()
{
    m_layerGroup = new QGroupBox("Layers");
    auto *vlay = new QVBoxLayout(m_layerGroup);
    vlay->setContentsMargins(6, 6, 6, 6);
    vlay->setSpacing(4);

    m_layerList = new QListWidget();
    m_layerList->setMaximumHeight(110);
    m_layerList->setItemDelegate(new LayerListDelegate(m_layerList));
    // Use an event filter on the viewport so we get the exact QMouseEvent position.
    // QCursor::pos() in a signal handler can be stale/wrong on some platforms.
    m_layerList->viewport()->installEventFilter(this);
    vlay->addWidget(m_layerList);

    auto *btnRow = new QHBoxLayout();
    m_addLayerBtn = new QPushButton("Add");
    m_addLayerBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_deleteLayerBtn = new QPushButton("Delete");
    m_deleteLayerBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_deleteLayerBtn->setEnabled(false);
    m_lockLayerBtn = new QPushButton("Lock");
    m_lockLayerBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_lockLayerBtn->setEnabled(false);
    m_moveToLayerBtn = new QPushButton("Move Sel.");
    m_moveToLayerBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_moveToLayerBtn->setToolTip("Move selected entity to this layer");
    m_moveToLayerBtn->setEnabled(false);
    btnRow->addWidget(m_addLayerBtn, 1);
    btnRow->addWidget(m_deleteLayerBtn, 1);
    btnRow->addWidget(m_lockLayerBtn, 1);
    btnRow->addWidget(m_moveToLayerBtn, 1);
    vlay->addLayout(btnRow);
}

void GraphicsDesignPanel::setupConnections()
{
    // Toolbar tool selection (ToolBarWidget extracted component)
    connect(m_toolBar, &ToolBarWidget::toolSelected, this, &GraphicsDesignPanel::onToolChanged);

    // Sync toolbar buttons when canvas changes tool internally (e.g. auto-select after draw)
    connect(m_canvas, &DesignCanvas::toolChanged, this, &GraphicsDesignPanel::onCanvasToolChanged);

    // Snap toggle (bidirectional: button �?canvas)
    connect(m_snapBtn, &QToolButton::toggled, m_canvas, &DesignCanvas::setSnapEnabled);
    connect(m_canvas, &DesignCanvas::snapEnabledChanged, m_snapBtn, &QToolButton::setChecked);

    // Per-type snap toggles
    connect(m_snapGridBtn, &QToolButton::toggled, m_canvas, &DesignCanvas::setGridSnapEnabled);
    connect(m_snapEndBtn,  &QToolButton::toggled, m_canvas, &DesignCanvas::setEndpointSnapEnabled);
    connect(m_snapMidBtn,  &QToolButton::toggled, m_canvas, &DesignCanvas::setMidpointSnapEnabled);
    connect(m_snapCtrBtn,  &QToolButton::toggled, m_canvas, &DesignCanvas::setCenterSnapEnabled);
    connect(m_snapIntBtn,  &QToolButton::toggled, m_canvas, &DesignCanvas::setIntersectionSnapEnabled);
    connect(m_snapPerpBtn, &QToolButton::toggled, m_canvas, &DesignCanvas::setPerpendicularSnapEnabled);
    connect(m_snapNearBtn, &QToolButton::toggled, m_canvas, &DesignCanvas::setNearestSnapEnabled);

    connect(m_gridSizeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            m_canvas, &DesignCanvas::setGridSize);
    connect(m_canvas, &DesignCanvas::gridSizeChanged, m_gridSizeSpin, &QDoubleSpinBox::setValue);
    connect(m_filletRadiusSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            m_canvas, &DesignCanvas::setFilletRadius);
    connect(m_chamferDistSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            m_canvas, &DesignCanvas::setChamferDistance);
    connect(m_offsetDistSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            m_canvas, &DesignCanvas::setOffsetDistance);
    connect(m_canvas, &DesignCanvas::measurementChanged,
            this, &GraphicsDesignPanel::onMeasurementChanged);
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

    // Line/Rect/Circle/Arc connects are handled by the generic PropertyPage::propertyChanged loop below.

    // Text page (inline, kept as-is)
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

    // Generic PropertyPage::propertyChanged handler for pages 1-4, 7-10
    for (int i = 1; i <= 10; ++i) {
        if (i == 5 || i == 6) continue; // Text (inline) and Polyline (special)
        auto *page = qobject_cast<PropertyPage*>(m_propertyStack->widget(i));
        if (page) {
            connect(page, &PropertyPage::propertyChanged, this, [this, page]() {
                if (m_updatingUi) return;
                const Entity *entity = m_canvas->selectedEntity();
                if (!entity) return;
                Document *doc = m_canvas->document();
                int index = doc->indexOf(entity->id());
                if (index < 0) return;
                auto newEntity = page->apply();
                if (newEntity) applyProperties(index, std::move(newEntity));
            });
        }
    }

    // PolylinePage insert/delete
    connect(m_polylinePage, &PolylinePage::insertAfterRequested, this, [this]() {
        if (m_updatingUi) return;
        const Entity *entity = m_canvas->selectedEntity();
        if (!entity || entity->kind() != Entity::Kind::Polygon) return;
        const auto *poly = static_cast<const PolylineEntity*>(entity);
        QList<QPointF> pts = poly->points();
        int insertAfter = m_polylinePage->currentRow();
        QPointF newPt;
        if (insertAfter >= 0 && insertAfter < pts.size()) {
            int next = (insertAfter + 1) % pts.size();
            newPt = (pts[insertAfter] + pts[next]) / 2.0;
            pts.insert(insertAfter + 1, newPt);
        } else {
            newPt = pts.isEmpty() ? QPointF(0, 0) : pts.last() + QPointF(10, 0);
            pts.append(newPt);
        }
        Document *doc = m_canvas->document();
        int idx = doc->indexOf(entity->id());
        if (idx < 0) return;
        applyProperties(idx, std::make_unique<PolylineEntity>(pts));
    });
    connect(m_polylinePage, &PolylinePage::deletePointRequested, this, [this]() {
        if (m_updatingUi) return;
        const Entity *entity = m_canvas->selectedEntity();
        if (!entity || entity->kind() != Entity::Kind::Polygon) return;
        const auto *poly = static_cast<const PolylineEntity*>(entity);
        QList<QPointF> pts = poly->points();
        int row = m_polylinePage->currentRow();
        if (row < 0 || row >= pts.size() || pts.size() <= 2) return;
        pts.removeAt(row);
        Document *doc = m_canvas->document();
        int idx = doc->indexOf(entity->id());
        if (idx < 0) return;
        applyProperties(idx, std::make_unique<PolylineEntity>(pts));
    });

    // TransformPanel signals
    connect(m_transformPanel, &TransformPanel::moveRequested,
            this, [this](double dx, double dy) { applyMoveBy(dx, dy); });
    connect(m_transformPanel, &TransformPanel::rotateRequested,
            this, [this](double angle) { applyRotateBy(angle); });
    connect(m_transformPanel, &TransformPanel::flipHorizontalRequested,
            this, &GraphicsDesignPanel::applyFlipH);
    connect(m_transformPanel, &TransformPanel::flipVerticalRequested,
            this, &GraphicsDesignPanel::applyFlipV);
    connect(m_transformPanel, &TransformPanel::rectArrayRequested,
            this, [this](int r, int c, double sx, double sy) { applyRectArray(r, c, sx, sy); });
    connect(m_transformPanel, &TransformPanel::scaleRequested,
            this, [this](double f) { applyScaleBy(f); });
    connect(m_transformPanel, &TransformPanel::polarArrayRequested,
            this, [this](int n, double span, double cx, double cy) { applyPolarArray(n, span, cx, cy); });
    connect(m_transformPanel, &TransformPanel::mirrorByAngleRequested,
            this, [this](double angle) { applyMirrorByAngle(angle); });
    connect(m_transformPanel, &TransformPanel::alignLeftRequested,     this, &GraphicsDesignPanel::applyAlignLeft);
    connect(m_transformPanel, &TransformPanel::alignCenterHRequested,  this, &GraphicsDesignPanel::applyAlignCenterH);
    connect(m_transformPanel, &TransformPanel::alignRightRequested,    this, &GraphicsDesignPanel::applyAlignRight);
    connect(m_transformPanel, &TransformPanel::alignTopRequested,      this, &GraphicsDesignPanel::applyAlignTop);
    connect(m_transformPanel, &TransformPanel::alignCenterVRequested,  this, &GraphicsDesignPanel::applyAlignCenterV);
    connect(m_transformPanel, &TransformPanel::alignBottomRequested,   this, &GraphicsDesignPanel::applyAlignBottom);

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
    bool isMeasure = (tool == DesignCanvas::ToolType::Measure);
    m_filletRadiusLabel->setVisible(isFillet);
    m_filletRadiusSpin->setVisible(isFillet);
    m_chamferDistLabel->setVisible(isChamfer);
    m_chamferDistSpin->setVisible(isChamfer);
    m_offsetDistLabel->setVisible(isOffset);
    m_offsetDistSpin->setVisible(isOffset);
    m_measureGroup->setVisible(isMeasure);
}

void GraphicsDesignPanel::onCanvasToolChanged()
{
    // Sync toolbar buttons when canvas changes tool internally (e.g. auto-select after drawing)
    int index = static_cast<int>(m_canvas->currentToolType());
    
    // Update button checked state (ToolBarWidget handles this internally)
    m_toolBar->setToolChecked(index);
    
    // Update tool-specific parameter widgets
    DesignCanvas::ToolType tool = static_cast<DesignCanvas::ToolType>(index);
    bool isFillet  = (tool == DesignCanvas::ToolType::Fillet);
    bool isChamfer = (tool == DesignCanvas::ToolType::Chamfer);
    bool isOffset  = (tool == DesignCanvas::ToolType::Offset);
    bool isMeasure = (tool == DesignCanvas::ToolType::Measure);
    m_filletRadiusLabel->setVisible(isFillet);
    m_filletRadiusSpin->setVisible(isFillet);
    m_chamferDistLabel->setVisible(isChamfer);
    m_chamferDistSpin->setVisible(isChamfer);
    m_offsetDistLabel->setVisible(isOffset);
    m_offsetDistSpin->setVisible(isOffset);
    m_measureGroup->setVisible(isMeasure);
}

void GraphicsDesignPanel::onSelectionChanged()
{
    const Entity *entity = m_canvas->selectedEntity();

    m_updatingUi = true;

    if (!entity) {
        m_propertyStack->setCurrentIndex(0);
        m_deleteBtn->setEnabled(false);
        m_transformPanel->setVisible(false);
        m_updatingUi = false;
        return;
    }

    m_deleteBtn->setEnabled(true);

    static const QHash<int, int> kindToPage = {
        {static_cast<int>(Entity::Kind::Line),             1},
        {static_cast<int>(Entity::Kind::Rectangle),        2},
        {static_cast<int>(Entity::Kind::Circle),           3},
        {static_cast<int>(Entity::Kind::Arc),              4},
        {static_cast<int>(Entity::Kind::Text),             5},
        {static_cast<int>(Entity::Kind::Polygon),          6},
        {static_cast<int>(Entity::Kind::LinearDimension),  7},
        {static_cast<int>(Entity::Kind::AngularDimension), 8},
        {static_cast<int>(Entity::Kind::RadiusDimension),  9},
        {static_cast<int>(Entity::Kind::Spline),           10},
    };

    int pageIdx = kindToPage.value(static_cast<int>(entity->kind()), 0);
    m_propertyStack->setCurrentIndex(pageIdx);

    if (pageIdx == 5) {
        // Text: inline page
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
    } else if (pageIdx > 0) {
        auto *page = qobject_cast<PropertyPage*>(m_propertyStack->widget(pageIdx));
        if (page) page->bind(entity);
    }

    // Show transform panel and reset its inputs
    m_transformPanel->setVisible(true);
    m_transformPanel->resetInputs();

    // Sync entity list selection
    Document *doc = m_canvas->document();
    for (int i = 0; i < doc->entityCount(); ++i) {
        if (doc->entityAt(i)->id() == entity->id()) {
            m_entityList->blockSignals(true);
            m_entityList->setCurrentRow(i);
            m_entityList->blockSignals(false);
            break;
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

void GraphicsDesignPanel::applyMoveBy(double dx, double dy)
{
    Document *doc = m_canvas->document();
    if (doc->selectedIds().isEmpty()) return;

    const QPointF delta(dx, dy);
    for (const QUuid &id : doc->selectedIds()) {
        int idx = doc->indexOf(id);
        if (idx < 0) continue;
        const Entity *e = doc->entityAt(idx);
        if (!e) continue;
        auto copy = e->clone();
        copy->translate(delta);
        applyProperties(idx, std::move(copy));
    }
}

void GraphicsDesignPanel::applyRotateBy(double angle)
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
    // angle parameter passed in
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
            auto rp = [&](const QPointF &pt) { return EntityTransform::rotatePoint(pt, pivot, angle); };
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
}

void GraphicsDesignPanel::applyFlipH()
{
    Document *doc = m_canvas->document();
    if (doc->selectedIds().isEmpty()) return;
    const QPointF pivot = EntityTransform::selectionCenter(doc);
    for (const QUuid &id : doc->selectedIds()) {
        int idx = doc->indexOf(id);
        if (idx < 0) continue;
        const Entity *e = doc->entityAt(idx);
        if (!e) continue;
        applyProperties(idx, EntityTransform::makeMirroredEntity(e, pivot, EntityTransform::FlipAxis::H));
    }
}

void GraphicsDesignPanel::applyFlipV()
{
    Document *doc = m_canvas->document();
    if (doc->selectedIds().isEmpty()) return;
    const QPointF pivot = EntityTransform::selectionCenter(doc);
    for (const QUuid &id : doc->selectedIds()) {
        int idx = doc->indexOf(id);
        if (idx < 0) continue;
        const Entity *e = doc->entityAt(idx);
        if (!e) continue;
        applyProperties(idx, EntityTransform::makeMirroredEntity(e, pivot, EntityTransform::FlipAxis::V));
    }
}

void GraphicsDesignPanel::applyRectArray(int rows, int cols, double dx, double dy)
{
    Document *doc = m_canvas->document();
    if (doc->selectedIds().isEmpty()) return;

    // Collect originals first to avoid index drift as we add entities
    std::vector<const Entity*> originals;
    originals.reserve(doc->selectedIds().size());
    for (const QUuid &id : doc->selectedIds()) {
        int idx = doc->indexOf(id);
        if (idx >= 0 && doc->entityAt(idx))
            originals.push_back(doc->entityAt(idx));
    }
    if (originals.empty()) return;

    // (row=0, col=0) is the original �?skip it; start at (r,c) != (0,0)
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

void GraphicsDesignPanel::applyScaleBy(double factor)
{
    Document *doc = m_canvas->document();
    if (doc->selectedIds().isEmpty()) return;
    if (qFuzzyCompare(factor, 1.0)) return;

    const QPointF pivot = EntityTransform::selectionCenter(doc);
    for (const QUuid &id : doc->selectedIds()) {
        int idx = doc->indexOf(id);
        if (idx < 0) continue;
        const Entity *e = doc->entityAt(idx);
        if (!e) continue;
        applyProperties(idx, EntityTransform::makeScaledEntity(e, pivot, factor));
    }
}

void GraphicsDesignPanel::applyPolarArray(int n, double span, double cx, double cy)
{
    Document *doc = m_canvas->document();
    if (doc->selectedIds().isEmpty()) return;
    if (n < 2) return;

    // Determine center: if both are 0, use selection center
    QPointF center(cx, cy);
    if (qFuzzyIsNull(center.x()) && qFuzzyIsNull(center.y()))
        center = EntityTransform::selectionCenter(doc);

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

    // Start at step (skip 0 �?that's the original)
    for (int i = 1; i < n; ++i) {
        double angle = step * i;
        for (const Entity *e : originals) {
            // Rotate each entity around shared center
            auto rotated = [&]() -> std::unique_ptr<Entity> {
                auto rp = [&](const QPointF &pt) { return EntityTransform::rotatePoint(pt, center, angle); };
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
// Mirror by axis angle
// ---------------------------------------------------------------------------

void GraphicsDesignPanel::applyMirrorByAngle(double axisDeg)
{
    Document *doc = m_canvas->document();
    if (doc->selectedIds().isEmpty()) return;

    double axisRad = axisDeg * M_PI / 180.0;

    // Reflect point P around a line through 'pivot' at angle 'axisRad'
    // Using reflection formula: P' = 2*(P·n)n - P  relative to pivot,
    // where n = (cos(axisRad), sin(axisRad)) is the axis direction unit vector.
    const QPointF pivot = EntityTransform::selectionCenter(doc);

    auto reflectPt = [&](const QPointF &pt) -> QPointF {
        double dx = pt.x() - pivot.x();
        double dy = pt.y() - pivot.y();
        double ca = std::cos(axisRad), sa = std::sin(axisRad);
        double dot = dx * ca + dy * sa;
        return QPointF(pivot.x() + 2.0 * dot * ca - dx,
                       pivot.y() + 2.0 * dot * sa - dy);
    };

    // Reflect arc start angle: angle �?2*axisAngle �?angle
    auto reflectAngle = [&](double angleDeg) -> double {
        return EntityTransform::normAngle(2.0 * axisDeg - angleDeg);
    };

    for (const QUuid &id : doc->selectedIds()) {
        int idx = doc->indexOf(id);
        if (idx < 0) continue;
        const Entity *e = doc->entityAt(idx);
        if (!e) continue;

        std::unique_ptr<Entity> reflected;
        switch (e->kind()) {
        case Entity::Kind::Line: {
            const auto *ln = static_cast<const LineEntity*>(e);
            reflected = std::make_unique<LineEntity>(reflectPt(ln->startPoint()),
                                                     reflectPt(ln->endPoint()));
            break;
        }
        case Entity::Kind::Circle: {
            const auto *c = static_cast<const CircleEntity*>(e);
            reflected = std::make_unique<CircleEntity>(reflectPt(c->center()), c->radius());
            break;
        }
        case Entity::Kind::Arc: {
            const auto *arc = static_cast<const ArcEntity*>(e);
            QPointF newC = reflectPt(arc->center());
            double newStart = reflectAngle(arc->startAngleDegrees() + arc->sweepAngleDegrees());
            reflected = std::make_unique<ArcEntity>(newC, arc->radius(),
                                                    newStart, arc->sweepAngleDegrees());
            break;
        }
        case Entity::Kind::Rectangle: {
            const auto *rect = static_cast<const RectangleEntity*>(e);
            double newAngle = EntityTransform::normAngle(2.0 * axisDeg - rect->angleDegrees());
            reflected = std::make_unique<RectangleEntity>(reflectPt(rect->center()),
                                                          rect->width(), rect->height(), newAngle);
            break;
        }
        case Entity::Kind::Polygon: {
            const auto *poly = static_cast<const PolylineEntity*>(e);
            QList<QPointF> pts;
            pts.reserve(poly->points().size());
            for (const QPointF &pt : poly->points()) pts.append(reflectPt(pt));
            reflected = std::make_unique<PolylineEntity>(pts);
            break;
        }
        default:
            reflected = e->clone();
            break;
        }
        applyProperties(idx, std::move(reflected));
    }
}

// ---------------------------------------------------------------------------
// Align entities
// ---------------------------------------------------------------------------

void GraphicsDesignPanel::applyAlignLeft()
{
    Document *doc = m_canvas->document();
    const auto &ids = doc->selectedIds();
    if (ids.size() < 2) return;

    double target = std::numeric_limits<double>::max();
    for (const QUuid &id : ids) {
        int idx = doc->indexOf(id);
        if (idx >= 0 && doc->entityAt(idx))
            target = std::min(target, doc->entityAt(idx)->boundingRect().left());
    }
    for (const QUuid &id : ids) {
        int idx = doc->indexOf(id);
        if (idx < 0) continue;
        const Entity *e = doc->entityAt(idx);
        if (!e) continue;
        double shift = target - e->boundingRect().left();
        if (qFuzzyIsNull(shift)) continue;
        auto copy = e->clone();
        copy->translate(QPointF(shift, 0.0));
        applyProperties(idx, std::move(copy));
    }
}

void GraphicsDesignPanel::applyAlignCenterH()
{
    Document *doc = m_canvas->document();
    const auto &ids = doc->selectedIds();
    if (ids.size() < 2) return;

    double cx = EntityTransform::selectionCenter(doc).x();
    for (const QUuid &id : ids) {
        int idx = doc->indexOf(id);
        if (idx < 0) continue;
        const Entity *e = doc->entityAt(idx);
        if (!e) continue;
        double shift = cx - e->boundingRect().center().x();
        if (qFuzzyIsNull(shift)) continue;
        auto copy = e->clone();
        copy->translate(QPointF(shift, 0.0));
        applyProperties(idx, std::move(copy));
    }
}

void GraphicsDesignPanel::applyAlignRight()
{
    Document *doc = m_canvas->document();
    const auto &ids = doc->selectedIds();
    if (ids.size() < 2) return;

    double target = -std::numeric_limits<double>::max();
    for (const QUuid &id : ids) {
        int idx = doc->indexOf(id);
        if (idx >= 0 && doc->entityAt(idx))
            target = std::max(target, doc->entityAt(idx)->boundingRect().right());
    }
    for (const QUuid &id : ids) {
        int idx = doc->indexOf(id);
        if (idx < 0) continue;
        const Entity *e = doc->entityAt(idx);
        if (!e) continue;
        double shift = target - e->boundingRect().right();
        if (qFuzzyIsNull(shift)) continue;
        auto copy = e->clone();
        copy->translate(QPointF(shift, 0.0));
        applyProperties(idx, std::move(copy));
    }
}

void GraphicsDesignPanel::applyAlignTop()
{
    Document *doc = m_canvas->document();
    const auto &ids = doc->selectedIds();
    if (ids.size() < 2) return;

    double target = -std::numeric_limits<double>::max();
    for (const QUuid &id : ids) {
        int idx = doc->indexOf(id);
        if (idx >= 0 && doc->entityAt(idx))
            target = std::max(target, doc->entityAt(idx)->boundingRect().top());
    }
    for (const QUuid &id : ids) {
        int idx = doc->indexOf(id);
        if (idx < 0) continue;
        const Entity *e = doc->entityAt(idx);
        if (!e) continue;
        double shift = target - e->boundingRect().top();
        if (qFuzzyIsNull(shift)) continue;
        auto copy = e->clone();
        copy->translate(QPointF(0.0, shift));
        applyProperties(idx, std::move(copy));
    }
}

void GraphicsDesignPanel::applyAlignCenterV()
{
    Document *doc = m_canvas->document();
    const auto &ids = doc->selectedIds();
    if (ids.size() < 2) return;

    double cy = EntityTransform::selectionCenter(doc).y();
    for (const QUuid &id : ids) {
        int idx = doc->indexOf(id);
        if (idx < 0) continue;
        const Entity *e = doc->entityAt(idx);
        if (!e) continue;
        double shift = cy - e->boundingRect().center().y();
        if (qFuzzyIsNull(shift)) continue;
        auto copy = e->clone();
        copy->translate(QPointF(0.0, shift));
        applyProperties(idx, std::move(copy));
    }
}

void GraphicsDesignPanel::applyAlignBottom()
{
    Document *doc = m_canvas->document();
    const auto &ids = doc->selectedIds();
    if (ids.size() < 2) return;

    double target = std::numeric_limits<double>::max();
    for (const QUuid &id : ids) {
        int idx = doc->indexOf(id);
        if (idx >= 0 && doc->entityAt(idx))
            target = std::min(target, doc->entityAt(idx)->boundingRect().bottom());
    }
    for (const QUuid &id : ids) {
        int idx = doc->indexOf(id);
        if (idx < 0) continue;
        const Entity *e = doc->entityAt(idx);
        if (!e) continue;
        double shift = target - e->boundingRect().bottom();
        if (qFuzzyIsNull(shift)) continue;
        auto copy = e->clone();
        copy->translate(QPointF(0.0, shift));
        applyProperties(idx, std::move(copy));
    }
}

// ---------------------------------------------------------------------------
// Measure tool readout
// ---------------------------------------------------------------------------

void GraphicsDesignPanel::onMeasurementChanged()
{
    if (!m_canvas->hasMeasurement()) {
        m_measureDistLabel->setText("—");
        m_measureDxLabel->setText("—");
        m_measureDyLabel->setText("—");
        m_measureAngleLabel->setText("—");
        return;
    }
    m_measureDistLabel->setText(QString::number(m_canvas->measureDistance(), 'f', 3) + " mm");
    m_measureDxLabel->setText(QString::number(m_canvas->measureDeltaX(), 'f', 3));
    m_measureDyLabel->setText(QString::number(m_canvas->measureDeltaY(), 'f', 3));
    m_measureAngleLabel->setText(QString::number(m_canvas->measureAngle(), 'f', 2) + "°");
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

bool GraphicsDesignPanel::eventFilter(QObject *obj, QEvent *event)
{
    // Intercept mouse presses on the layer list viewport so we get the exact
    // QMouseEvent position rather than relying on the potentially-stale QCursor::pos().
    if (obj == m_layerList->viewport() && event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() != Qt::LeftButton)
            return QWidget::eventFilter(obj, event);

        int x   = static_cast<int>(me->position().x());
        int row = m_layerList->indexAt(me->pos()).row();
        Document *doc = m_canvas->document();
        if (row < 0 || row >= doc->layerCount())
            return QWidget::eventFilter(obj, event);

        QListWidgetItem *item = m_layerList->item(row);
        if (!item) return QWidget::eventFilter(obj, event);

        if (x < kEyeColW) {
            // Toggle visibility �?update document directly; layersChanged→refreshLayerList rebuilds item.
            doc->setLayerVisible(row, !doc->layerAt(row).visible);
            m_canvas->update();
            return true;   // consume: no selection change needed
        } else if (x < kIconsW) {
            // Toggle lock.
            doc->setLayerLocked(row, !doc->layerAt(row).locked);
            m_canvas->update();
            return true;
        } else if (x < kSwatchOff + kColorSwW) {
            // Color swatch �?open blocking dialog AFTER reading row (item may be gone after doc->set*).
            QColor current = item->data(Qt::UserRole + 2).value<QColor>();
            QColor chosen  = QColorDialog::getColor(current, this, "Layer Color");
            if (chosen.isValid()) {
                doc->setLayerColor(row, chosen);
                m_canvas->update();
            }
            return true;
        }
        // Fall through for clicks on the text/name area �?let Qt handle selection.
    }
    return QWidget::eventFilter(obj, event);
}

void GraphicsDesignPanel::onLayerItemChanged(QListWidgetItem *item)
{
    // Eye / lock / color are handled directly in the pressed lambda via doc->set*().
    // This handler is only reached for user inline-edits (layer rename) or external
    // setCheckState calls (e.g. Space key).  Read data upfront before any doc mutation
    // because doc->set*() emits layersChanged -> refreshLayerList -> clear() -> item deleted.
    int row = m_layerList->row(item);
    Document *doc = m_canvas->document();
    if (row < 0 || row >= doc->layerCount()) return;

    const bool    visible = (item->checkState() == Qt::Checked);
    const QString name    = item->text().trimmed();
    // Do NOT access `item` after this point.

    doc->setLayerVisible(row, visible);
    if (!name.isEmpty() && name != doc->layerAt(row).name)
        doc->setLayerName(row, name);
    m_canvas->update();
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
