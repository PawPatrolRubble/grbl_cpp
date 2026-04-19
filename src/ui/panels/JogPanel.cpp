#include "JogPanel.h"
#include "ui_JogPanel.h"
#include <QRegularExpressionValidator>
#include <QKeyEvent>

JogPanel::JogPanel(QWidget *parent)
    : QWidget(parent), ui(new Ui::JogPanel)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_StyledBackground, true);

    QRegularExpression rx(R"(^-?\d*\.?\d*$)");
    ui->absXEdit->setValidator(new QRegularExpressionValidator(rx, ui->absXEdit));
    ui->absYEdit->setValidator(new QRegularExpressionValidator(rx, ui->absYEdit));
    ui->absZEdit->setValidator(new QRegularExpressionValidator(rx, ui->absZEdit));

    connect(ui->jogStepSlider, &QSlider::valueChanged, this, [this](int val) {
        double step = val / 10.0;
        ui->jogStepLabel->setText(QString("%1 mm").arg(step, 0, 'f', 1));
    });

    connect(ui->jogModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        m_jogMode = static_cast<JogMode>(idx);
    });

    auto setupJogBtn = [this](QPushButton *btn, const QString &axis, int dir) {
        connect(btn, &QPushButton::pressed, this, [this, axis, dir]() {
            doJog(axis, dir);
        });
        connect(btn, &QPushButton::released, this, [this]() {
            if (m_jogMode == JogMode::Continuous)
                emit jogCancelRequested();
        });
    };

    setupJogBtn(ui->btnXPlus, "X", 1);
    setupJogBtn(ui->btnXMinus, "X", -1);
    setupJogBtn(ui->btnYPlus, "Y", 1);
    setupJogBtn(ui->btnYMinus, "Y", -1);
    setupJogBtn(ui->btnZPlus, "Z", 1);
    setupJogBtn(ui->btnZMinus, "Z", -1);

    connect(ui->goToAbsBtn, &QPushButton::clicked, this, [this]() {
        double x = ui->absXEdit->text().toDouble();
        double y = ui->absYEdit->text().toDouble();
        double z = ui->absZEdit->text().toDouble();
        double feed = ui->jogFeedSpin->value();
        emit goToAbsoluteRequested(x, y, z, feed);
    });

    connect(ui->goToXBtn, &QPushButton::clicked, this, [this]() {
        double x = ui->absXEdit->text().toDouble();
        double feed = ui->jogFeedSpin->value();
        emit goToXRequested(x, feed);
    });

    connect(ui->goToYBtn, &QPushButton::clicked, this, [this]() {
        double y = ui->absYEdit->text().toDouble();
        double feed = ui->jogFeedSpin->value();
        emit goToYRequested(y, feed);
    });

    connect(ui->goToZBtn, &QPushButton::clicked, this, [this]() {
        double z = ui->absZEdit->text().toDouble();
        double feed = ui->jogFeedSpin->value();
        emit goToZRequested(z, feed);
    });
}

JogPanel::~JogPanel()
{
    delete ui;
}

void JogPanel::doJog(const QString &axis, int direction)
{
    double feed = ui->jogFeedSpin->value();
    if (m_jogMode == JogMode::Continuous) {
        emit jogContinuousRequested(axis, direction, feed);
    } else {
        double step = ui->jogStepSlider->value() / 10.0;
        double signedStep = (direction >= 0) ? step : -step;
        emit jogStepRequested(axis, signedStep, feed);
    }
}

bool JogPanel::handleKeyPress(QKeyEvent *event)
{
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
    ui->btnXPlus->setEnabled(enabled);
    ui->btnXMinus->setEnabled(enabled);
    ui->btnYPlus->setEnabled(enabled);
    ui->btnYMinus->setEnabled(enabled);
    ui->btnZPlus->setEnabled(enabled);
    ui->btnZMinus->setEnabled(enabled);
    ui->goToAbsBtn->setEnabled(enabled);
    ui->goToXBtn->setEnabled(enabled);
    ui->goToYBtn->setEnabled(enabled);
    ui->goToZBtn->setEnabled(enabled);
}