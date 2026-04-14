#include "GraphicsDesignPanel.h"
#include "../DesignCanvas.h"
#include "../LineNumberPlainTextEdit.h"
#include "../../grbl/GraphicsDesign/Document.h"
#include "../../grbl/GraphicsDesign/Entity.h"

#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
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
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // Tool bar
    auto *toolBar = new QHBoxLayout();
    m_toolCombo = new QComboBox();
    m_toolCombo->addItem("Select");
    m_toolCombo->addItem("Line");
    m_toolCombo->addItem("Rectangle");
    m_toolCombo->addItem("Circle");
    m_toolCombo->addItem("Arc");
    toolBar->addWidget(m_toolCombo);

    m_deleteBtn = new QPushButton("Delete");
    m_deleteBtn->setEnabled(false);
    toolBar->addWidget(m_deleteBtn);

    m_clearBtn = new QPushButton("Clear All");
    toolBar->addWidget(m_clearBtn);

    m_shapeInfoLabel = new QLabel("No selection");
    toolBar->addWidget(m_shapeInfoLabel);

    toolBar->addStretch();
    mainLayout->addLayout(toolBar);

    // Canvas
    m_canvas = new DesignCanvas();
    m_canvas->setMinimumHeight(200);
    mainLayout->addWidget(m_canvas, 1);

    // Options grid
    auto *optionsGrid = new QGridLayout();
    optionsGrid->setSpacing(8);

    int row = 0;
    optionsGrid->addWidget(new QLabel("Tool Mode:"), row, 0);
    m_toolModeCombo = new QComboBox();
    m_toolModeCombo->addItem("Pen Plotter");
    m_toolModeCombo->addItem("Laser");
    optionsGrid->addWidget(m_toolModeCombo, row, 1);

    optionsGrid->addWidget(new QLabel("Feed Speed:"), row, 2);
    m_feedSpeedSpinBox = new QDoubleSpinBox();
    m_feedSpeedSpinBox->setRange(0, 10000);
    m_feedSpeedSpinBox->setValue(2000);
    optionsGrid->addWidget(m_feedSpeedSpinBox, row, 3);

    row++;
    optionsGrid->addWidget(new QLabel("Plunge Speed:"), row, 0);
    m_plungeSpeedSpinBox = new QDoubleSpinBox();
    m_plungeSpeedSpinBox->setRange(0, 10000);
    m_plungeSpeedSpinBox->setValue(1500);
    optionsGrid->addWidget(m_plungeSpeedSpinBox, row, 1);

    optionsGrid->addWidget(new QLabel("Safe Z:"), row, 2);
    m_fastHeightSpinBox = new QDoubleSpinBox();
    m_fastHeightSpinBox->setRange(-100, 100);
    m_fastHeightSpinBox->setValue(-5);
    optionsGrid->addWidget(m_fastHeightSpinBox, row, 3);

    row++;
    optionsGrid->addWidget(new QLabel("Work Z:"), row, 0);
    m_workHeightSpinBox = new QDoubleSpinBox();
    m_workHeightSpinBox->setRange(-100, 100);
    m_workHeightSpinBox->setValue(5);
    optionsGrid->addWidget(m_workHeightSpinBox, row, 1);

    mainLayout->addLayout(optionsGrid);

    // G-code preview
    mainLayout->addWidget(new QLabel("G-code Preview:"));
    m_gcodePreview = new LineNumberPlainTextEdit();
    m_gcodePreview->setReadOnly(true);
    m_gcodePreview->setMaximumHeight(100);
    m_gcodePreview->setFont(QFont("Consolas", 10));
    mainLayout->addWidget(m_gcodePreview);

    // Status line
    m_lineCountLabel = new QLabel("0 entities, 0 G-code lines");
    m_lineCountLabel->setAlignment(Qt::AlignRight);
    mainLayout->addWidget(m_lineCountLabel);

    m_statusLabel = new QLabel();
    mainLayout->addWidget(m_statusLabel);

    // Bottom buttons
    auto *buttonBar = new QHBoxLayout();
    m_saveBtn = new QPushButton("Save Design");
    buttonBar->addWidget(m_saveBtn);

    m_loadBtn = new QPushButton("Load Design");
    buttonBar->addWidget(m_loadBtn);

    buttonBar->addStretch();

    m_generateBtn = new QPushButton("Generate & Load");
    m_generateBtn->setObjectName("startBtn");
    buttonBar->addWidget(m_generateBtn);

    mainLayout->addLayout(buttonBar);
}

void GraphicsDesignPanel::setupConnections()
{
    // Tool selection
    connect(m_toolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GraphicsDesignPanel::onToolChanged);

    // Document changes
    connect(m_canvas, &DesignCanvas::documentStateChanged, this, &GraphicsDesignPanel::updatePreview);
    connect(m_canvas, &DesignCanvas::selectedEntityChanged, this, &GraphicsDesignPanel::onSelectionChanged);

    // Buttons
    connect(m_deleteBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::onDeleteSelectedClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::onClearAllClicked);
    connect(m_saveBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::onSaveDesignClicked);
    connect(m_loadBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::onLoadDesignClicked);
    connect(m_generateBtn, &QPushButton::clicked, this, &GraphicsDesignPanel::onGenerateClicked);
}

void GraphicsDesignPanel::onToolChanged(int index)
{
    DesignCanvas::ToolType tool = static_cast<DesignCanvas::ToolType>(index);
    m_canvas->setTool(tool);
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