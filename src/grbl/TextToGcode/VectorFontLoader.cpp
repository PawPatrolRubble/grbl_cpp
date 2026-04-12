#include "VectorFontLoader.h"
#include "HersheyFontParser.h"
#include "SvgFontParser.h"
#include "HpglFontParser.h"
#include "JsonFontParser.h"
#include <QFileInfo>
#include <QFile>

std::shared_ptr<VectorFontParser> VectorFontLoader::getParserForFile(const QString& filePath)
{
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    if (ext == "jhf") return std::make_shared<HersheyFontParser>();
    if (ext == "svg") return std::make_shared<SvgFontParser>();
    if (ext == "hpgl" || ext == "glf") return std::make_shared<HpglFontParser>();
    if (ext == "json") return std::make_shared<JsonFontParser>();

    return nullptr;
}

FontLoadResult VectorFontLoader::loadFont(const QString& filePath)
{
    FontLoadResult result;

    QFileInfo fi(filePath);
    if (!fi.exists()) {
        result.errorMessage = QString("File not found: %1").arg(filePath);
        return result;
    }

    auto parser = getParserForFile(filePath);
    if (!parser) {
        result.errorMessage = QString("Unsupported font format: .%1").arg(fi.suffix());
        return result;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.errorMessage = QString("Cannot open file: %1").arg(filePath);
        return result;
    }

    result.glyphs = parser->parse(file);
    if (result.glyphs.isEmpty()) {
        result.errorMessage = QString("No glyphs found in font file: %1").arg(fi.fileName());
        return result;
    }

    result.success = true;
    return result;
}