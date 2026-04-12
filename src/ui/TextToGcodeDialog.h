#pragma once

#include <QDialog>
#include <QStringList>

namespace Ui {
class TextToGcodeDialog;
}

class TextToGcodeDialog : public QDialog
{
    Q_OBJECT

public:
    enum class FontSource {
        SystemFont = 0,
        CustomVector = 1
    };

    explicit TextToGcodeDialog(QWidget *parent = nullptr);
    ~TextToGcodeDialog();

signals:
    void gcodeGenerated(const QStringList &lines);

private slots:
    void onBrowseClicked();
    void onGenerateClicked();
    void updatePreview();

private:
    Ui::TextToGcodeDialog *ui;
    QStringList m_generatedLines;
};