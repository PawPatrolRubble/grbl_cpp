#include "EmptyPage.h"
#include "../../../grbl/GraphicsDesign/Entity.h"

#include <QVBoxLayout>
#include <QLabel>

EmptyPage::EmptyPage(QWidget *parent)
    : PropertyPage(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    m_label = new QLabel("No selection");
    m_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_label, 1);
}

void EmptyPage::bind(const Entity *entity)
{
    // EmptyPage never binds to an entity
    // Just update the message based on selection state
}

std::unique_ptr<Entity> EmptyPage::apply() const
{
    // EmptyPage never applies changes
    return nullptr;
}

int EmptyPage::entityType() const
{
    // No entity type - this is a placeholder
    return -1;
}

void EmptyPage::setMessage(const QString &message)
{
    m_label->setText(message);
}