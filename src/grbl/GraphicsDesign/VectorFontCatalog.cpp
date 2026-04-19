#include "VectorFontCatalog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

QList<BuiltinFont> builtinVectorFonts()
{
    return {
        {":/fonts/fonts/futural.jhf",    "Sans Serif Light"},
        {":/fonts/fonts/futuram.jhf",    "Sans Serif Medium"},
        {":/fonts/fonts/rowmans.jhf",    "Serif Simplex"},
        {":/fonts/fonts/rowmand.jhf",    "Serif Duplex"},
        {":/fonts/fonts/timesr.jhf",     "Times Roman"},
        {":/fonts/fonts/timesi.jhf",     "Times Italic"},
        {":/fonts/fonts/scripts.jhf",    "Script Simplex"},
        {":/fonts/fonts/scriptc.jhf",    "Script Complex"},
        {":/fonts/fonts/cursive.jhf",    "Cursive"},
        {":/fonts/fonts/gothiceng.jhf",  "Gothic English"},
    };
}

QList<QPair<QString, QString>> findInstalledShxFonts()
{
    QList<QPair<QString, QString>> result;

    QString appPath = QCoreApplication::applicationDirPath();
    QDir d(appPath + "/bigfonts");
    if (d.exists()) {
        const auto entries = d.entryInfoList({"*.shx"}, QDir::Files, QDir::Name);
        for (const QFileInfo &fi : entries) {
            QString path = fi.absoluteFilePath();
            QString label = fi.baseName();
            if (isBigfontShx(path))
                label += " (CJK)";
            result.append({path, label});
        }
    }

    return result;
}

bool isBigfontShx(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QByteArray header = f.read(32);
    return header.contains("bigfont");
}

bool textContainsCJK(const QString &text)
{
    for (const QChar &c : text) {
        ushort u = c.unicode();
        if ((u >= 0x4E00 && u <= 0x9FFF) || (u >= 0x3400 && u <= 0x4DBF) ||
            (u >= 0x3000 && u <= 0x303F) || (u >= 0xFF00 && u <= 0xFFEF) ||
            (u >= 0x3040 && u <= 0x30FF)) {
            return true;
        }
    }
    return false;
}