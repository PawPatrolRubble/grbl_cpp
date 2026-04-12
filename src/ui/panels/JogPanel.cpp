#include "JogPanel.h"
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QKeyEvent>

JogPanel::JogPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *group = new QGroupBox("Jog Control");
    auto *jogLayout = new QVBoxLayout(group);

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

    // Outer layout
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(group);
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
