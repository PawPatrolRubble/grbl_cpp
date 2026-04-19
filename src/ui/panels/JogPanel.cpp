#include "JogPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QKeyEvent>
#include <QLineEdit>
#include <QGroupBox>
#include <QRegularExpressionValidator>

JogPanel::JogPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("jogPanel");
    setAttribute(Qt::WA_StyledBackground, true);
    auto *jogLayout = new QVBoxLayout(this);
    jogLayout->setContentsMargins(8, 8, 8, 8);

    // Mode / Step row
    auto *modeRow = new QHBoxLayout();
    modeRow->addWidget(new QLabel("Mode:"));
    m_jogModeCombo = new QComboBox();
    m_jogModeCombo->addItems({"Continuous", "Step"});
    m_jogModeCombo->setCurrentIndex(0);
    modeRow->addWidget(m_jogModeCombo);

    modeRow->addWidget(new QLabel("Step:"));
    m_jogStepSlider = new QSlider(Qt::Horizontal);
    m_jogStepSlider->setRange(1, 100);
    m_jogStepSlider->setValue(10);
    m_jogStepLabel = new QLabel("1.0 mm");
    modeRow->addWidget(m_jogStepSlider);
    modeRow->addWidget(m_jogStepLabel);
    jogLayout->addLayout(modeRow);

    connect(m_jogStepSlider, &QSlider::valueChanged, this, [this](int val) {
        double step = val / 10.0;
        m_jogStepLabel->setText(QString("%1 mm").arg(step, 0, 'f', 1));
    });

    connect(m_jogModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        m_jogMode = static_cast<JogMode>(idx);
    });

    // Feed row
    auto *feedRow = new QHBoxLayout();
    feedRow->addWidget(new QLabel("Feed:"));
    m_jogFeedSpin = new QDoubleSpinBox();
    m_jogFeedSpin->setRange(1, 30000);
    m_jogFeedSpin->setValue(1000);
    m_jogFeedSpin->setSuffix(" mm/min");
    m_jogFeedSpin->setDecimals(0);
    feedRow->addWidget(m_jogFeedSpin);
    jogLayout->addLayout(feedRow);

    // Directional pad
    auto *grid = new QGridLayout();
    auto *btnYPlus = new QPushButton("Y+");
    auto *btnYMinus = new QPushButton("Y-");
    auto *btnXMinus = new QPushButton("X-");
    auto *btnXPlus = new QPushButton("X+");
    auto *btnZPlus = new QPushButton("Z+");
    auto *btnZMinus = new QPushButton("Z-");

    grid->addWidget(btnYPlus, 0, 1);
    grid->addWidget(btnXMinus, 1, 0);
    grid->addWidget(btnXPlus, 1, 2);
    grid->addWidget(btnYMinus, 2, 1);
    grid->addWidget(btnZPlus, 0, 3);
    grid->addWidget(btnZMinus, 2, 3);
    jogLayout->addLayout(grid);

    m_jogButtons = {btnXPlus, btnXMinus, btnYPlus, btnYMinus, btnZPlus, btnZMinus};

    // Wire directional buttons
    auto setupJogBtn = [this](QPushButton *btn, const QString &axis, int dir) {
        connect(btn, &QPushButton::pressed, this, [this, axis, dir]() {
            doJog(axis, dir);
        });
        connect(btn, &QPushButton::released, this, [this]() {
            if (m_jogMode == JogMode::Continuous)
                emit jogCancelRequested();
        });
    };

    setupJogBtn(btnXPlus, "X", 1);
    setupJogBtn(btnXMinus, "X", -1);
    setupJogBtn(btnYPlus, "Y", 1);
    setupJogBtn(btnYMinus, "Y", -1);
    setupJogBtn(btnZPlus, "Z", 1);
    setupJogBtn(btnZMinus, "Z", -1);

    // --- Absolute movement section ---
    auto *absGroup = new QGroupBox("Go To Position");
    auto *absLayout = new QGridLayout(absGroup);
    absLayout->setContentsMargins(4, 4, 4, 4);

    auto makeEdit = [](const QString &placeholder) {
        auto *edit = new QLineEdit();
        edit->setPlaceholderText(placeholder);
        edit->setMaximumWidth(100);
        // Only allow numeric input (decimals, negatives)
        QRegularExpression rx(R"(^-?\d*\.?\d*$)");
        edit->setValidator(new QRegularExpressionValidator(rx, edit));
        return edit;
    };

    absLayout->addWidget(new QLabel("X:"), 0, 0);
    m_absXEdit = makeEdit("0.000");
    absLayout->addWidget(m_absXEdit, 0, 1);

    absLayout->addWidget(new QLabel("Y:"), 0, 2);
    m_absYEdit = makeEdit("0.000");
    absLayout->addWidget(m_absYEdit, 0, 3);

    absLayout->addWidget(new QLabel("Z:"), 0, 4);
    m_absZEdit = makeEdit("0.000");
    absLayout->addWidget(m_absZEdit, 0, 5);

    m_goToAbsBtn = new QPushButton("Go To");
    m_goToAbsBtn->setToolTip("Move to absolute work position (G90 G0)");
    absLayout->addWidget(m_goToAbsBtn, 1, 0, 1, 6);
    m_jogButtons.append(m_goToAbsBtn);  // respect setMotionEnabled()

    jogLayout->addWidget(absGroup);

    connect(m_goToAbsBtn, &QPushButton::clicked, this, [this]() {
        double x = m_absXEdit->text().toDouble();
        double y = m_absYEdit->text().toDouble();
        double z = m_absZEdit->text().toDouble();
        double feed = m_jogFeedSpin->value();
        emit goToAbsoluteRequested(x, y, z, feed);
    });
}

void JogPanel::doJog(const QString &axis, int direction)
{
    double feed = m_jogFeedSpin->value();
    if (m_jogMode == JogMode::Continuous) {
        emit jogContinuousRequested(axis, direction, feed);
    } else {
        double step = m_jogStepSlider->value() / 10.0;
        double signedStep = (direction >= 0) ? step : -step;
        emit jogStepRequested(axis, signedStep, feed);
    }
}

bool JogPanel::handleKeyPress(QKeyEvent *event)
{
    // In continuous mode, only act on the initial key-down — not auto-repeat.
    // Auto-repeat would flood GRBL's 128-byte RX buffer with $J commands faster
    // than the jog-cancel (0x85) can purge them, causing the machine to keep
    // moving after key release.
    if (event->isAutoRepeat())
        return true;

    switch (event->key()) {
    case Qt::Key_Right:  doJog("X",  1); return true;
    case Qt::Key_Left:   doJog("X", -1); return true;
    case Qt::Key_Up:     doJog("Y",  1); return true;
    case Qt::Key_Down:   doJog("Y", -1); return true;
    case Qt::Key_PageUp: doJog("Z",  1); return true;
    case Qt::Key_PageDown: doJog("Z", -1); return true;
    default: return false;
    }
}

bool JogPanel::handleKeyRelease(QKeyEvent *event)
{
    if (m_jogMode != JogMode::Continuous)
        return false;

    switch (event->key()) {
    case Qt::Key_Right:
    case Qt::Key_Left:
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
        emit jogCancelRequested();
        return true;
    default:
        return false;
    }
}

void JogPanel::setMotionEnabled(bool enabled)
{
    for (auto *btn : m_jogButtons)
        btn->setEnabled(enabled);
}