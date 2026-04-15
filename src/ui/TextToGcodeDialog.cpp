#include "TextToGcodeDialog.h"
#include "ui_TextToGcodeDialog.h"
#include "LineNumberPlainTextEdit.h"
#include "grbl/TextToGcode/VectorFontLoader.h"
#include "grbl/TextToGcode/TextToGcodeGenerator.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QCoreApplication>
#include <QFontDatabase>

TextToGcodeDialog::TextToGcodeDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TextToGcodeDialog)
{
    ui->setupUi(this);

    connect(ui->browseButton, &QPushButton::clicked, this, &TextToGcodeDialog::onBrowseClicked);
    connect(ui->generateButton, &QPushButton::clicked, this, &TextToGcodeDialog::onGenerateClicked);
    connect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(ui->previewButton, &QPushButton::clicked, this, &TextToGcodeDialog::updatePreview);

    connect(ui->fontSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        bool isSystem = (static_cast<FontSource>(index) == FontSource::SystemFont);
        ui->fontFamilyCombo->setVisible(isSystem);
        ui->fontPathInput->setVisible(!isSystem);
        ui->browseButton->setVisible(!isSystem);
    });

    ui->fontFamilyCombo->addItems(QFontDatabase::families());

    ui->fontSourceCombo->setCurrentIndex(static_cast<int>(FontSource::SystemFont));
    ui->fontPathInput->setVisible(false);
    ui->browseButton->setVisible(false);

    // Update text statistics on input change
    connect(ui->textInput, &QPlainTextEdit::textChanged, this, [this]() {
        QString text = ui->textInput->toPlainText();
        int lineCount = text.isEmpty() ? 0 : text.split('\n').size();
        int charCount = text.length();
        ui->textStatsLabel->setText(QString("%1 lines, %2 characters").arg(lineCount).arg(charCount));
    });
    ui->textStatsLabel->setText("0 lines, 0 characters");
}

TextToGcodeDialog::~TextToGcodeDialog()
{
    delete ui;
}

void TextToGcodeDialog::onBrowseClicked()
{
    QString dir = QCoreApplication::applicationDirPath() + "/../resources/fonts";
    QString file = QFileDialog::getOpenFileName(this, "Select Vector Font", dir,
        "Fonts (*.json *.svg *.jhf *.glf)");
    if (!file.isEmpty()) {
        ui->fontPathInput->setText(file);
    }
}

void TextToGcodeDialog::updatePreview()
{
    QString text = ui->textInput->toPlainText();
    FontSource fontSource = static_cast<FontSource>(ui->fontSourceCombo->currentIndex());
    QString fontPath = ui->fontPathInput->text();

    if (text.isEmpty()) {
        ui->gcodePreview->clear();
        return;
    }

    if (fontSource == FontSource::CustomVector && fontPath.isEmpty()) {
        ui->gcodePreview->clear();
        return;
    }

    TextToGcodeGenerator::Options options;
    options.fontSize = ui->sizeSpinBox->value();
    options.toolMode = (ui->toolModeCombo->currentIndex() == 0)
                        ? TextToGcodeGenerator::ToolMode::PenPlotter
                        : TextToGcodeGenerator::ToolMode::Laser;
    options.feedRateXY = ui->feedSpeedSpinBox->value();
    options.plungeRateZ = ui->plungeSpeedSpinBox->value();
    options.safeZ = ui->fastHeightSpinBox->value();
    options.cutZ = ui->workHeightSpinBox->value();

    TextToGcodeGenerator generator;

    if (fontSource == FontSource::SystemFont) {
        QFont font(ui->fontFamilyCombo->currentText());
        font.setPointSizeF(options.fontSize);
        m_generatedLines = generator.generateFromSystemFont(text, font, options);
    } else {
        FontLoadResult loadResult = VectorFontLoader::loadFont(fontPath);
        if (!loadResult.success) {
            ui->gcodePreview->setPlainText("; Error: " + loadResult.errorMessage);
            m_generatedLines.clear();
            return;
        }
        m_generatedLines = generator.generate(text, loadResult.glyphs, options);
    }

    QString previewText = QStringList(m_generatedLines.mid(0, 100)).join("\n");
    if (m_generatedLines.size() > 100) {
        previewText += "\n... (more lines)";
    }
    ui->gcodePreview->setPlainText(previewText);
}

void TextToGcodeDialog::onGenerateClicked()
{
    updatePreview();
    bool hasError = !m_generatedLines.isEmpty()
                    && m_generatedLines.first().startsWith("; Error");
    if (!m_generatedLines.isEmpty() && !hasError) {
        emit gcodeGenerated(m_generatedLines);
        accept();
    } else {
        QMessageBox::warning(this, "Generation Failed",
            "Could not generate valid G-code.");
    }
}