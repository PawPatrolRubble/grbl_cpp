#include "RackJobStartDialog.h"
#include "plotting/PlotJob.h"
#include "grbl/ToolLibrary.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QVBoxLayout>

RackJobStartDialog::RackJobStartDialog(const PlotJob& job, const ToolLibrary& tools, QWidget* parent)
    : QDialog(parent), m_holder(new QComboBox)
{
    setObjectName("rackJobStartDialog");
    setWindowTitle("Start automatic pen job");
    auto* layout = new QVBoxLayout(this);
    auto label = [&](const QString& text) {
        auto* result = new QLabel(text); result->setWordWrap(true); result->setTextFormat(Qt::PlainText);
        layout->addWidget(result);
    };
    QStringList sequence;
    for (const auto& id : job.penSequence) if (const auto* pen = tools.tool(id)) sequence << pen->name;
    label("Pen order: " + sequence.join(" → "));
    label(job.returnFinalPen ? "Finish: return the final pen; holder empty at rack clearance."
                             : "Finish: keep the final pen raised at rack clearance.");
    label("After an interrupted exchange, inspect the holder and every used slot before confirming. This passive rack has no pickup sensor.");
    label("What is physically in the holder now?");
    m_holder->setObjectName("confirmedHolderCombo");
    m_holder->addItem("Choose holder contents...");
    m_holder->addItem("Empty holder", QVariant::fromValue(QUuid{}));
    for (const auto& pen : tools.tools()) m_holder->addItem(pen.name, QVariant::fromValue(pen.id));
    layout->addWidget(m_holder);
    const auto* initial = tools.tool(job.initialToolId);
    label("This compiled job expects: " + (initial ? initial->name : QString("empty holder"))
          + ". If different, change Initial holder in Design and regenerate.");
    QStringList stationLines;
    QSet<QUuid> listed;
    auto addSlot = [&](const QUuid& id) {
        if (listed.contains(id)) return;
        listed.insert(id);
        if (const auto* pen = tools.tool(id))
            stationLines << tools.penRackStation(id).slot + " — " + pen->name + (id == job.initialToolId ? ": empty, ready for return" : ": pen present, ready for pickup");
    };
    if (!job.initialToolId.isNull()) addSlot(job.initialToolId);
    for (const auto& id : job.penSequence) addSlot(id);
    label("Required rack slots:\n" + stationLines.join('\n'));
    auto* homed = new QCheckBox("Machine has been homed since its last reset/disconnect");
    auto* occupancy = new QCheckBox("Holder and required rack slots match the conditions above");
    auto* heights = new QCheckBox("Shared drawing/pen-up heights and clear travel suit every pen in this job");
    homed->setObjectName("rackStartHomedCheck"); occupancy->setObjectName("rackStartOccupancyCheck");
    heights->setObjectName("rackStartHeightsCheck");
    for (auto* check : {homed, occupancy, heights}) layout->addWidget(check);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto* start = buttons->button(QDialogButtonBox::Ok);
    start->setObjectName("rackStartButton"); start->setText("Start complete job");
    layout->addWidget(buttons);
    const auto update = [this, start, homed, occupancy, heights, expectedTool = job.initialToolId] {
        start->setEnabled(homed->isChecked() && occupancy->isChecked() && heights->isChecked()
                          && m_holder->currentData().isValid() && holderToolId() == expectedTool);
    };
    for (auto* check : {homed, occupancy, heights}) connect(check, &QCheckBox::toggled, this, update);
    connect(m_holder, &QComboBox::currentIndexChanged, this, update);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    update();
}
QUuid RackJobStartDialog::holderToolId() const { return m_holder->currentData().toUuid(); }
