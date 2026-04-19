#pragma once

#include <QWidget>
#include <QStringList>
#include <memory>

class DesignCanvas;
class QCheckBox;
class QComboBox;
class QFontComboBox;
class QPushButton;
class QToolButton;
class QLabel;
class QDoubleSpinBox;
class QStackedWidget;
class QLineEdit;
class LineNumberPlainTextEdit;
class Entity;

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
        int toolMode = 0;
    };

signals:
    void gcodeGenerated(const QStringList &lines);

private slots:
    void onToolChanged(int index);
    void onCanvasToolChanged();
    void onSelectionChanged();
    void updatePreview();
    void onGenerateClicked();
    void onSaveDesignClicked();
    void onLoadDesignClicked();
    void onDeleteSelectedClicked();
    void onClearAllClicked();
    void onExportGcodeClicked();
    void onImportImageClicked();
    void onImportDxfClicked();
    void applyLineProperties();
    void applyRectProperties();
    void applyCircleProperties();
    void applyArcProperties();
    void applyTextProperties();

private:
    QStringList m_generatedLines;

    DesignCanvas *m_canvas;
    QComboBox *m_toolCombo;
    QComboBox *m_toolModeCombo;
    QPushButton *m_deleteBtn;
    QPushButton *m_clearBtn;
    QPushButton *m_saveBtn;
    QPushButton *m_loadBtn;
    QPushButton *m_exportBtn;
    QPushButton *m_generateBtn;
    QToolButton *m_snapBtn;
    QLabel *m_lineCountLabel;
    QLabel *m_statusLabel;
    QDoubleSpinBox *m_feedSpeedSpinBox;
    QDoubleSpinBox *m_plungeSpeedSpinBox;
    QDoubleSpinBox *m_fastHeightSpinBox;
    QDoubleSpinBox *m_workHeightSpinBox;
    LineNumberPlainTextEdit *m_gcodePreview;

    QStackedWidget *m_propertyStack;
    bool m_updatingUi = false;

    QLabel *m_emptyPageLabel;

    QDoubleSpinBox *m_lineCenterX;
    QDoubleSpinBox *m_lineCenterY;
    QDoubleSpinBox *m_lineLength;
    QDoubleSpinBox *m_lineAngle;

    QDoubleSpinBox *m_rectCenterX;
    QDoubleSpinBox *m_rectCenterY;
    QDoubleSpinBox *m_rectWidth;
    QDoubleSpinBox *m_rectHeight;
    QDoubleSpinBox *m_rectAngle;

    QDoubleSpinBox *m_circleCenterX;
    QDoubleSpinBox *m_circleCenterY;
    QDoubleSpinBox *m_circleRadius;

    QDoubleSpinBox *m_arcCenterX;
    QDoubleSpinBox *m_arcCenterY;
    QDoubleSpinBox *m_arcRadius;
    QDoubleSpinBox *m_arcStartAngle;
    QDoubleSpinBox *m_arcSweepAngle;

    QDoubleSpinBox *m_textPosX;
    QDoubleSpinBox *m_textPosY;
    QLineEdit *m_textEdit;
    QDoubleSpinBox *m_textFontSize;
    QComboBox *m_textFontModeCombo;
    QComboBox *m_textVectorFontCombo;
    QFontComboBox *m_textSystemFontCombo;
    QWidget *m_textVectorFontWidget;
    QWidget *m_textSystemFontWidget;
    QLineEdit *m_textCustomFontPath;
    QPushButton *m_textBrowseFontBtn;
    QCheckBox *m_textSingleStrokeCheck;
    QWidget *m_textSingleStrokeParams;
    QDoubleSpinBox *m_textSsResolution;
    QDoubleSpinBox *m_textSsSmoothing;
    QDoubleSpinBox *m_textSsCleanup;
    QDoubleSpinBox *m_textSsMergeGap;
    QDoubleSpinBox *m_textSsMinSegLen;

    void setupUi();
    void setupConnections();
    void buildEmptyPage();
    void buildLinePage();
    void buildRectPage();
    void buildCirclePage();
    void buildArcPage();
    void buildTextPage();
    void applyProperties(int index, std::unique_ptr<Entity> newEntity);
};