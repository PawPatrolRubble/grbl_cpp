#include "TextTool.h"

#include "../TextEntity.h"

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QFileDialog>
#include <QApplication>
#include <QWidget>
#include <memory>

TextTool::TextTool(Document& document, CommandStack& commandStack)
    : Tool(document, commandStack)
{
}

QString TextTool::name() const
{
    return "Text";
}

QString TextTool::instructions() const
{
    return "Click to place text.";
}

void TextTool::mousePress(const QPointF& pos)
{
    m_placing = true;
    m_currentPoint = pos;

    QWidget *parent = QApplication::activeWindow();

    // Built-in vector font catalog (resource path → display name)
    struct BuiltinFont { QString path; QString name; };
    static const QList<BuiltinFont> builtinFonts = {
        {":/fonts/fonts/futural.jhf",    "Sans Serif Light"},
        {":/fonts/fonts/futuram.jhf",    "Sans Serif Medium"},
        {":/fonts/fonts/rowmans.jhf",    "Serif Simplex"},
        {":/fonts/fonts/rowmand.jhf",    "Serif Duplex"},
        {":/fonts/fonts/timesr.jhf",     "Times Roman"},
        {":/fonts/fonts/timesi.jhf",     "Times Italic"},
        {":/fonts/fonts/scripts.jhf",    "Script Simplex"},
        {":/fonts/fonts/scriptc.jhf",    "Script Complex"},
        {":/fonts/fonts/cursive.jhf",    "Cursive"},
        {":/fonts/fonts/gothiceng.jhf",  "Gothic English"},
    };

    // Build text placement dialog
    QDialog dialog(parent);
    dialog.setWindowTitle("Add Text");
    dialog.setMinimumWidth(380);

    auto *layout = new QVBoxLayout(&dialog);

    // Text input (multi-line)
    layout->addWidget(new QLabel("Text:"));
    auto *textEdit = new QPlainTextEdit();
    textEdit->setMinimumHeight(80);
    textEdit->setPlaceholderText("Enter text (supports multiple lines)");
    layout->addWidget(textEdit);

    // Font size
    auto *sizeRow = new QHBoxLayout();
    sizeRow->addWidget(new QLabel("Font Size:"));
    auto *sizeSpinBox = new QDoubleSpinBox();
    sizeSpinBox->setRange(1.0, 200.0);
    sizeSpinBox->setValue(m_fontSize);
    sizeSpinBox->setSuffix(" mm");
    sizeRow->addWidget(sizeSpinBox);
    layout->addLayout(sizeRow);

    // Font mode selection
    auto *modeRow = new QHBoxLayout();
    modeRow->addWidget(new QLabel("Font Type:"));
    auto *modeCombo = new QComboBox();
    modeCombo->addItem("Single-Stroke (CNC)");
    modeCombo->addItem("System Font (Outline)");
    modeCombo->setCurrentIndex(m_useVectorFont ? 0 : 1);
    modeRow->addWidget(modeCombo);
    layout->addLayout(modeRow);

    // Vector font selection (built-in combo + custom browse)
    auto *vectorFontRow = new QHBoxLayout();
    vectorFontRow->addWidget(new QLabel("CNC Font:"));
    auto *vectorFontCombo = new QComboBox();
    int selectedVecIdx = 0;
    for (int i = 0; i < builtinFonts.size(); ++i) {
        vectorFontCombo->addItem(builtinFonts[i].name, builtinFonts[i].path);
        if (builtinFonts[i].path == m_vectorFontPath)
            selectedVecIdx = i;
    }
    vectorFontCombo->addItem("Custom file...");
    vectorFontCombo->setCurrentIndex(selectedVecIdx);
    vectorFontRow->addWidget(vectorFontCombo);
    auto *vectorFontWidget = new QWidget();
    vectorFontWidget->setLayout(vectorFontRow);
    layout->addWidget(vectorFontWidget);

    // Custom font file path (hidden unless "Custom file..." is selected)
    auto *customRow = new QHBoxLayout();
    auto *fontPathEdit = new QLineEdit(m_vectorFontPath);
    fontPathEdit->setReadOnly(true);
    fontPathEdit->setPlaceholderText("No file selected");
    customRow->addWidget(fontPathEdit);
    auto *browseBtn = new QPushButton("Browse...");
    customRow->addWidget(browseBtn);
    auto *customWidget = new QWidget();
    customWidget->setLayout(customRow);
    layout->addWidget(customWidget);

    // System font selection — add label and combo separately for proper dropdown behavior
    auto *systemFontLabel = new QLabel("System Font:");
    layout->addWidget(systemFontLabel);
    auto *fontCombo = new QFontComboBox();
    fontCombo->setCurrentFont(QFont(m_fontFamily));
    fontCombo->setMinimumWidth(250);
    layout->addWidget(fontCombo);

    // Show/hide based on font mode and vector combo selection
    auto updateVisibility = [&]() {
        bool isVector = (modeCombo->currentIndex() == 0);
        vectorFontWidget->setVisible(isVector);
        customWidget->setVisible(isVector && vectorFontCombo->currentIndex() == builtinFonts.size());
        systemFontLabel->setVisible(!isVector);
        fontCombo->setVisible(!isVector);
    };
    updateVisibility();
    QObject::connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     [&](int) { updateVisibility(); });
    QObject::connect(vectorFontCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     [&](int) { updateVisibility(); });

    // Auto-detect CJK: switch to system font mode when Chinese/Japanese text is typed
    auto detectCJK = [&]() {
        QString txt = textEdit->toPlainText();
        bool hasCJK = false;
        for (const QChar& c : txt) {
            ushort u = c.unicode();
            if ((u >= 0x4E00 && u <= 0x9FFF) || (u >= 0x3400 && u <= 0x4DBF) ||
                (u >= 0x3000 && u <= 0x303F) || (u >= 0xFF00 && u <= 0xFFEF) ||
                (u >= 0x3040 && u <= 0x30FF)) {
                hasCJK = true;
                break;
            }
        }
        if (hasCJK) {
            if (modeCombo->currentIndex() == 0) {
                modeCombo->setCurrentIndex(1); // Switch to System Font
            }
            // Filter font combo to show only CJK-capable fonts
            fontCombo->setWritingSystem(QFontDatabase::SimplifiedChinese);
            // Select a good default if current font doesn't support CJK
            QFont testFont(fontCombo->currentFont());
            QFontMetrics fm(testFont);
            if (!fm.inFont(QChar(0x4E2D))) {
                fontCombo->setCurrentFont(QFont("Microsoft YaHei"));
            }
        } else {
            // Reset to show all fonts
            fontCombo->setWritingSystem(QFontDatabase::Any);
        }
    };
    QObject::connect(textEdit, &QPlainTextEdit::textChanged, detectCJK);

    // Browse for custom vector font file
    QObject::connect(browseBtn, &QPushButton::clicked, [&]() {
        QString path = QFileDialog::getOpenFileName(
            &dialog, "Select Vector Font",
            QString(),
            "Vector Fonts (*.jhf *.svg *.hpgl *.glf *.json);;All Files (*)");
        if (!path.isEmpty()) {
            fontPathEdit->setText(path);
        }
    });

    // OK / Cancel buttons
    auto *buttonRow = new QHBoxLayout();
    buttonRow->addStretch();
    auto *okBtn = new QPushButton("OK");
    auto *cancelBtn = new QPushButton("Cancel");
    buttonRow->addWidget(okBtn);
    buttonRow->addWidget(cancelBtn);
    layout->addLayout(buttonRow);

    QObject::connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    QObject::connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    m_placing = false;

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString text = textEdit->toPlainText().trimmed();
    if (text.isEmpty()) return;

    // Save settings for next placement
    m_fontSize = sizeSpinBox->value();
    m_useVectorFont = (modeCombo->currentIndex() == 0);
    m_fontFamily = fontCombo->currentFont().family();

    // Determine vector font path
    int vecIdx = vectorFontCombo->currentIndex();
    if (vecIdx < builtinFonts.size()) {
        m_vectorFontPath = builtinFonts[vecIdx].path;
    } else {
        m_vectorFontPath = fontPathEdit->text();
    }

    // Auto-detect CJK characters: fall back to system font if vector mode selected
    if (m_useVectorFont) {
        bool hasCJK = false;
        for (const QChar& c : text) {
            ushort u = c.unicode();
            if ((u >= 0x4E00 && u <= 0x9FFF) ||   // CJK Unified Ideographs
                (u >= 0x3400 && u <= 0x4DBF) ||   // CJK Extension A
                (u >= 0x3000 && u <= 0x303F) ||   // CJK Symbols
                (u >= 0xFF00 && u <= 0xFFEF) ||   // Fullwidth Forms
                (u >= 0x3040 && u <= 0x30FF)) {   // Hiragana + Katakana
                hasCJK = true;
                break;
            }
        }
        if (hasCJK) {
            m_useVectorFont = false;
            // Pick a CJK-capable font if current isn't one
            QFont testFont(m_fontFamily);
            QFontMetrics fm(testFont);
            if (!fm.inFont(QChar(0x4E2D))) { // Test with '中'
                m_fontFamily = "Microsoft YaHei";
            }
        }
    }

    // Create the entity
    auto entity = std::make_unique<TextEntity>(pos, text, m_fontSize);
    if (m_useVectorFont && !m_vectorFontPath.isEmpty()) {
        entity->setVectorFont(m_vectorFontPath);
    } else {
        entity->setFontFamily(m_fontFamily);
    }

    commandStack().execute(std::make_unique<AddEntityCommand>(std::move(entity)));
}

void TextTool::mouseMove(const QPointF& pos)
{
    m_currentPoint = pos;
}

void TextTool::mouseRelease(const QPointF& pos)
{
    // Text tool uses press only
}

void TextTool::cancel()
{
    m_placing = false;
    m_currentPoint = QPointF();
}

void TextTool::paintPreview(QPainter& painter) const
{
    if (!m_placing) return;

    const double size = 10.0;
    painter.drawLine(QPointF(m_currentPoint.x() - size, m_currentPoint.y()),
                     QPointF(m_currentPoint.x() + size, m_currentPoint.y()));
    painter.drawLine(QPointF(m_currentPoint.x(), m_currentPoint.y() - size),
                     QPointF(m_currentPoint.x(), m_currentPoint.y() + size));
}

bool TextTool::isActive() const
{
    return m_placing;
}
