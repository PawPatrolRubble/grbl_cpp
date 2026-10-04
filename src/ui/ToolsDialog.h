#pragma once

#include <QDialog>
#include <QUuid>
#include <array>

class ToolLibrary;
class QListWidget;
class QLineEdit;
class QPushButton;
class QCheckBox;
class QLabel;
class QDoubleSpinBox;
class QPlainTextEdit;
class QComboBox;

class ToolsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ToolsDialog(ToolLibrary& library, QWidget* parent = nullptr);

private:
    QUuid selectedToolId() const;
    void refreshTools();
    void loadSelectedTool();
    void saveSelectedTool();
    void updateColorButton();
    void loadRackSettings();
    void saveRackSettings();
    void refreshPreviewPens();
    void updatePreview();

    ToolLibrary& m_library;
    QListWidget* m_list;
    QLineEdit* m_name;
    QPushButton* m_color;
    QPushButton* m_remove;
    QCheckBox* m_enabled;
    QLabel* m_status;
    QCheckBox* m_stationConfigured;
    QCheckBox* m_stationCalibrated;
    QLineEdit* m_slot;
    QDoubleSpinBox *m_slotX, *m_slotY, *m_returnZ, *m_pickupZ;
    QPlainTextEdit *m_returnMacro, *m_pickupMacro;
    QCheckBox* m_rackConfigured;
    QCheckBox* m_rackCalibrated;
    enum RackField { ClearanceZ, TravelFeed, DockFeed, SettleSeconds, LocalRadius,
                     MinX, MaxX, MinY, MaxY, MinZ, MaxZ, RackFieldCount };
    std::array<QDoubleSpinBox*, RackFieldCount> m_rackValues{};
    QComboBox *m_previewCurrent, *m_previewNext, *m_workCoordinateSystem;
    QDoubleSpinBox* m_drawingFeed;
    QPlainTextEdit* m_previewText;
    QLabel* m_previewStatus;
    bool m_loading = false;
    bool m_saving = false;
};
