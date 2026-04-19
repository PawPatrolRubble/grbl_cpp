#include "SettingsPanel.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QHeaderView>
#include <QColorDialog>
#include <QScrollArea>

static void setButtonColor(QPushButton *btn, const QColor &c)
{
    btn->setStyleSheet(
        QString("background-color: rgba(%1,%2,%3,%4); border: 1px solid #666; border-radius: 3px; min-width: 40px; min-height: 20px;")
            .arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha()));
    btn->setProperty("_color", QVariant::fromValue(c));
}

static QColor buttonColor(QPushButton *btn)
{
    return btn->property("_color").value<QColor>();
}

SettingsPanel::SettingsPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("settingsPanel");
    setAttribute(Qt::WA_StyledBackground, true);

    // Scrollable content
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget();
    auto *mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // =====================================================================
    // Canvas Settings section
    // =====================================================================
    auto *canvasLabel = new QLabel("Canvas Settings");
    canvasLabel->setObjectName("sectionTitle");
    mainLayout->addWidget(canvasLabel);

    auto *canvasForm = new QFormLayout();

    m_paperColorBtn = new QPushButton();
    setButtonColor(m_paperColorBtn, SettingsManager::CanvasSettings{}.paperColor);
    connect(m_paperColorBtn, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(buttonColor(m_paperColorBtn), this, "Paper Color",
                                          QColorDialog::ShowAlphaChannel);
        if (c.isValid()) { setButtonColor(m_paperColorBtn, c); emitCanvasChanged(); }
    });
    canvasForm->addRow("Paper Color:", m_paperColorBtn);

    m_showRuledLines = new QCheckBox("Show ruled lines");
    m_showRuledLines->setChecked(true);
    connect(m_showRuledLines, &QCheckBox::toggled, this, &SettingsPanel::emitCanvasChanged);
    canvasForm->addRow(m_showRuledLines);

    m_lineSpacing = new QDoubleSpinBox();
    m_lineSpacing->setRange(1.0, 100.0);
    m_lineSpacing->setDecimals(1);
    m_lineSpacing->setSuffix(" mm");
    m_lineSpacing->setValue(8.0);
    connect(m_lineSpacing, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SettingsPanel::emitCanvasChanged);
    canvasForm->addRow("Line Spacing:", m_lineSpacing);

    m_lineColorBtn = new QPushButton();
    setButtonColor(m_lineColorBtn, SettingsManager::CanvasSettings{}.lineColor);
    connect(m_lineColorBtn, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(buttonColor(m_lineColorBtn), this, "Line Color",
                                          QColorDialog::ShowAlphaChannel);
        if (c.isValid()) { setButtonColor(m_lineColorBtn, c); emitCanvasChanged(); }
    });
    canvasForm->addRow("Line Color:", m_lineColorBtn);

    m_showMarginLine = new QCheckBox("Show margin line");
    m_showMarginLine->setChecked(true);
    connect(m_showMarginLine, &QCheckBox::toggled, this, &SettingsPanel::emitCanvasChanged);
    canvasForm->addRow(m_showMarginLine);

    m_marginOffset = new QDoubleSpinBox();
    m_marginOffset->setRange(0.0, 200.0);
    m_marginOffset->setDecimals(1);
    m_marginOffset->setSuffix(" mm");
    m_marginOffset->setValue(10.0);
    connect(m_marginOffset, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SettingsPanel::emitCanvasChanged);
    canvasForm->addRow("Margin Offset:", m_marginOffset);

    m_marginColorBtn = new QPushButton();
    setButtonColor(m_marginColorBtn, SettingsManager::CanvasSettings{}.marginColor);
    connect(m_marginColorBtn, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(buttonColor(m_marginColorBtn), this, "Margin Color",
                                          QColorDialog::ShowAlphaChannel);
        if (c.isValid()) { setButtonColor(m_marginColorBtn, c); emitCanvasChanged(); }
    });
    canvasForm->addRow("Margin Color:", m_marginColorBtn);

    mainLayout->addLayout(canvasForm);

    // =====================================================================
    // Jog Defaults section
    // =====================================================================
    auto *jogLabel = new QLabel("Jog Defaults");
    jogLabel->setObjectName("sectionTitle");
    mainLayout->addWidget(jogLabel);

    auto *form = new QFormLayout();

    m_jogStepSpin = new QDoubleSpinBox();
    m_jogStepSpin->setRange(0.001, 999.0);
    m_jogStepSpin->setDecimals(3);
    m_jogStepSpin->setSuffix(" mm");
    m_jogStepSpin->setValue(1.0);
    form->addRow("Step Size:", m_jogStepSpin);

    m_jogFeedSpin = new QDoubleSpinBox();
    m_jogFeedSpin->setRange(1, 10000);
    m_jogFeedSpin->setDecimals(0);
    m_jogFeedSpin->setSuffix(" mm/min");
    m_jogFeedSpin->setValue(500);
    form->addRow("Feed Rate:", m_jogFeedSpin);

    mainLayout->addLayout(form);

    // GRBL Settings section
    auto *grblLabel = new QLabel("GRBL Settings");
    grblLabel->setObjectName("sectionTitle");
    mainLayout->addWidget(grblLabel);

    auto *refreshBtn = new QPushButton("Refresh ($$)");
    connect(refreshBtn, &QPushButton::clicked, this, &SettingsPanel::grblSettingsRefreshRequested);
    mainLayout->addWidget(refreshBtn);

    m_grblTable = new QTableWidget(0, 3);
    m_grblTable->setHorizontalHeaderLabels({"Setting", "Value", "Description"});
    m_grblTable->horizontalHeader()->setStretchLastSection(true);
    m_grblTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_grblTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_grblTable->verticalHeader()->setVisible(false);
    m_grblTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_grblTable->setEditTriggers(QAbstractItemView::DoubleClicked);
    mainLayout->addWidget(m_grblTable);

    connect(m_grblTable, &QTableWidget::cellChanged, this, [this](int row, int col) {
        if (col != 1) return;
        auto *idItem = m_grblTable->item(row, 0);
        auto *valItem = m_grblTable->item(row, 1);
        if (!idItem || !valItem) return;
        bool ok = false;
        int id = idItem->text().mid(1).toInt(&ok);
        if (!ok) return;
        double value = valItem->text().toDouble(&ok);
        if (!ok) return;
        emit grblSettingWriteRequested(id, value);
    });

    connect(m_jogStepSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this]() {
                emit jogDefaultsChanged(m_jogStepSpin->value(), m_jogFeedSpin->value());
            });
    connect(m_jogFeedSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this]() {
                emit jogDefaultsChanged(m_jogStepSpin->value(), m_jogFeedSpin->value());
            });

    // =====================================================================
    // Save button
    // =====================================================================
    mainLayout->addSpacing(12);
    auto *saveBtn = new QPushButton("Save Settings");
    saveBtn->setObjectName("saveSettingsBtn");
    connect(saveBtn, &QPushButton::clicked, this, [this]() {
        auto &sm = SettingsManager::instance();
        sm.setCanvasSettings(currentCanvasSettings());
        sm.setJogStepSize(m_jogStepSpin->value());
        sm.setJogFeedRate(m_jogFeedSpin->value());
        sm.save();
    });
    mainLayout->addWidget(saveBtn);

    mainLayout->addStretch();
    scroll->setWidget(content);
    outerLayout->addWidget(scroll);

    // Load initial values from settings singleton
    auto &sm = SettingsManager::instance();
    applyCanvasSettings(sm.canvasSettings());
    m_jogStepSpin->setValue(sm.jogStepSize());
    m_jogFeedSpin->setValue(sm.jogFeedRate());
}

double SettingsPanel::jogStepSize() const
{
    return m_jogStepSpin->value();
}

double SettingsPanel::jogFeedRate() const
{
    return m_jogFeedSpin->value();
}

void SettingsPanel::setGrblSettings(const QMap<int, double> &settings)
{
    populateGrblTable(settings);
}

void SettingsPanel::populateGrblTable(const QMap<int, double> &settings)
{
    m_grblTable->blockSignals(true);
    m_grblTable->setRowCount(0);
    m_grblTable->setRowCount(settings.size());

    int row = 0;
    for (auto it = settings.constBegin(); it != settings.constEnd(); ++it) {
        auto *idItem = new QTableWidgetItem(QString("$%1").arg(it.key()));
        idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
        m_grblTable->setItem(row, 0, idItem);

        auto *valItem = new QTableWidgetItem(QString::number(it.value(), 'f', 3));
        m_grblTable->setItem(row, 1, valItem);

        auto *descItem = new QTableWidgetItem(grblSettingDescription(it.key()));
        descItem->setFlags(descItem->flags() & ~Qt::ItemIsEditable);
        m_grblTable->setItem(row, 2, descItem);

        ++row;
    }
    m_grblTable->blockSignals(false);
}

QString SettingsPanel::grblSettingDescription(int id)
{
    static const QMap<int, QString> desc = {
        {0, "Step pulse (µs)"},
        {1, "Step idle delay (ms)"},
        {2, "Step port invert mask"},
        {3, "Direction port invert mask"},
        {4, "Step enable invert"},
        {5, "Limit pins invert"},
        {6, "Probe pin invert"},
        {10, "Status report mask"},
        {11, "Junction deviation (mm)"},
        {12, "Arc tolerance (mm)"},
        {13, "Report inches (bool)"},
        {20, "Soft limits (bool)"},
        {21, "Hard limits (bool)"},
        {22, "Homing cycle (bool)"},
        {23, "Homing dir invert mask"},
        {24, "Homing feed (mm/min)"},
        {25, "Homing seek (mm/min)"},
        {26, "Homing debounce (ms)"},
        {27, "Homing pull-off (mm)"},
        {30, "Max spindle speed (RPM)"},
        {31, "Min spindle speed (RPM)"},
        {32, "Laser mode (bool)"},
        {100, "X steps/mm"},
        {101, "Y steps/mm"},
        {102, "Z steps/mm"},
        {110, "X max rate (mm/min)"},
        {111, "Y max rate (mm/min)"},
        {112, "Z max rate (mm/min)"},
        {120, "X acceleration (mm/s²)"},
        {121, "Y acceleration (mm/s²)"},
        {122, "Z acceleration (mm/s²)"},
        {130, "X max travel (mm)"},
        {131, "Y max travel (mm)"},
        {132, "Z max travel (mm)"},
    };
    return desc.value(id, "");
}

void SettingsPanel::applyCanvasSettings(const SettingsManager::CanvasSettings &s)
{
    m_showRuledLines->blockSignals(true);
    m_lineSpacing->blockSignals(true);
    m_showMarginLine->blockSignals(true);
    m_marginOffset->blockSignals(true);

    m_showRuledLines->setChecked(s.showRuledLines);
    m_lineSpacing->setValue(s.lineSpacing);
    setButtonColor(m_lineColorBtn, s.lineColor);
    m_showMarginLine->setChecked(s.showMarginLine);
    m_marginOffset->setValue(s.marginOffset);
    setButtonColor(m_marginColorBtn, s.marginColor);
    setButtonColor(m_paperColorBtn, s.paperColor);

    m_showRuledLines->blockSignals(false);
    m_lineSpacing->blockSignals(false);
    m_showMarginLine->blockSignals(false);
    m_marginOffset->blockSignals(false);
}

SettingsManager::CanvasSettings SettingsPanel::currentCanvasSettings() const
{
    SettingsManager::CanvasSettings s;
    s.showRuledLines = m_showRuledLines->isChecked();
    s.lineSpacing    = m_lineSpacing->value();
    s.lineColor      = buttonColor(m_lineColorBtn);
    s.showMarginLine = m_showMarginLine->isChecked();
    s.marginOffset   = m_marginOffset->value();
    s.marginColor    = buttonColor(m_marginColorBtn);
    s.paperColor     = buttonColor(m_paperColorBtn);
    return s;
}

void SettingsPanel::emitCanvasChanged()
{
    emit canvasSettingsChanged(currentCanvasSettings());
}