#pragma once

#include <QDialog>
#include <QStringList>

namespace Ui {
class GraphicsDesignDialog;
}

/// Dialog for designing shapes and generating G-code.
class GraphicsDesignDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GraphicsDesignDialog(QWidget *parent = nullptr);
    ~GraphicsDesignDialog();

    struct Options {
        double feedRateXY = 2000.0;
        double plungeRateZ = 1500.0;
        double safeZ = -5.0;
        double cutZ = 5.0;
        int toolMode = 0;  // 0 = PenPlotter, 1 = Laser
    };

signals:
    void gcodeGenerated(const QStringList &lines);

private slots:
    void onToolChanged(int index);
    void onSelectionChanged();
    void updatePreview();
    void onGenerateClicked();
    void onSaveDesignClicked();
    void onLoadDesignClicked();
    void onDeleteSelectedClicked();
    void onClearAllClicked();

private:
    Ui::GraphicsDesignDialog *ui;
    QStringList m_generatedLines;

    void setupConnections();
};