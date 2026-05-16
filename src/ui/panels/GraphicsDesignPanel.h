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
class QSpinBox;
class QSlider;
class QStackedWidget;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QGroupBox;
class LineNumberPlainTextEdit;
class Entity;
class ToolBarWidget;
class TransformPanel;
class PolylinePage;
class PropertyPage;

class GraphicsDesignPanel : public QWidget
{
    Q_OBJECT

public:
    explicit GraphicsDesignPanel(QWidget *parent = nullptr);
    ~GraphicsDesignPanel() = default;

    void setWorktableSize(const QSizeF &size);
    DesignCanvas* canvas() const { return m_canvas; }

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
    void applyTextProperties();
    void applyFlipH();
    void applyFlipV();
    void applyAlignLeft();
    void applyAlignCenterH();
    void applyAlignRight();
    void applyAlignTop();
    void applyAlignCenterV();
    void applyAlignBottom();
    void onMeasurementChanged();
    void buildLayerSection();
    void refreshLayerList();
    void onLayerItemChanged(QListWidgetItem *item);
    void onAddLayerClicked();
    void onDeleteLayerClicked();
    void onLockLayerClicked();
    void onLayerCurrentRowChanged(int row);
    void onMoveEntityToLayerClicked();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QStringList m_generatedLines;

    DesignCanvas *m_canvas;
    ToolBarWidget *m_toolBar;
    QWidget *m_floatingPropertyPanel = nullptr;
    QToolButton *m_collapseBtn = nullptr;
    bool m_propertyPanelCollapsed = false;
    QSlider *m_opacitySlider = nullptr;
    QComboBox *m_toolModeCombo;

    QPushButton *m_deleteBtn;
    QPushButton *m_clearBtn;
    QPushButton *m_saveBtn;
    QPushButton *m_loadBtn;
    QPushButton *m_exportBtn;
    QPushButton *m_generateBtn;
    QToolButton *m_snapBtn;
    QToolButton *m_snapGridBtn = nullptr;
    QToolButton *m_snapEndBtn  = nullptr;
    QToolButton *m_snapMidBtn  = nullptr;
    QToolButton *m_snapCtrBtn  = nullptr;
    QToolButton *m_snapIntBtn  = nullptr;
    QToolButton *m_snapPerpBtn = nullptr;
    QToolButton *m_snapNearBtn = nullptr;
    QDoubleSpinBox *m_gridSizeSpin = nullptr;
    QLabel         *m_filletRadiusLabel  = nullptr;
    QDoubleSpinBox *m_filletRadiusSpin   = nullptr;
    QLabel         *m_chamferDistLabel   = nullptr;
    QDoubleSpinBox *m_chamferDistSpin    = nullptr;
    QLabel         *m_offsetDistLabel    = nullptr;
    QDoubleSpinBox *m_offsetDistSpin     = nullptr;
    QGroupBox      *m_measureGroup       = nullptr;
    QLabel         *m_measureDistLabel   = nullptr;
    QLabel         *m_measureDxLabel     = nullptr;
    QLabel         *m_measureDyLabel     = nullptr;
    QLabel         *m_measureAngleLabel  = nullptr;
    QLabel *m_lineCountLabel;
    QLabel *m_statusLabel;
    QDoubleSpinBox *m_feedSpeedSpinBox;
    QDoubleSpinBox *m_plungeSpeedSpinBox;
    QDoubleSpinBox *m_fastHeightSpinBox;
    QDoubleSpinBox *m_workHeightSpinBox;
    LineNumberPlainTextEdit *m_gcodePreview;
    QListWidget *m_entityList;

    QStackedWidget *m_propertyStack;
    bool m_updatingUi = false;

    // Text property widgets (no TextPage extracted yet �?kept inline)
    QDoubleSpinBox *m_textPosX;
    QDoubleSpinBox *m_textPosY;
    QPlainTextEdit *m_textEdit;
    QDoubleSpinBox *m_textFontSize;
    QDoubleSpinBox *m_textLineHeight;
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

    // Extracted property pages
    PolylinePage *m_polylinePage = nullptr;  ///< needed for currentRow() in insert/delete ops

    // Transform panel (extracted component)
    TransformPanel *m_transformPanel = nullptr;

    // Layer management
    QGroupBox *m_layerGroup = nullptr;
    QListWidget *m_layerList = nullptr;
    QPushButton *m_addLayerBtn = nullptr;
    QPushButton *m_deleteLayerBtn = nullptr;
    QPushButton *m_lockLayerBtn = nullptr;
    QPushButton *m_moveToLayerBtn = nullptr;

    void setupUi();
    void setupConnections();
    void buildTextPage();          ///< Text entity page (inline �?no PropertyPage subclass yet)
    void refreshEntityList();
    void onEntityItemChanged(QListWidgetItem *item);
    void applyProperties(int index, std::unique_ptr<Entity> newEntity);

    // Transform helpers (called from TransformPanel signal lambdas)
    void applyMoveBy(double dx, double dy);
    void applyRotateBy(double angle);
    void applyRectArray(int rows, int cols, double spacingX, double spacingY);
    void applyScaleBy(double factor);
    void applyPolarArray(int count, double spanDeg, double cx, double cy);
    void applyMirrorByAngle(double axisAngle);
};
