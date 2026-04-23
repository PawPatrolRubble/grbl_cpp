#include "PropertyPage.h"
#include <QDoubleSpinBox>

PropertyPage::PropertyPage(QWidget *parent)
    : QWidget(parent)
{
}

QDoubleSpinBox *PropertyPage::createCoordSpinBox(double rangeMin, double rangeMax, int decimals)
{
    auto *sb = new QDoubleSpinBox();
    sb->setRange(rangeMin, rangeMax);
    sb->setDecimals(decimals);
    sb->setSingleStep(1.0);
    return sb;
}

QDoubleSpinBox *PropertyPage::createAngleSpinBox()
{
    return createCoordSpinBox(-3600.0, 3600.0, 2);
}