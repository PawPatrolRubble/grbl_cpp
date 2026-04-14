#include "GraphicsDesignDialog.h"
#include "ui_GraphicsDesignDialog.h"
#include "DesignCanvas.h"
#include "LineNumberPlainTextEdit.h"
#include "../grbl/GraphicsDesign/Document.h"
#include "../grbl/GraphicsDesign/Entity.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>

GraphicsDesignDialog::GraphicsDesignDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::GraphicsDesignDialog)
{
    ui->setupUi(this);

    // Set monospace font for G-code preview
    ui->gcodePreview->setFont(QFont("Consolas", 10));
    ui->gcodePreview->setStyleSheet(
        "QPlainTextEdit {"
        "  background-color: #ffffff;"
        "  border: 1px solid #e5e7eb;"
        "}"
    );

    // Tool mode options
    ui->toolModeCombo->addItem("Pen Plotter");
    ui->toolModeCombo->addItem("Laser");

    setupConnections();

    // Initial preview
    updatePreview();
}

GraphicsDesignDialog::~GraphicsDesignDialog()
{
    delete ui;
}

void GraphicsDesignDialog::setupConnections()
{
    // Tool selection
    connect(ui->toolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GraphicsDesignDialog::onToolChanged);

    // Document changes
    connect(ui->designCanvas, &DesignCanvas::documentStateChanged,
            this, &GraphicsDesignDialog::updatePreview);
    connect(ui->designCanvas, &DesignCanvas::selectedEntityChanged,
            this, &GraphicsDesignDialog::onSelectionChanged);

    // Buttons
    connect(ui->previewButton, &QPushButton::clicked, this, &GraphicsDesignDialog::updatePreview);
    connect(ui->generateButton, &QPushButton::clicked, this, &GraphicsDesignDialog::onGenerateClicked);
    connect(ui->saveButton, &QPushButton::clicked, this, &GraphicsDesignDialog::onSaveDesignClicked);
    connect(ui->loadButton, &QPushButton::clicked, this, &GraphicsDesignDialog::onLoadDesignClicked);
    connect(ui->deleteButton, &QPushButton::clicked, this, &GraphicsDesignDialog::onDeleteSelectedClicked);
    connect(ui->clearButton, &QPushButton::clicked, this, &GraphicsDesignDialog::onClearAllClicked);
    connect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

void GraphicsDesignDialog::onToolChanged(int index)
{
    DesignCanvas::ToolType tool = static_cast<DesignCanvas::ToolType>(index);
    ui->designCanvas->setTool(tool);
}

void GraphicsDesignDialog::onSelectionChanged()
{
    const Entity *entity = ui->designCanvas->selectedEntity();
    if (entity) {
        QString typeName;
        switch (entity->kind()) {
            case Entity::Kind::Line: typeName = "Line"; break;
            case Entity::Kind::Rectangle: typeName = "Rectangle"; break;
            case Entity::Kind::Circle: typeName = "Circle"; break;
            case Entity::Kind::Arc: typeName = "Arc"; break;
            default: typeName = "Entity"; break;
        }
        ui->shapeInfoLabel->setText(QString("Selected: %1").arg(typeName));
        ui->deleteButton->setEnabled(true);
    } else {
        ui->shapeInfoLabel->setText("No selection");
        ui->deleteButton->setEnabled(false);
    }
}

void GraphicsDesignDialog::updatePreview()
{
    Document *doc = ui->designCanvas->document();
    m_generatedLines = doc->generateGcode(
        ui->feedSpeedSpinBox->value(),
        ui->plungeSpeedSpinBox->value(),
        ui->fastHeightSpinBox->value(),
        ui->workHeightSpinBox->value());

    QString previewText = QStringList(m_generatedLines.mid(0, 100)).join("\n");
    if (m_generatedLines.size() > 100) {
        previewText += "\n... (more lines)";
    }
    ui->gcodePreview->setPlainText(previewText);
    ui->lineCountLabel->setText(QString("%1 entities, %2 G-code lines")
                                .arg(doc->entityCount())
                                .arg(m_generatedLines.size()));
}

void GraphicsDesignDialog::onGenerateClicked()
{
    if (m_generatedLines.isEmpty() || m_generatedLines.size() < 3) {
        QMessageBox::warning(this, "No Design",
            "Please create some shapes before generating G-code.");
        return;
    }

    emit gcodeGenerated(m_generatedLines);
    accept();
}

void GraphicsDesignDialog::onSaveDesignClicked()
{
    QString file = QFileDialog::getSaveFileName(this, "Save Design",
        QString(), "Design Files (*.json)");
    if (!file.isEmpty()) {
        QJsonDocument doc = ui->designCanvas->document()->toJson();
        QFile f(file);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(doc.toJson());
            f.close();
            ui->statusLabel->setText("Design saved.");
        } else {
            QMessageBox::warning(this, "Save Failed", "Could not save file.");
        }
    }
}

void GraphicsDesignDialog::onLoadDesignClicked()
{
    QString file = QFileDialog::getOpenFileName(this, "Load Design",
        QString(), "Design Files (*.json)");
    if (!file.isEmpty()) {
        QFile f(file);
        if (f.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            f.close();
            ui->designCanvas->document()->loadFromJson(doc);
            ui->statusLabel->setText("Design loaded.");
            updatePreview();
        } else {
            QMessageBox::warning(this, "Load Failed", "Could not load file.");
        }
    }
}

void GraphicsDesignDialog::onDeleteSelectedClicked()
{
    ui->designCanvas->deleteSelectedEntity();
    updatePreview();
}

void GraphicsDesignDialog::onClearAllClicked()
{
    ui->designCanvas->clearDocument();
    updatePreview();
    ui->shapeInfoLabel->setText("No selection");
}