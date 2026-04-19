#pragma once

#include <QWidget>

class GraphicsDesignPanel;

/// Graphics design page - full-page wrapper for GraphicsDesignPanel.
/// Opens as a closable tab when user creates/opens a design.
class GraphicsDesignPage : public QWidget
{
    Q_OBJECT

public:
    explicit GraphicsDesignPage(QWidget *parent = nullptr);

    GraphicsDesignPanel *designPanel() const { return m_panel; }

signals:
    void gcodeGenerated(const QStringList &lines);

private:
    GraphicsDesignPanel *m_panel;
};