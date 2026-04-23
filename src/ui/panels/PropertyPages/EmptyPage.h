#pragma once

#include "PropertyPage.h"

class QLabel;

/// Placeholder page for "No selection" state.
/// Does not handle any entity type - used as default page in stack.
class EmptyPage : public PropertyPage
{
    Q_OBJECT

public:
    explicit EmptyPage(QWidget *parent = nullptr);

    void bind(const Entity *entity) override;
    std::unique_ptr<Entity> apply() const override;
    int entityType() const override;

    void setMessage(const QString &message);

private:
    QLabel *m_label;
};