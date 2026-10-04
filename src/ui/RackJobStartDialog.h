#pragma once
#include <QDialog>
#include <QUuid>
class ToolLibrary;
struct PlotJob;
class QComboBox;

// Makes physical assumptions explicit without sending any machine commands.
class RackJobStartDialog : public QDialog {
    Q_OBJECT
public:
    RackJobStartDialog(const PlotJob& job, const ToolLibrary& tools, QWidget* parent = nullptr);
    QUuid holderToolId() const;
private:
    QComboBox* m_holder;
};
