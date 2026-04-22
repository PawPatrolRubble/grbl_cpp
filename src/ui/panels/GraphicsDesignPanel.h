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
class QStackedWidget;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QTableWidget;
class QGroupBox;
class LineNumberPlainTextEdit;
class Entity;

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
    void applyLineProperties();
    void applyRectProperties();
    void applyCircleProperties();
    void applyArcProperties();
    void applyTextProperties();
    void applyPolylineProperties();
    void polylineInsertVertex();
    void polylineDeleteVertex();
    void applyDimensionProperties();
    void applyAngularDimensionProperties();
    void applyRadiusDimensionProperties();
    void applyMoveBy();
    void applyRotateBy();
    void applyFlipH();
    void applyFlipV();
    void applyRectArray();
    void applyScaleBy();
    void applyPolarArray();
    void buildLayerSection();
    void refreshLayerList();
    void onLayerItemChanged(QListWidgetItem *item);
    void onAddLayerClicked();
    void onDeleteLayerClicked();
    void onLockLayerClicked();
    void onLayerCurrentRowChanged(int row);
    void onMoveEntityToLayerClicked();

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
    QDoubleSpinBox *m_gridSizeSpin = nullptr;
    QLabel         *m_filletRadiusLabel  = nullptr;
    QDoubleSpinBox *m_filletRadiusSpin   = nullptr;
    QLabel         *m_chamferDistLabel   = nullptr;
    QDoubleSpinBox *m_chamferDistSpin    = nullptr;
    QLabel         *m_offsetDistLabel    = nullptr;
    QDoubleSpinBox *m_offsetDistSpin     = nullptr;
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

    QLabel *m_polylinePointCountLabel = nullptr;
    QTableWidget *m_polylineTable = nullptr;
    QPushButton *m_polylineInsertBtn = nullptr;
    QPushButton *m_polylineDeleteBtn = nullptr;

    // Dimension property widgets
    QDoubleSpinBox *m_dimP1X = nullptr;
    QDoubleSpinBox *m_dimP1Y = nullptr;
    QDoubleSpinBox *m_dimP2X = nullptr;
    QDoubleSpinBox *m_dimP2Y = nullptr;
    QDoubleSpinBox *m_dimOffset = nullptr;

    // Angular dimension property widgets
    QDoubleSpinBox *m_adimVertexX = nullptr;
    QDoubleSpinBox *m_adimVertexY = nullptr;
    QDoubleSpinBox *m_adimP1X = nullptr;
    QDoubleSpinBox *m_adimP1Y = nullptr;
    QDoubleSpinBox *m_adimP2X = nullptr;
    QDoubleSpinBox *m_adimP2Y = nullptr;
    QDoubleSpinBox *m_adimRadius = nullptr;

    // Radius dimension property widgets
    QDoubleSpinBox *m_rdimCX = nullptr;
    QDoubleSpinBox *m_rdimCY = nullptr;
    QDoubleSpinBox *m_rdimRadius = nullptr;
    QDoubleSpinBox *m_rdimAngle = nullptr;
    QCheckBox     *m_rdimDiameter = nullptr;

    // Transform section (move / rotate, always visible when entity is selected)
    QGroupBox *m_transformGroup = nullptr;
    QDoubleSpinBox *m_moveDx = nullptr;
    QDoubleSpinBox *m_moveDy = nullptr;
    QDoubleSpinBox *m_rotateAngle = nullptr;
    QSpinBox       *m_arrayRows   = nullptr;
    QSpinBox       *m_arrayCols   = nullptr;
    QDoubleSpinBox *m_arraySpacingX = nullptr;
    QDoubleSpinBox *m_arraySpacingY = nullptr;
    QDoubleSpinBox *m_scaleFactor   = nullptr;
    // Polar array
    QDoubleSpinBox *m_polarCenterX  = nullptr;
    QDoubleSpinBox *m_polarCenterY  = nullptr;
    QSpinBox       *m_polarCount    = nullptr;
    QDoubleSpinBox *m_polarSpan     = nullptr;

    // Layer management
    QGroupBox *m_layerGroup = nullptr;
    QListWidget *m_layerList = nullptr;
    QPushButton *m_addLayerBtn = nullptr;
    QPushButton *m_deleteLayerBtn = nullptr;
    QPushButton *m_lockLayerBtn = nullptr;
    QPushButton *m_moveToLayerBtn = nullptr;

    void setupUi();
    void setupConnections();
    void buildEmptyPage();
    void buildLinePage();
    void buildRectPage();
    void buildCirclePage();
    void buildArcPage();
    void buildTextPage();
    void buildPolylinePage();
    void buildDimensionPage();
    void buildAngularDimensionPage();
    void buildRadiusDimensionPage();
    void buildTransformSection();
    void refreshEntityList();
    void onEntityItemChanged(QListWidgetItem *item);
    void applyProperties(int index, std::unique_ptr<Entity> newEntity);
};