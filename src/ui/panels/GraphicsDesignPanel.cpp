#include "GraphicsDesignPanel.h"
#include "../DesignCanvas.h"
#include "../LineNumberPlainTextEdit.h"
#include "../../grbl/GraphicsDesign/Document.h"
#include "../../grbl/GraphicsDesign/Entity.h"

#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>

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

    // Shape info
    m_shapeInfoLabel = new QLabel("No selection");
    rightLayout->addWidget(m_shapeInfoLabel);

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

    // Bottom buttons
    auto *buttonBar = new QHBoxLayout();
    m_saveBtn = new QPushButton("Save");
    buttonBar->addWidget(m_saveBtn);

    m_loadBtn = new QPushButton("Load");
    buttonBar->addWidget(m_loadBtn);

    m_exportBtn = new QPushButton("Export");
    buttonBar->addWidget(m_exportBtn);

    m_generateBtn = new QPushButton("Generate & Load");
    m_generateBtn->setObjectName("startBtn");
    buttonBar->addWidget(m_generateBtn);

    rightLayout->addLayout(buttonBar);

    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 3);  // Canvas gets 3x stretch
    splitter->setStretchFactor(1, 1);  // Panel gets 1x stretch

    outerLayout->addWidget(splitter);
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
    if (entity) {
        QString typeName;
        switch (entity->kind()) {
            case Entity::Kind::Line: typeName = "Line"; break;
            case Entity::Kind::Rectangle: typeName = "Rectangle"; break;
            case Entity::Kind::Circle: typeName = "Circle"; break;
            case Entity::Kind::Arc: typeName = "Arc"; break;
            case Entity::Kind::Text: typeName = "Text"; break;
            default: typeName = "Entity"; break;
        }
        m_shapeInfoLabel->setText(QString("Selected: %1").arg(typeName));
        m_deleteBtn->setEnabled(true);
    } else {
        m_shapeInfoLabel->setText("No selection");
        m_deleteBtn->setEnabled(false);
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
    m_shapeInfoLabel->setText("No selection");
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