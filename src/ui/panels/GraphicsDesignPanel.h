#pragma once

#include <QWidget>
#include <QStringList>

class DesignCanvas;
class QComboBox;
class QPushButton;
class QLabel;
class QDoubleSpinBox;
class LineNumberPlainTextEdit;

/// Panel for designing shapes and generating G-code.
/// Dockable widget that integrates into the main window.
class GraphicsDesignPanel : public QWidget
{
    Q_OBJECT

public:
    explicit GraphicsDesignPanel(QWidget *parent = nullptr);
    ~GraphicsDesignPanel() = default;

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
    void onExportGcodeClicked();

private:
    QStringList m_generatedLines;

    // UI components
    DesignCanvas *m_canvas;
    QComboBox *m_toolCombo;
    QComboBox *m_toolModeCombo;
    QPushButton *m_deleteBtn;
    QPushButton *m_clearBtn;
    QPushButton *m_saveBtn;
    QPushButton *m_loadBtn;
    QPushButton *m_exportBtn;
    QPushButton *m_generateBtn;
    QLabel *m_shapeInfoLabel;
    QLabel *m_lineCountLabel;
    QLabel *m_statusLabel;
    QDoubleSpinBox *m_feedSpeedSpinBox;
    QDoubleSpinBox *m_plungeSpeedSpinBox;
    QDoubleSpinBox *m_fastHeightSpinBox;
    QDoubleSpinBox *m_workHeightSpinBox;
    LineNumberPlainTextEdit *m_gcodePreview;

    void setupUi();
    void setupConnections();
};