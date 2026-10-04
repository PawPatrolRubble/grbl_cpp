#include "ToolsDialog.h"
#include "../grbl/ToolLibrary.h"
#include "../SettingsManager.h"
#include "../grbl/ToolChangeMacroCompiler.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QScopedValueRollback>
#include <QTabWidget>
#include <QFontDatabase>

ToolsDialog::ToolsDialog(ToolLibrary& library, QWidget* parent)
    : QDialog(parent), m_library(library)
{
    setObjectName("toolsDialog");
    setWindowTitle("Tools and Pen Rack");
    resize(880, 710);
    auto* layout = new QVBoxLayout(this);
    auto* description = new QLabel("Define tools and their preview colors. Pen is the currently supported type. Configure passive pen stations and inspect exchanges in Macro preview.");
    description->setWordWrap(true);
    layout->addWidget(description);

    auto* tabs = new QTabWidget;
    tabs->setObjectName("penRackTabs");
    layout->addWidget(tabs, 1);
    auto* pensPage = new QWidget;
    auto* pensLayout = new QHBoxLayout(pensPage);
    tabs->addTab(pensPage, "Tools and pen stations");
    auto* listColumn = new QVBoxLayout;
    pensLayout->addLayout(listColumn, 1);
    m_list = new QListWidget;
    m_list->setObjectName("toolDefinitionList");
    listColumn->addWidget(m_list);
    auto* buttons = new QHBoxLayout;
    auto* add = new QPushButton("Add tool");
    add->setObjectName("addToolButton");
    m_remove = new QPushButton("Remove tool");
    m_remove->setObjectName("removeToolButton");
    buttons->addWidget(add);
    buttons->addWidget(m_remove);
    listColumn->addLayout(buttons);

    auto* detailsScroll = new QScrollArea;
    detailsScroll->setWidgetResizable(true);
    detailsScroll->setFrameShape(QFrame::NoFrame);
    auto* details = new QWidget;
    auto* detailsLayout = new QVBoxLayout(details);
    detailsScroll->setWidget(details);
    pensLayout->addWidget(detailsScroll, 2);

    auto* form = new QFormLayout;
    m_name = new QLineEdit;
    m_name->setObjectName("toolNameEdit");
    m_name->setMaxLength(120);
    m_color = new QPushButton("Choose color...");
    m_color->setObjectName("toolColorButton");
    m_enabled = new QCheckBox("Available for assignment");
    m_enabled->setObjectName("toolEnabledCheck");
    form->addRow("Name:", m_name);
    auto* typeLabel = new QLabel("Pen");
    typeLabel->setObjectName("toolTypeLabel");
    form->addRow("Tool type:", typeLabel);
    form->addRow("Preview / ink color:", m_color);
    form->addRow(m_enabled);
    detailsLayout->addLayout(form);

    auto* stationForm = new QFormLayout;
    m_stationConfigured = new QCheckBox("Station configured");
    m_stationConfigured->setObjectName("penRackConfiguredCheck");
    stationForm->addRow(m_stationConfigured);
    m_stationCalibrated = new QCheckBox("Passive pickup and return verified on machine");
    m_stationCalibrated->setObjectName("penStationCalibratedCheck");
    m_stationCalibrated->setToolTip("Verify both sequences and shared drawing heights before enabling. Motion edits clear this confirmation.");
    stationForm->addRow(m_stationCalibrated);
    m_slot = new QLineEdit;
    m_slot->setObjectName("penSlotEdit");
    m_slot->setMaxLength(80);
    stationForm->addRow("Unique slot:", m_slot);
    auto coordinate = [](const char* name) {
        auto* spin = new QDoubleSpinBox;
        spin->setObjectName(name);
        spin->setDecimals(3);
        spin->setRange(-100000, 100000);
        spin->setSuffix(" mm");
        return spin;
    };
    m_slotX = coordinate("penSlotX"); m_slotY = coordinate("penSlotY");
    m_returnZ = coordinate("penReturnZ"); m_pickupZ = coordinate("penPickupZ");
    stationForm->addRow("Station X:", m_slotX);
    stationForm->addRow("Station Y:", m_slotY);
    stationForm->addRow("Return contact Z:", m_returnZ);
    stationForm->addRow("Pickup contact Z:", m_pickupZ);
    detailsLayout->addLayout(stationForm);
    auto* macroHelp = new QLabel("Clearance, travel, contact, settling and retraction are generated automatically. Templates add local release/grip steps. Use G53 G0/G1 with explicit axes (and F for G1), or G4 P. Actuator commands are reserved for a later hardware profile.");
    macroHelp->setWordWrap(true);
    detailsLayout->addWidget(macroHelp);
    auto* macros = new QTabWidget;
    m_returnMacro = new QPlainTextEdit;
    m_pickupMacro = new QPlainTextEdit;
    m_returnMacro->setObjectName("penReturnMacro");
    m_pickupMacro->setObjectName("penPickupMacro");
    for (auto* edit : {m_returnMacro, m_pickupMacro}) {
        edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        edit->setMinimumHeight(140);
        edit->setLineWrapMode(QPlainTextEdit::NoWrap);
        edit->setTabChangesFocus(true);
    }
    macros->addTab(m_returnMacro, "Return / release steps");
    macros->addTab(m_pickupMacro, "Pickup / grip steps");
    detailsLayout->addWidget(macros);
    auto* values = new QLabel("Values: {slotX}, {slotY}, {returnZ}, {pickupZ}, {clearanceZ}, {travelFeed}, {dockFeed}, {settleSeconds}. Use ; for comments.");
    values->setWordWrap(true);
    detailsLayout->addWidget(values);

    auto* rackScroll = new QScrollArea;
    rackScroll->setWidgetResizable(true);
    rackScroll->setFrameShape(QFrame::NoFrame);
    auto* rackPage = new QWidget;
    auto* rackLayout = new QVBoxLayout(rackPage);
    rackScroll->setWidget(rackPage);
    tabs->addTab(rackScroll, "Rack settings");
    auto* rackHelp = new QLabel("Enter your machine-coordinate travel bounds and rack clearance. Higher Z means raised. Coordinates are in millimeters; feeds are mm/min. Local steps stay inside the docking radius around each station. Preview assumes the straight route between stations is clear.");
    rackHelp->setWordWrap(true);
    rackLayout->addWidget(rackHelp);
    m_rackConfigured = new QCheckBox("Rack bounds and clearance configured");
    m_rackConfigured->setObjectName("rackConfiguredCheck");
    rackLayout->addWidget(m_rackConfigured);
    m_rackCalibrated = new QCheckBox("Bounds and clear travel at clearance verified on machine");
    m_rackCalibrated->setObjectName("rackCalibratedCheck");
    m_rackCalibrated->setToolTip("Verify straight travel between every used station and the drawing area with a pen loaded. Motion edits clear this confirmation.");
    rackLayout->addWidget(m_rackCalibrated);
    auto* rackForm = new QFormLayout;
    const std::array<const char*, RackFieldCount> labels{"Travel clearance Z:", "XY travel feed:", "Docking Z feed:",
        "Settle time:", "Local docking radius:", "Minimum X:", "Maximum X:", "Minimum Y:", "Maximum Y:", "Minimum Z:", "Maximum Z:"};
    const std::array<const char*, RackFieldCount> names{"rackClearanceZ", "rackTravelFeed", "rackDockFeed",
        "rackSettleSeconds", "rackLocalRadius", "rackMinX", "rackMaxX", "rackMinY", "rackMaxY", "rackMinZ", "rackMaxZ"};
    for (int field = 0; field < RackFieldCount; ++field) {
        auto* spin = coordinate(names[field]);
        if (field == TravelFeed || field == DockFeed) { spin->setRange(0.001, 1000000); spin->setSuffix(" mm/min"); }
        if (field == SettleSeconds) { spin->setRange(0, ToolChangeMacroCompiler::maxDwellSeconds); spin->setSuffix(" s"); }
        if (field == LocalRadius) spin->setRange(0.001, 100000);
        m_rackValues[field] = spin;
        rackForm->addRow(labels[field], spin);
        connect(spin, &QDoubleSpinBox::editingFinished, this, &ToolsDialog::saveRackSettings);
    }
    rackLayout->addLayout(rackForm);
    rackLayout->addStretch();
    connect(m_rackConfigured, &QCheckBox::toggled, this, &ToolsDialog::saveRackSettings);
    connect(m_rackCalibrated, &QCheckBox::toggled, this, &ToolsDialog::saveRackSettings);

    auto* previewPage = new QWidget;
    auto* previewLayout = new QVBoxLayout(previewPage);
    tabs->addTab(previewPage, "Macro preview");
    auto* previewHelp = new QLabel("Preview only. Identify the holder contents and requested pen to inspect the exchange. No machine connection is needed. The final holder position remains at machine clearance; drawing motion is not part of this preview.");
    previewHelp->setWordWrap(true);
    previewLayout->addWidget(previewHelp);
    auto* previewForm = new QFormLayout;
    m_previewCurrent = new QComboBox; m_previewNext = new QComboBox;
    m_previewCurrent->setObjectName("previewCurrentPen"); m_previewNext->setObjectName("previewNextPen");
    m_workCoordinateSystem = new QComboBox;
    m_workCoordinateSystem->setObjectName("previewWorkCoordinateSystem");
    for (int frame = 54; frame <= 59; ++frame) m_workCoordinateSystem->addItem(QString("G%1").arg(frame), frame);
    m_drawingFeed = new QDoubleSpinBox;
    m_drawingFeed->setObjectName("previewDrawingFeed");
    m_drawingFeed->setDecimals(3); m_drawingFeed->setRange(0.001, 1000000);
    m_drawingFeed->setValue(1000); m_drawingFeed->setSuffix(" mm/min");
    previewForm->addRow("Current holder:", m_previewCurrent);
    previewForm->addRow("Requested pen:", m_previewNext);
    previewForm->addRow("Restore work frame:", m_workCoordinateSystem);
    previewForm->addRow("Restore drawing feed:", m_drawingFeed);
    previewLayout->addLayout(previewForm);
    m_previewStatus = new QLabel;
    m_previewStatus->setObjectName("macroPreviewStatus");
    m_previewStatus->setWordWrap(true); m_previewStatus->setTextFormat(Qt::PlainText);
    previewLayout->addWidget(m_previewStatus);
    m_previewText = new QPlainTextEdit;
    m_previewText->setObjectName("toolChangePreviewText");
    m_previewText->setReadOnly(true);
    m_previewText->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_previewText->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    previewLayout->addWidget(m_previewText, 1);
    for (auto* combo : {m_previewCurrent, m_previewNext, m_workCoordinateSystem})
        connect(combo, &QComboBox::currentIndexChanged, this, &ToolsDialog::updatePreview);
    connect(m_drawingFeed, &QDoubleSpinBox::valueChanged, this, &ToolsDialog::updatePreview);

    m_status = new QLabel("Changes save automatically. Automatic passive exchanges require calibrated stations and rack travel. Motion edits clear calibration.");
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    auto* close = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(close);

    connect(add, &QPushButton::clicked, this, [this]() {
        int number = 1;
        QString name;
        bool exists;
        do {
            name = QString("Pen %1").arg(number++);
            exists = false;
            for (const auto& profile : m_library.tools()) exists |= profile.name == name;
        } while (exists);
        const auto id = m_library.addTool(name, Qt::black);
        for (int row = 0; row < m_list->count(); ++row) {
            if (m_list->item(row)->data(Qt::UserRole).toUuid() == id) m_list->setCurrentRow(row);
        }
        m_name->setFocus();
        m_name->selectAll();
    });
    connect(m_remove, &QPushButton::clicked, this, [this]() {
        m_library.removeTool(selectedToolId());
    });
    connect(m_name, &QLineEdit::editingFinished, this, &ToolsDialog::saveSelectedTool);
    connect(m_enabled, &QCheckBox::toggled, this, &ToolsDialog::saveSelectedTool);
    connect(m_stationConfigured, &QCheckBox::toggled, this, &ToolsDialog::saveSelectedTool);
    connect(m_stationCalibrated, &QCheckBox::toggled, this, &ToolsDialog::saveSelectedTool);
    connect(m_slot, &QLineEdit::editingFinished, this, &ToolsDialog::saveSelectedTool);
    for (auto* spin : {m_slotX, m_slotY, m_returnZ, m_pickupZ})
        connect(spin, &QDoubleSpinBox::editingFinished, this, &ToolsDialog::saveSelectedTool);
    for (auto* edit : {m_returnMacro, m_pickupMacro})
        connect(edit, &QPlainTextEdit::textChanged, this, &ToolsDialog::saveSelectedTool);
    connect(m_color, &QPushButton::clicked, this, [this]() {
        const QColor chosen = QColorDialog::getColor(m_color->property("previewColor").value<QColor>(),
                                                     this, "Tool preview / pen ink color");
        if (!chosen.isValid()) return;
        m_color->setProperty("previewColor", chosen);
        saveSelectedTool();
    });
    connect(m_list, &QListWidget::currentRowChanged, this, &ToolsDialog::loadSelectedTool);
    connect(&m_library, &ToolLibrary::changed, this, &ToolsDialog::refreshTools);
    connect(&SettingsManager::instance(), &SettingsManager::settingsSaveFailed, this,
            [this](const QString& error) { m_status->setText("Could not save tools: " + error); });
    refreshTools();
}

QUuid ToolsDialog::selectedToolId() const
{
    return m_list->currentItem() ? m_list->currentItem()->data(Qt::UserRole).toUuid() : QUuid{};
}

void ToolsDialog::refreshTools()
{
    const QUuid selected = selectedToolId();
    const int previousRow = m_list->currentRow();
    const QSignalBlocker blocker(m_list);
    m_list->clear();
    int selectedRow = -1;
    for (const auto& profile : m_library.tools()) {
        QPixmap swatch(14, 14);
        swatch.fill(profile.previewColor);
        auto* item = new QListWidgetItem(QIcon(swatch), profile.name + (profile.enabled ? "" : " (disabled)"));
        item->setData(Qt::UserRole, profile.id);
        m_list->addItem(item);
        if (profile.id == selected) selectedRow = m_list->count() - 1;
    }
    if (selectedRow < 0 && m_list->count() > 0) selectedRow = qBound(0, previousRow, m_list->count() - 1);
    m_list->setCurrentRow(selectedRow);
    if (!m_saving) { loadSelectedTool(); loadRackSettings(); }
    refreshPreviewPens();
}

void ToolsDialog::loadSelectedTool()
{
    const QScopedValueRollback<bool> loading(m_loading, true);
    const auto* profile = m_library.tool(selectedToolId());
    const auto station = m_library.penRackStation(selectedToolId());
    const QSignalBlocker nameBlocker(m_name);
    const QSignalBlocker enabledBlocker(m_enabled);
    m_name->setEnabled(profile);
    m_color->setEnabled(profile);
    m_enabled->setEnabled(profile);
    m_remove->setEnabled(profile);
    m_stationConfigured->setEnabled(profile); m_slot->setEnabled(profile);
    m_stationCalibrated->setEnabled(profile);
    m_stationCalibrated->setChecked(profile && station.motionCalibrated);
    for (auto* spin : {m_slotX, m_slotY, m_returnZ, m_pickupZ}) spin->setEnabled(profile);
    m_returnMacro->setEnabled(profile); m_pickupMacro->setEnabled(profile);
    m_name->setText(profile ? profile->name : QString{});
    m_enabled->setChecked(profile && profile->enabled);
    m_color->setProperty("previewColor", profile ? profile->previewColor : QColor(Qt::black));
    m_stationConfigured->setChecked(profile && station.configured);
    m_slot->setText(profile ? station.slot : QString{});
    m_slotX->setValue(station.slotX); m_slotY->setValue(station.slotY);
    m_returnZ->setValue(station.returnZ); m_pickupZ->setValue(station.pickupZ);
    m_returnMacro->setPlainText(profile ? station.returnMacro : QString{});
    m_pickupMacro->setPlainText(profile ? station.pickupMacro : QString{});
    updateColorButton();
}

void ToolsDialog::saveSelectedTool()
{
    if (m_loading) return;
    const auto* existing = m_library.tool(selectedToolId());
    if (!existing) return;
    auto profile = *existing;
    profile.name = m_name->text().trimmed();
    profile.previewColor = m_color->property("previewColor").value<QColor>();
    profile.enabled = m_enabled->isChecked();
    auto station = m_library.penRackStation(profile.id);
    station.configured = m_stationConfigured->isChecked(); station.slot = m_slot->text().trimmed();
    station.motionCalibrated = m_stationCalibrated->isChecked();
    station.slotX = m_slotX->value(); station.slotY = m_slotY->value();
    station.returnZ = m_returnZ->value(); station.pickupZ = m_pickupZ->value();
    station.returnMacro = m_returnMacro->toPlainText(); station.pickupMacro = m_pickupMacro->toPlainText();
    if (profile.name.isEmpty()) {
        m_status->setText("Enter a tool name. The previous name is still saved.");
        return;
    }
    const QScopedValueRollback<bool> saving(m_saving, true);
    m_library.updateTool(profile);
    m_library.updatePenRackStation(profile.id, station);
    { const QSignalBlocker blocker(m_stationCalibrated); m_stationCalibrated->setChecked(m_library.penRackStation(profile.id).motionCalibrated); }
}

void ToolsDialog::loadRackSettings()
{
    const QScopedValueRollback<bool> loading(m_loading, true);
    const auto& r = m_library.rackSettings();
    m_rackConfigured->setChecked(r.configured);
    m_rackCalibrated->setChecked(r.motionCalibrated);
    const std::array<double, RackFieldCount> values{r.clearanceZ, r.travelFeed, r.dockFeed, r.settleSeconds, r.localRadius,
        r.minX, r.maxX, r.minY, r.maxY, r.minZ, r.maxZ};
    for (int field = 0; field < RackFieldCount; ++field) m_rackValues[field]->setValue(values[field]);
}

void ToolsDialog::saveRackSettings()
{
    if (m_loading) return;
    auto r = m_library.rackSettings();
    r.configured = m_rackConfigured->isChecked();
    r.motionCalibrated = m_rackCalibrated->isChecked();
    r.clearanceZ = m_rackValues[ClearanceZ]->value(); r.travelFeed = m_rackValues[TravelFeed]->value();
    r.dockFeed = m_rackValues[DockFeed]->value(); r.settleSeconds = m_rackValues[SettleSeconds]->value();
    r.localRadius = m_rackValues[LocalRadius]->value();
    r.minX = m_rackValues[MinX]->value(); r.maxX = m_rackValues[MaxX]->value();
    r.minY = m_rackValues[MinY]->value(); r.maxY = m_rackValues[MaxY]->value();
    r.minZ = m_rackValues[MinZ]->value(); r.maxZ = m_rackValues[MaxZ]->value();
    const QScopedValueRollback<bool> saving(m_saving, true);
    m_library.updateRackSettings(r);
    { const QSignalBlocker blocker(m_rackCalibrated); m_rackCalibrated->setChecked(m_library.rackSettings().motionCalibrated); }
}

void ToolsDialog::refreshPreviewPens()
{
    auto fill = [this](QComboBox* combo, const QString& empty, bool next) {
        const auto previous = combo->currentData();
        const QSignalBlocker blocker(combo);
        combo->clear(); combo->addItem(empty, QVariant::fromValue(QUuid{}));
        for (const auto& pen : m_library.tools()) combo->addItem(pen.name + (pen.enabled ? "" : " (disabled)"), QVariant::fromValue(pen.id));
        int index = previous.isValid() ? combo->findData(previous) : (next && combo->count() > 1 ? 1 : 0);
        if (index < 0) { combo->addItem("Missing pen", previous); index = combo->count() - 1; }
        combo->setCurrentIndex(index);
    };
    fill(m_previewCurrent, "Empty holder", false);
    fill(m_previewNext, "Return current pen only", true);
    updatePreview();
}

void ToolsDialog::updatePreview()
{
    const auto result = ToolChangeMacroCompiler::compile(m_library,
        {m_previewCurrent->currentData().toUuid(), m_previewNext->currentData().toUuid(),
         m_workCoordinateSystem->currentData().toInt(), m_drawingFeed->value()});
    m_previewText->setPlainText(result.displayText());
    if (!result.valid()) m_previewStatus->setText(QString("Preview needs configuration: %1 issue(s).").arg(result.errors.size()));
    else if (result.phases.isEmpty()) m_previewStatus->setText("No pen exchange is needed.");
    else m_previewStatus->setText(QString("%1 phases, %2 command lines. Passive motion preview; no commands are sent.")
        .arg(result.phases.size()).arg(result.commandLines().size()));
}

void ToolsDialog::updateColorButton()
{
    const QColor color = m_color->property("previewColor").value<QColor>();
    m_color->setText(color.name());
    m_color->setStyleSheet(QString("background-color: %1; color: %2;")
                              .arg(color.name(), color.lightness() < 128 ? "white" : "black"));
}
