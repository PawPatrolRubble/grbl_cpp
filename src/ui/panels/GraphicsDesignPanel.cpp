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
#include "../../grbl/DxfParser.h"

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

GraphicsDesignPanel::GraphicsDesignPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("graphicsDesignPanel");
    setupUi();
    setupConnections();
    updatePreview();
}

void GraphicsDesignPanel::setupUi()
{
    auto *outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    // ---- Left: Canvas ----
    m_canvas = new DesignCanvas();
    m_canvas->setMinimumWidth(300);
    m_canvas->setMinimumHeight(200);
    splitter->addWidget(m_canvas);

    // ---- Right: Operations panel ----
    auto *rightPanel = new QWidget();
    rightPanel->setMinimumWidth(220);
    rightPanel->setMaximumWidth(380);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(8, 8, 8, 8);
    rightLayout->setSpacing(6);

    // Tool bar row
    auto *toolBar = new QHBoxLayout();
    m_toolCombo = new QComboBox();
    m_toolCombo->addItem("Select");
    m_toolCombo->addItem("Line");
    m_toolCombo->addItem("Rectangle");
    m_toolCombo->addItem("Circle");
    m_toolCombo->addItem("Arc");
    m_toolCombo->addItem("Text");
    toolBar->addWidget(m_toolCombo);

    m_snapBtn = new QToolButton();
    m_snapBtn->setText("Snap");
    m_snapBtn->setCheckable(true);
    m_snapBtn->setChecked(false);
    m_snapBtn->setToolTip("Toggle grid/entity snap (S)");
    toolBar->addWidget(m_snapBtn);
    toolBar->addStretch();
    rightLayout->addLayout(toolBar);

    // Action buttons row
    auto *actionRow = new QHBoxLayout();
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

    // Options grid
    auto *optionsGrid = new QGridLayout();
    optionsGrid->setSpacing(8);

    int row = 0;
    optionsGrid->addWidget(new QLabel("Tool Mode:"), row, 0);
    m_toolModeCombo = new QComboBox();
    m_toolModeCombo->addItem("Pen Plotter");
    m_toolModeCombo->addItem("Laser");
    optionsGrid->addWidget(m_toolModeCombo, row, 1);

    row++;
    optionsGrid->addWidget(new QLabel("Feed Speed:"), row, 0);
    m_feedSpeedSpinBox = new QDoubleSpinBox();
    m_feedSpeedSpinBox->setRange(0, 10000);
    m_feedSpeedSpinBox->setValue(2000);
    optionsGrid->addWidget(m_feedSpeedSpinBox, row, 1);

    row++;
    optionsGrid->addWidget(new QLabel("Plunge Speed:"), row, 0);
    m_plungeSpeedSpinBox = new QDoubleSpinBox();
    m_plungeSpeedSpinBox->setRange(0, 10000);
    m_plungeSpeedSpinBox->setValue(1500);
    optionsGrid->addWidget(m_plungeSpeedSpinBox, row, 1);

    row++;
    optionsGrid->addWidget(new QLabel("Safe Z:"), row, 0);
    m_fastHeightSpinBox = new QDoubleSpinBox();
    m_fastHeightSpinBox->setRange(-100, 100);
    m_fastHeightSpinBox->setValue(-5);
    optionsGrid->addWidget(m_fastHeightSpinBox, row, 1);

    row++;
    optionsGrid->addWidget(new QLabel("Work Z:"), row, 0);
    m_workHeightSpinBox = new QDoubleSpinBox();
    m_workHeightSpinBox->setRange(-100, 100);
    m_workHeightSpinBox->setValue(5);
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
    m_saveBtn = new QPushButton("Save");
    buttonBar1->addWidget(m_saveBtn);

    m_loadBtn = new QPushButton("Load");
    buttonBar1->addWidget(m_loadBtn);

    m_exportBtn = new QPushButton("Export");
    buttonBar1->addWidget(m_exportBtn);

    m_generateBtn = new QPushButton("Generate & Load");
    m_generateBtn->setObjectName("startBtn");
    buttonBar1->addWidget(m_generateBtn);

    rightLayout->addLayout(buttonBar1);

    auto *buttonBar2 = new QHBoxLayout();
    auto *importImgBtn = new QPushButton("Import Image");
    connect(importImgBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::onImportImageClicked);
    buttonBar2->addWidget(importImgBtn);

    auto *importDxfBtn = new QPushButton("Import DXF");
    connect(importDxfBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::onImportDxfClicked);
    buttonBar2->addWidget(importDxfBtn);

    buttonBar2->addStretch();

    rightLayout->addLayout(buttonBar2);

    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 3);  // Canvas gets 3x stretch
    splitter->setStretchFactor(1, 1);  // Panel gets 1x stretch

    outerLayout->addWidget(splitter);
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

    m_textEdit = new QLineEdit();
    layout->addRow("Text:", m_textEdit);

    m_textFontSize = new QDoubleSpinBox();
    m_textFontSize->setRange(1, 100000);
    m_textFontSize->setDecimals(2);
    layout->addRow("Font Size:", m_textFontSize);

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
        QString txt = m_textEdit->text();
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
    connect(m_textEdit, &QLineEdit::textChanged, this, detectCJK);

    m_propertyStack->addWidget(page);
}

void GraphicsDesignPanel::setupConnections()
{
    // Tool selection
    connect(m_toolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GraphicsDesignPanel::onToolChanged);

    // Sync combo when canvas changes tool internally (e.g. auto-select after draw)
    connect(m_canvas, &DesignCanvas::toolChanged, this, &GraphicsDesignPanel::onCanvasToolChanged);

    // Snap toggle
    connect(m_snapBtn, &QToolButton::toggled, m_canvas, &DesignCanvas::setSnapEnabled);

    // Undo/Redo buttons
    auto *undoBtn = findChild<QPushButton*>("undoBtn");
    auto *redoBtn = findChild<QPushButton*>("redoBtn");
    if (undoBtn) connect(undoBtn, &QPushButton::clicked, m_canvas, &DesignCanvas::undo);
    if (redoBtn) connect(redoBtn, &QPushButton::clicked, m_canvas, &DesignCanvas::redo);

    // Document changes
    connect(m_canvas, &DesignCanvas::documentStateChanged, this, &GraphicsDesignPanel::updatePreview);
    connect(m_canvas, &DesignCanvas::selectedEntityChanged, this, &GraphicsDesignPanel::onSelectionChanged);

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
    connect(m_textEdit, &QLineEdit::textChanged,
            this, &GraphicsDesignPanel::applyTextProperties);
    connect(m_textFontSize, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
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
}

void GraphicsDesignPanel::onToolChanged(int index)
{
    DesignCanvas::ToolType tool = static_cast<DesignCanvas::ToolType>(index);
    m_canvas->setTool(tool);
}

void GraphicsDesignPanel::onCanvasToolChanged()
{
    // Sync combo box when canvas changes tool internally (e.g. auto-select after drawing)
    int index = static_cast<int>(m_canvas->currentToolType());
    if (m_toolCombo->currentIndex() != index) {
        m_toolCombo->blockSignals(true);
        m_toolCombo->setCurrentIndex(index);
        m_toolCombo->blockSignals(false);
    }
}

void GraphicsDesignPanel::onSelectionChanged()
{
    const Entity *entity = m_canvas->selectedEntity();

    m_updatingUi = true;

    if (!entity) {
        m_propertyStack->setCurrentIndex(0);
        m_emptyPageLabel->setText("No selection");
        m_deleteBtn->setEnabled(false);
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
            m_textPosX->setValue(text->position().x());
            m_textPosY->setValue(text->position().y());
            m_textEdit->blockSignals(true);
            m_textEdit->setText(text->text());
            m_textEdit->blockSignals(false);
            m_textFontSize->setValue(text->fontSize());

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
    default:
        m_propertyStack->setCurrentIndex(0);
        m_emptyPageLabel->setText("Unknown entity type");
        break;
    }

    m_updatingUi = false;
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
    m_lineCountLabel->setText(QString("%1 entities, %2 G-code lines")
                              .arg(doc->entityCount())
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
    previewLabel->setStyleSheet("background: #222; border: 1px solid #555;");
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
        m_textEdit->text(),
        m_textFontSize->value());
    newText->setLetterSpacing(text->letterSpacing());
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