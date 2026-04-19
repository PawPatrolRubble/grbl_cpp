#pragma once

#include <QList>
#include <QPair>
#include <QString>

struct BuiltinFont {
    QString path;
    QString name;
};

QList<BuiltinFont> builtinVectorFonts();

QList<QPair<QString, QString>> findInstalledShxFonts();

bool isBigfontShx(const QString &path);

bool textContainsCJK(const QString &text);