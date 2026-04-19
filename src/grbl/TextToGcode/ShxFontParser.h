#pragma once

#include "VectorFontParser.h"
#include <QList>
#include <QPointF>
#include <QMap>
#include <QChar>
#include <cstdint>
#include <functional>

/// Parser for AutoCAD SHX compiled shape font files.
/// Supports "AutoCAD-86 unifont 1.0", "AutoCAD-86 shapes 1.0/1.1",
/// and "AutoCAD-86 bigfont 1.0" (Chinese/CJK) formats.
class ShxFontParser : public VectorFontParser
{
public:
    QMap<QChar, QList<QList<QPointF>>> parse(QIODevice& device) override;
    void setFilePath(const QString& path) { m_bigfontPath = path; }
    QString formatName() const override { return "SHX"; }

private:
    enum class FontType { Unifont, Shapes, Bigfont };

    FontType m_fontType = FontType::Unifont;
    bool m_isExtendedBigfont = false;
    QString m_bigfontPath;
    QByteArray m_bigfontCodecName;  // Detected encoding for bigfont (GBK, Shift-JIS, Big5, etc.)

    // Binary reader helper
    struct DataReader {
        const QByteArray& data;
        int index = 0;

        explicit DataReader(const QByteArray& d, int startIndex = 0)
            : data(d), index(startIndex) {}

        bool hasData() const { return index < data.size(); }
        void skip(int n) { index += n; }

        uint8_t u8();
        int8_t i8();
        int octant();
        uint16_t u16();
        QByteArray readStr();
        QByteArray readBytes(int n);
    };

    // Shape record
    struct ShxSymbol {
        int number = 0;
        int byteCount = 0;
        QByteArray name;
        QList<int> codes;
    };

    // Shape renderer state
    struct RenderContext {
        QList<QList<QPointF>> strokes;
        QList<QPointF> currentStroke;
        QPointF cursor{0, 0};
        double vectorLength = 1.0;
        bool penDown = true;
        QList<QPointF> locationStack;

        FontType fontType = FontType::Unifont;
        bool isExtendedBigfont = false;

        std::function<QList<int>(int)> getCodes;

        void moveTo(const QPointF& pt);
        void lineTo(const QPointF& pt);
        void drawVector(int code);
        void drawDisplacement(double x, double y);
        void drawArcSpan(double radius, double startAngle, double spanAngle, bool ccw);
        void drawArcStartToEnd(double radius, double startAngle, double endAngle, bool ccw);
        void drawBulge(double x, double y, double bulge);
        void finishStroke();
        void render(int shapeNumber, bool resetToBaseline = false);
    };

    // Direction lookup tables (16 directions)
    static const double VEC_X[16];
    static const double VEC_Y[16];

    // Format-specific loaders
    QMap<int, ShxSymbol> parseUnifontShapes(DataReader& reader);
    QMap<int, ShxSymbol> parseShapesV1(DataReader& reader);
    QMap<int, ShxSymbol> parseBigfontShapes(const QByteArray& fileData, DataReader& reader);
    QList<int> parseShapeCodes(DataReader& reader, bool unifont);

    void mergeCompanionUnifont(const QString& bigfontPath,
                               QMap<int, ShxSymbol>& shapes, int& above);

    // Detect bigfont character encoding from shape code distribution
    void detectBigfontEncoding(const QMap<int, ShxSymbol>& shapes);

    // Decode octant byte → (startOctant, octantSpan, ccw)
    static void decodeOctant(int specs, int& startOctant, int& octantSpan, bool& ccw);

    // Convert parsed shapes to glyph map
    QMap<QChar, QList<QList<QPointF>>> renderAllGlyphs(
        const QMap<int, ShxSymbol>& shapes, int above);
};
