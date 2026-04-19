#include "ShxFontParser.h"
#include "HersheyFontParser.h"
#include "SvgFontParser.h"
#include "HpglFontParser.h"
#include "JsonFontParser.h"
#include <QtMath>
#include <QDebug>
#include <QStringDecoder>
#include <QTextCodec>
#include <QDir>
#include <QFileInfo>
#include <cmath>

// 16-direction vector tables
const double ShxFontParser::VEC_X[16] = {
    1, 1, 1, 0.5, 0, -0.5, -1, -1, -1, -1, -1, -0.5, 0, 0.5, 1, 1
};
const double ShxFontParser::VEC_Y[16] = {
    0, 0.5, 1, 1, 1, 1, 1, 0.5, 0, -0.5, -1, -1, -1, -1, -1, -0.5
};

// ─── DataReader ────────────────────────────────────────────────

uint8_t ShxFontParser::DataReader::u8()
{
    if (index >= data.size()) return 0;
    return static_cast<uint8_t>(data.at(index++));
}

int8_t ShxFontParser::DataReader::i8()
{
    if (index >= data.size()) return 0;
    uint8_t v = static_cast<uint8_t>(data.at(index++));
    if (v > 127)
        return static_cast<int8_t>((v & 127) - 128);
    return static_cast<int8_t>(v);
}

int ShxFontParser::DataReader::octant()
{
    if (index >= data.size()) return 0;
    uint8_t v = static_cast<uint8_t>(data.at(index++));
    if (v & 128)
        return -(v & 127);
    return v;
}

uint16_t ShxFontParser::DataReader::u16()
{
    if (index + 1 >= data.size()) return 0;
    uint8_t lo = static_cast<uint8_t>(data.at(index++));
    uint8_t hi = static_cast<uint8_t>(data.at(index++));
    return lo | (hi << 8);
}

QByteArray ShxFontParser::DataReader::readStr()
{
    QByteArray result;
    while (index < data.size()) {
        char c = data.at(index++);
        if (c == 0) break;
        result.append(c);
    }
    return result;
}

QByteArray ShxFontParser::DataReader::readBytes(int n)
{
    QByteArray result = data.mid(index, n);
    index += n;
    return result;
}

// ─── Octant decode ─────────────────────────────────────────────

void ShxFontParser::decodeOctant(int specs, int& startOctant, int& octantSpan, bool& ccw)
{
    ccw = true;
    if (specs < 0) {
        ccw = false;
        specs = -specs;
    }
    startOctant = (specs >> 4) & 0xF;
    octantSpan = specs & 0xF;
}

// ─── Shape code parser ─────────────────────────────────────────

QList<int> ShxFontParser::parseShapeCodes(DataReader& reader, bool unifont)
{
    QList<int> codes;
    while (reader.hasData()) {
        int code = reader.u8();
        codes.append(code);
        if (code == 0) return codes;

        if (code > 14) continue;  // encoded vector, single byte
        if (code == 1 || code == 2 || code == 5 || code == 6 || code == 14)
            continue;  // single-byte codes

        if (code == 3 || code == 4) {
            codes.append(reader.u8());
        } else if (code == 7) {
            if (unifont) {
                codes.append(reader.u16());
            } else if (m_fontType == FontType::Bigfont) {
                int subCode = reader.u8();
                codes.append(subCode);
                if (subCode == 0) {
                    // Extended sub-shape: primitive#(u8 hi, u8 lo), basepoint(i8,i8), [width,] height
                    codes.append(reader.u8()); // primitive hi
                    codes.append(reader.u8()); // primitive lo
                    codes.append(reader.i8()); // basepoint x
                    codes.append(reader.i8()); // basepoint y
                    if (m_isExtendedBigfont) {
                        codes.append(reader.u8()); // width
                        codes.append(reader.u8()); // height
                    } else {
                        codes.append(reader.u8()); // height
                    }
                }
            } else {
                codes.append(reader.u8());
            }
        } else if (code == 8) {
            codes.append(reader.i8());
            codes.append(reader.i8());
        } else if (code == 9) {
            int x = 1, y = 1;
            while (x || y) {
                x = reader.i8();
                y = reader.i8();
                codes.append(x);
                codes.append(y);
            }
        } else if (code == 10) {
            codes.append(reader.u8());     // radius
            codes.append(reader.octant()); // octant specs
        } else if (code == 11) {
            codes.append(reader.u8());     // start offset
            codes.append(reader.u8());     // end offset
            codes.append(reader.u8());     // radius hi
            codes.append(reader.u8());     // radius lo
            codes.append(reader.octant()); // octant specs
        } else if (code == 12) {
            codes.append(reader.i8());     // x
            codes.append(reader.i8());     // y
            codes.append(reader.i8());     // bulge
        } else if (code == 13) {
            int x = 1, y = 1;
            while (x || y) {
                x = reader.i8();
                y = reader.i8();
                codes.append(x);
                codes.append(y);
                if (x || y)
                    codes.append(reader.i8()); // bulge
            }
        }
    }
    return codes;
}

// ─── Unifont parser ─────────────────────────────────────────────

QMap<int, ShxFontParser::ShxSymbol> ShxFontParser::parseUnifontShapes(DataReader& reader)
{
    QMap<int, ShxSymbol> shapes;
    while (reader.hasData()) {
        if (reader.index + 4 > reader.data.size()) break;
        int shapeNumber = reader.u16();
        int byteCount = reader.u16();
        if (byteCount <= 0) break;

        QByteArray name = reader.readStr();
        int dataBytes = byteCount - name.size() - 1;
        if (dataBytes <= 0 || reader.index + dataBytes > reader.data.size()) break;

        QByteArray dataRecord = reader.readBytes(dataBytes);
        DataReader dataReader(dataRecord, 0);
        QList<int> codes = parseShapeCodes(dataReader, true);

        ShxSymbol sym;
        sym.number = shapeNumber;
        sym.byteCount = byteCount;
        sym.name = name;
        sym.codes = codes;
        if (!codes.isEmpty() && codes.last() == 0)
            shapes[shapeNumber] = sym;
    }
    return shapes;
}

// ─── Shapes v1.0/1.1 parser ────────────────────────────────────

QMap<int, ShxFontParser::ShxSymbol> ShxFontParser::parseShapesV1(DataReader& reader)
{
    QMap<int, ShxSymbol> shapes;

    if (reader.u8() != 0x1A) return shapes;

    int firstNumber = reader.u16();
    int lastNumber = reader.u16();
    int shapeCount = reader.u16();
    (void)firstNumber;
    (void)lastNumber;

    struct IndexEntry { int number; int dataSize; };
    QList<IndexEntry> indexTable;
    for (int i = 0; i < shapeCount; ++i) {
        IndexEntry e;
        e.number = reader.u16();
        e.dataSize = reader.u16();
        indexTable.append(e);
    }

    for (const auto& entry : indexTable) {
        if (reader.index + entry.dataSize > reader.data.size()) break;
        QByteArray record = reader.readBytes(entry.dataSize);
        DataReader recReader(record, 0);
        QByteArray name = recReader.readStr();
        QList<int> codes = parseShapeCodes(recReader, false);

        ShxSymbol sym;
        sym.number = entry.number;
        sym.byteCount = entry.dataSize;
        sym.name = name;
        sym.codes = codes;
        if (!codes.isEmpty() && codes.last() == 0)
            shapes[entry.number] = sym;
    }
    return shapes;
}

// ─── Bigfont parser ─────────────────────────────────────────────

QMap<int, ShxFontParser::ShxSymbol> ShxFontParser::parseBigfontShapes(
    const QByteArray& fileData, DataReader& reader)
{
    QMap<int, ShxSymbol> shapes;

    // Header: itemLength(u16), count(u16), changeNumber(u16)
    /*int itemLength =*/ reader.u16();
    int count = reader.u16();
    int changeNumber = reader.u16();

    // Skip change table (changeNumber * 4 bytes)
    reader.skip(changeNumber * 4);

    // Read index table: count entries of (code_2bytes, length_u16, offset_u32)
    struct BigFontEntry {
        uint16_t code;
        uint16_t length;
        uint32_t offset;
    };
    QList<BigFontEntry> entries;

    for (int i = 0; i < count; ++i) {
        if (reader.index + 8 > reader.data.size()) break;

        // Read code as 2 raw bytes in little-endian order.
        // Double-byte (CJK) codes: LE (trail byte first), e.g. GBK D2BB → bytes [BB,D2] → code=0xD2BB
        // Single-byte codes: second byte is 0x00, code is just first byte, e.g. 0x8E → bytes [8E,00] → code=0x008E
        uint8_t b0 = reader.u8();
        uint8_t b1 = reader.u8();
        uint16_t code = (b1 == 0x00) ? b0 : ((b1 << 8) | b0);

        uint16_t length = reader.u16();

        // Read offset as LE u32
        uint8_t o0 = reader.u8();
        uint8_t o1 = reader.u8();
        uint8_t o2 = reader.u8();
        uint8_t o3 = reader.u8();
        uint32_t offset = o0 | (o1 << 8) | (o2 << 16) | (o3 << 24);

        if (code == 0 && length == 0 && offset == 0) continue;
        entries.append({code, length, offset});
    }

    // Read shape data for each entry from absolute file offsets
    for (const auto& entry : entries) {
        if (entry.offset + entry.length > static_cast<uint32_t>(fileData.size())) continue;

        QByteArray shapeData = fileData.mid(entry.offset, entry.length);

        if (entry.code == 0) {
            // Shape #0: font definition — parse info and store
            ShxSymbol sym;
            sym.number = 0;
            sym.byteCount = entry.length;

            // Find null terminator for info string
            int nullPos = shapeData.indexOf('\0');
            if (nullPos >= 0) {
                sym.name = shapeData.left(nullPos);
                int idx = nullPos + 1;
                int remaining = shapeData.size() - idx;

                if (remaining >= 5) {
                    // Extended bigfont: height, pad, orientation, width
                    m_isExtendedBigfont = true;
                    if (idx < shapeData.size())
                        sym.codes.append(static_cast<uint8_t>(shapeData[idx++])); // height/above
                    if (idx < shapeData.size())
                        sym.codes.append(static_cast<uint8_t>(shapeData[idx++])); // pad
                    if (idx < shapeData.size())
                        sym.codes.append(static_cast<uint8_t>(shapeData[idx++])); // orientation
                    if (idx < shapeData.size())
                        sym.codes.append(static_cast<uint8_t>(shapeData[idx++])); // width
                } else if (remaining >= 3) {
                    // Regular bigfont: baseUp, baseDown, orientation
                    m_isExtendedBigfont = false;
                    sym.codes.append(static_cast<uint8_t>(shapeData[idx++])); // baseUp
                    sym.codes.append(static_cast<uint8_t>(shapeData[idx++])); // baseDown
                    sym.codes.append(static_cast<uint8_t>(shapeData[idx++])); // orientation
                }
            }
            shapes[0] = sym;
        } else {
            // Regular character shape
            // Bigfont per-shape data format:
            //   [optional 2-byte code prefix] [name\0] [opcodes]              (regular bigfont)
            //   [optional 2-byte code prefix] [name\0] [mode] [ext?] [opcodes] (extended bigfont)
            // - Code prefix: 2-byte GBK character code (present for double-byte chars)
            // - Name: null-terminated string (empty name = single 0x00 byte)
            // - Mode (extended only): 1 byte (0=horiz, 1=vert, 2=both, >=4 adds 2 extension bytes)
            // - Extension (extended only): 2 bytes when mode has bit 2 set (mode >= 4)
            // - Then shape opcodes

            int dataStart = 0;

            // Check for 2-byte GBK code prefix at start of shape data.
            // The shape data stores the GBK code in natural (big-endian) byte order:
            // [leadByte, trailByte]. Since entry.code is the LE-decoded value
            // (leadByte << 8 | trailByte), compare [codeHi, codeLo].
            if (entry.code > 0xFF && shapeData.size() > 2 &&
                static_cast<uint8_t>(shapeData.at(0)) == ((entry.code >> 8) & 0xFF) &&
                static_cast<uint8_t>(shapeData.at(1)) == (entry.code & 0xFF)) {
                dataStart = 2;
            }

            // Find null terminator of name string
            int nullPos = shapeData.indexOf('\0', dataStart);
            if (nullPos < 0)
                nullPos = shapeData.size();
            int idx = nullPos + 1;

            // Only extended bigfonts have a mode byte + optional extension bytes
            // after the name. Regular bigfonts start opcodes immediately.
            if (m_isExtendedBigfont) {
                if (idx >= shapeData.size()) continue;
                uint8_t mode = static_cast<uint8_t>(shapeData.at(idx++));
                if (mode >= 4 && idx + 2 <= shapeData.size())
                    idx += 2;
            }

            DataReader shapeReader(shapeData, idx);
            QList<int> codes = parseShapeCodes(shapeReader, false);

            ShxSymbol sym;
            sym.number = entry.code;
            sym.byteCount = entry.length;
            sym.codes = codes;
            if (!codes.isEmpty() && codes.last() == 0)
                shapes[entry.code] = sym;
        }
    }

    return shapes;
}

// ─── RenderContext ──────────────────────────────────────────────

void ShxFontParser::RenderContext::moveTo(const QPointF& pt)
{
    cursor = pt;
}

void ShxFontParser::RenderContext::lineTo(const QPointF& pt)
{
    currentStroke.append(pt);
    cursor = pt;
}

void ShxFontParser::RenderContext::finishStroke()
{
    if (currentStroke.size() >= 2)
        strokes.append(currentStroke);
    currentStroke.clear();
}

void ShxFontParser::RenderContext::drawVector(int code)
{
    int angle = code & 0xF;
    int length = (code >> 4) & 0xF;
    drawDisplacement(VEC_X[angle] * length, VEC_Y[angle] * length);
}

void ShxFontParser::RenderContext::drawDisplacement(double x, double y)
{
    double scale = vectorLength;
    QPointF target(cursor.x() + x * scale, cursor.y() + y * scale);
    if (penDown) {
        if (currentStroke.isEmpty())
            currentStroke.append(cursor);
        lineTo(target);
    } else {
        finishStroke();
        moveTo(target);
    }
}

void ShxFontParser::RenderContext::drawArcSpan(double radius, double startAngle, double spanAngle, bool ccw)
{
    double endAngle = startAngle + (ccw ? spanAngle : -spanAngle);
    drawArcStartToEnd(radius, startAngle, endAngle, ccw);
}

void ShxFontParser::RenderContext::drawArcStartToEnd(double radius, double startAngle, double endAngle, bool ccw)
{
    if (radius <= 0.0) return;

    // Arc start point (on circle)
    QPointF arcStart(radius * std::cos(startAngle), radius * std::sin(startAngle));
    QPointF arcEnd(radius * std::cos(endAngle), radius * std::sin(endAngle));

    // The arc start should align with current cursor
    QPointF refPoint = ccw ? arcStart : arcEnd;
    QPointF center = cursor - refPoint;

    // Generate arc points
    double totalAngle = endAngle - startAngle;
    if (ccw && totalAngle < 0) totalAngle += 2.0 * M_PI;
    if (!ccw && totalAngle > 0) totalAngle -= 2.0 * M_PI;

    int segments = std::max(4, static_cast<int>(std::abs(totalAngle) / (M_PI / 8)));
    double step = totalAngle / segments;

    if (penDown) {
        if (currentStroke.isEmpty())
            currentStroke.append(cursor);
        for (int i = 1; i <= segments; ++i) {
            double a = startAngle + step * i;
            QPointF pt(center.x() + radius * std::cos(a),
                       center.y() + radius * std::sin(a));
            lineTo(pt);
        }
    } else {
        finishStroke();
        QPointF finalPt = ccw ? arcEnd : arcStart;
        moveTo(QPointF(center.x() + finalPt.x(), center.y() + finalPt.y()));
    }
}

void ShxFontParser::RenderContext::drawBulge(double x, double y, double bulge)
{
    if (penDown && std::abs(bulge) > 0.001) {
        QPointF startPt = cursor;
        double scale = vectorLength;
        QPointF endPt(startPt.x() + x * scale, startPt.y() + y * scale);
        double b = std::abs(bulge) / 127.0;
        bool ccw = bulge > 0;

        // Bulge-to-arc: bulge = tan(angle/4)
        double dx = endPt.x() - startPt.x();
        double dy = endPt.y() - startPt.y();
        double chordLen = std::sqrt(dx * dx + dy * dy);
        if (chordLen < 1e-10) {
            drawDisplacement(x, y);
            return;
        }
        double sagitta = b * chordLen / 2.0;
        double radius = (chordLen * chordLen / 4.0 + sagitta * sagitta) / (2.0 * sagitta);

        QPointF mid((startPt.x() + endPt.x()) / 2.0, (startPt.y() + endPt.y()) / 2.0);
        double perpX = -dy / chordLen;
        double perpY = dx / chordLen;

        double d = radius - sagitta;
        QPointF center;
        if (ccw)
            center = QPointF(mid.x() + perpX * d, mid.y() + perpY * d);
        else
            center = QPointF(mid.x() - perpX * d, mid.y() - perpY * d);

        double startAngle = std::atan2(startPt.y() - center.y(), startPt.x() - center.x());
        double endAngle = std::atan2(endPt.y() - center.y(), endPt.x() - center.x());

        double totalAngle = endAngle - startAngle;
        if (ccw && totalAngle < 0) totalAngle += 2.0 * M_PI;
        if (!ccw && totalAngle > 0) totalAngle -= 2.0 * M_PI;

        int segments = std::max(4, static_cast<int>(std::abs(totalAngle) / (M_PI / 8)));
        double step = totalAngle / segments;

        if (currentStroke.isEmpty())
            currentStroke.append(cursor);
        for (int i = 1; i <= segments; ++i) {
            double a = startAngle + step * i;
            QPointF pt(center.x() + radius * std::cos(a),
                       center.y() + radius * std::sin(a));
            lineTo(pt);
        }
    } else {
        drawDisplacement(x, y);
    }
}

void ShxFontParser::RenderContext::render(int shapeNumber, bool resetToBaseline)
{
    QList<int> codes;
    try {
        codes = getCodes(shapeNumber);
    } catch (...) {
        return;
    }
    if (codes.isEmpty()) return;

    double baselineY = cursor.y();
    // For bigfont shapes, pen starts UP — shapes use explicit PEN_DOWN opcodes.
    // For other types (unifont/shapes), pen starts DOWN per SHX convention.
    if (fontType == FontType::Bigfont)
        penDown = false;
    else
        penDown = true;
    bool skipNext = false;

    int idx = 0;
    while (idx < codes.size()) {
        int code = codes[idx++];

        if (code > 15 && !skipNext) {
            drawVector(code);
        } else if (code == 0) {
            break;
        } else if (code == 1 && !skipNext) {
            penDown = true;
        } else if (code == 2 && !skipNext) {
            penDown = false;
            finishStroke();
        } else if (code == 3 || code == 4) {
            int factor = codes[idx++];
            if (!skipNext) {
                if (code == 3) vectorLength /= factor;
                else vectorLength *= factor;
            }
        } else if (code == 5 && !skipNext) {
            locationStack.append(cursor);
        } else if (code == 6 && !skipNext) {
            finishStroke();
            if (!locationStack.isEmpty())
                cursor = locationStack.takeLast();
        } else if (code == 7) {
            int subShape = codes[idx++];
            if (fontType == FontType::Bigfont && subShape == 0) {
                // Extended sub-shape: primitive#(hi,lo), basepoint(x,y), [width,] height
                int primHi = codes[idx++];
                int primLo = codes[idx++];
                int primCode = (primHi << 8) | primLo;
                int bpX = codes[idx++];
                int bpY = codes[idx++];
                int subHeight, subWidth;
                if (isExtendedBigfont) {
                    subWidth = codes[idx++];
                    subHeight = codes[idx++];
                } else {
                    subHeight = codes[idx++];
                    subWidth = subHeight;
                }
                if (!skipNext && primCode != 0) {
                    // Render sub-shape at offset, scaled
                    QPointF savedCursor = cursor;
                    double savedVL = vectorLength;
                    bool savedPen = penDown;
                    QPointF offset(bpX * vectorLength, bpY * vectorLength);
                    cursor = savedCursor + offset;
                    render(primCode);
                    cursor = savedCursor;
                    vectorLength = savedVL;
                    penDown = savedPen;
                }
            } else {
                if (!skipNext)
                    render(subShape);
            }
        } else if (code == 8) {
            int x = codes[idx++];
            int y = codes[idx++];
            if (!skipNext) drawDisplacement(x, y);
        } else if (code == 9) {
            while (true) {
                int x = codes[idx++];
                int y = codes[idx++];
                if (x == 0 && y == 0) break;
                if (!skipNext) drawDisplacement(x, y);
            }
        } else if (code == 10) {
            int radius = codes[idx++];
            int octantSpecs = codes[idx++];
            int startOct, span;
            bool ccw;
            decodeOctant(octantSpecs, startOct, span, ccw);
            if (span == 0) span = 8;
            if (!skipNext)
                drawArcSpan(radius * vectorLength,
                            qDegreesToRadians(startOct * 45.0),
                            qDegreesToRadians(span * 45.0), ccw);
        } else if (code == 11) {
            int startOffset = codes[idx++];
            int endOffset = codes[idx++];
            int radiusHi = codes[idx++];
            int radiusLo = codes[idx++];
            int octantSpecs = codes[idx++];
            int radius = (radiusHi << 8) + radiusLo;
            int startOct, span;
            bool ccw;
            decodeOctant(octantSpecs, startOct, span, ccw);
            if (endOffset == 0) endOffset = 256;
            double binaryDeg = 45.0 / 256.0;
            double startAngle, endAngle;
            if (ccw) {
                int endOct = startOct + span - 1;
                startAngle = startOct * 45.0 + startOffset * binaryDeg;
                endAngle = endOct * 45.0 + endOffset * binaryDeg;
            } else {
                int endOct = startOct - span + 1;
                startAngle = startOct * 45.0 - startOffset * binaryDeg;
                endAngle = endOct * 45.0 - endOffset * binaryDeg;
            }
            if (!skipNext)
                drawArcStartToEnd(radius * vectorLength,
                                  qDegreesToRadians(startAngle),
                                  qDegreesToRadians(endAngle), ccw);
        } else if (code == 12) {
            int x = codes[idx++];
            int y = codes[idx++];
            int bulge = codes[idx++];
            if (!skipNext) drawBulge(x, y, bulge);
        } else if (code == 13) {
            while (true) {
                int x = codes[idx++];
                int y = codes[idx++];
                if (x == 0 && y == 0) break;
                int bulge = codes[idx++];
                if (!skipNext) drawBulge(x, y, bulge);
            }
        } else if (code == 14) {
            // Vertical text flag — skip next command if horizontal mode
            skipNext = true;
            continue;
        }
        skipNext = false;
    }

    finishStroke();

    if (resetToBaseline) {
        cursor.setY(baselineY);
    }
}

// ─── Main parse entry ──────────────────────────────────────────

QMap<QChar, QList<QList<QPointF>>> ShxFontParser::parse(QIODevice& device)
{
    QByteArray data = device.readAll();
    if (data.isEmpty())
        return {};

    QMap<int, ShxSymbol> shapes;
    int above = 0;

    if (data.startsWith("AutoCAD-86 unifont 1.0")) {
        // Unifont format
        m_fontType = FontType::Unifont;
        DataReader reader(data, 0x18);
        if (!reader.hasData() || reader.u8() != 0x1A)
            return {};

        // Skip: record count (u16), unknown (u16), font def record size (u16)
        reader.skip(6);

        // Font definition
        /*QByteArray name =*/ reader.readStr();
        above = reader.u8();
        /*int below =*/ reader.u8();
        /*int mode =*/ reader.u8();
        /*int encoding =*/ reader.u8();
        /*int embed =*/ reader.u8();
        reader.skip(1); // trailing 0x00

        shapes = parseUnifontShapes(reader);

    } else if (data.startsWith("AutoCAD-86 bigfont 1.0")) {
        // Bigfont format (Chinese/CJK)
        m_fontType = FontType::Bigfont;

        // Find 0x1A marker after header
        int headerEnd = data.indexOf('\x1A');
        if (headerEnd < 0) return {};
        DataReader reader(data, headerEnd + 1);

        shapes = parseBigfontShapes(data, reader);

        // Extract font metrics from shape #0
        if (shapes.contains(0)) {
            const auto& defCodes = shapes[0].codes;
            if (m_isExtendedBigfont) {
                // Extended: codes[0]=height, codes[1]=pad, codes[2]=orientation, codes[3]=width
                if (defCodes.size() >= 1) above = defCodes[0];
            } else {
                // Regular: codes[0]=baseUp, codes[1]=baseDown, codes[2]=orientation
                if (defCodes.size() >= 2)
                    above = defCodes[0] + defCodes[1]; // height = baseUp + baseDown
                else if (defCodes.size() >= 1)
                    above = defCodes[0];
            }
            shapes.remove(0);
        }

        // Bigfont files reference sub-shapes from a companion unifont.
        // Load the companion unifont so sub-shape references resolve correctly.
        if (!m_bigfontPath.isEmpty())
            mergeCompanionUnifont(m_bigfontPath, shapes, above);

        // Detect character encoding from shape code distribution
        detectBigfontEncoding(shapes);

    } else if (data.startsWith("AutoCAD-86 shapes 1.0") ||
               data.startsWith("AutoCAD-86 shapes 1.1")) {
        m_fontType = FontType::Shapes;
        DataReader reader(data, 0x17);
        shapes = parseShapesV1(reader);

        // Shape 0 is font definition
        if (shapes.contains(0)) {
            const auto& defCodes = shapes[0].codes;
            if (defCodes.size() >= 2) {
                above = defCodes[0];
            }
            shapes.remove(0);
        }
    } else {
        return {};
    }

    return renderAllGlyphs(shapes, above);
}

void ShxFontParser::mergeCompanionUnifont(const QString& bigfontPath,
                                            QMap<int, ShxSymbol>& shapes, int& above)
{
    static const QStringList companionNames = {
        "romans.shx", "romand.shx", "simplex.shx", "txt.shx",
        "monotxt.shx", "isocp.shx", "gothice.shx"
    };

    QFileInfo fi(bigfontPath);
    QDir dir = fi.absoluteDir();

    for (const QString& name : companionNames) {
        QString companionPath = dir.filePath(name);
        if (!QFile::exists(companionPath)) {
            companionPath = dir.filePath(name.toLower());
        }
        if (!QFile::exists(companionPath))
            continue;

        QFile file(companionPath);
        if (!file.open(QIODevice::ReadOnly))
            continue;

        QByteArray data = file.readAll();
        if (!data.startsWith("AutoCAD-86 unifont 1.0") &&
            !data.startsWith("AutoCAD-86 shapes 1.0") &&
            !data.startsWith("AutoCAD-86 shapes 1.1"))
            continue;

        ShxFontParser companionParser;
        companionParser.m_fontType = FontType::Unifont;
        if (data.startsWith("AutoCAD-86 unifont 1.0")) {
            DataReader reader(data, 0x18);
            if (!reader.hasData() || reader.u8() != 0x1A)
                continue;
            reader.skip(6);
            reader.readStr();
            int companionAbove = reader.u8();
            reader.u8(); // below
            reader.u8(); // mode
            reader.u8(); // encoding
            reader.u8(); // embed
            reader.skip(1);

            QMap<int, ShxSymbol> companionShapes = companionParser.parseUnifontShapes(reader);
            for (auto it = companionShapes.begin(); it != companionShapes.end(); ++it) {
                if (!shapes.contains(it.key()))
                    shapes.insert(it.key(), it.value());
            }
            if (above == 0)
                above = companionAbove;
        }
        break;
    }
}

void ShxFontParser::detectBigfontEncoding(const QMap<int, ShxSymbol>& shapes)
{
    // Analyze lead byte distribution of double-byte shape codes to detect encoding.
    // Key insight: Shift-JIS uses lead bytes 0x81-0x9F and 0xE0-0xEF for kanji,
    // with NO codes in 0xA0-0xDF. GBK/Big5 fonts typically have codes in 0xA0-0xDF
    // (especially 0xB0-0xD7 for common Chinese characters).
    bool hasCodesInA0_DF = false;
    bool hasCodesIn81_9F = false;
    bool hasCodesInB0_D7 = false;
    bool hasCodesInE0_EF = false;

    for (auto it = shapes.constBegin(); it != shapes.constEnd(); ++it) {
        int code = it.key();
        if (code <= 0xFF) continue;
        uint8_t lead = (code >> 8) & 0xFF;
        if (lead >= 0xA0 && lead <= 0xDF) hasCodesInA0_DF = true;
        if (lead >= 0x81 && lead <= 0x9F) hasCodesIn81_9F = true;
        if (lead >= 0xB0 && lead <= 0xD7) hasCodesInB0_D7 = true;
        if (lead >= 0xE0 && lead <= 0xEF) hasCodesInE0_EF = true;
    }

    if (!hasCodesInA0_DF && hasCodesIn81_9F && hasCodesInE0_EF) {
        // Shift-JIS: kanji in 0x81-0x9F + 0xE0-0xEF, no codes in 0xA0-0xDF
        m_bigfontCodecName = "Shift-JIS";
    } else if (hasCodesInB0_D7) {
        // GBK/GB2312: common Chinese characters in 0xB0-0xD7
        m_bigfontCodecName = "GBK";
    } else {
        // Default to GBK for other cases
        m_bigfontCodecName = "GBK";
    }
}

QMap<QChar, QList<QList<QPointF>>> ShxFontParser::renderAllGlyphs(
    const QMap<int, ShxSymbol>& shapes, int above)
{
    QMap<QChar, QList<QList<QPointF>>> result;

    // Build a codes lookup for sub-shape references
    auto getCodes = [&shapes](int number) -> QList<int> {
        if (shapes.contains(number))
            return shapes[number].codes;
        return {};
    };

    for (auto it = shapes.constBegin(); it != shapes.constEnd(); ++it) {
        int shapeNum = it.key();

        // Convert shape number to Unicode character
        QChar ch;
        if (m_fontType == FontType::Bigfont && shapeNum > 0xFF) {
            // Double-byte character: lead byte in high byte, trail byte in low byte
            uint8_t lead = (shapeNum >> 8) & 0xFF;
            uint8_t trail = shapeNum & 0xFF;
            QByteArray charBytes;
            charBytes.append(static_cast<char>(lead));
            charBytes.append(static_cast<char>(trail));
            // Use detected encoding (GBK, Shift-JIS, etc.) for decoding
            QTextCodec* codec = nullptr;
            if (!m_bigfontCodecName.isEmpty())
                codec = QTextCodec::codecForName(m_bigfontCodecName);
            if (!codec) codec = QTextCodec::codecForName("GBK");
            if (!codec) codec = QTextCodec::codecForName("GB18030");
            if (!codec) continue;
            QString decoded = codec->toUnicode(charBytes);
            if (decoded.isEmpty()) continue;
            ch = decoded.at(0);
        } else {
            if (shapeNum < 33 || shapeNum > 0xFFFF) continue; // skip control chars
            ch = QChar(shapeNum);
        }

        RenderContext ctx;
        ctx.getCodes = getCodes;
        ctx.fontType = m_fontType;
        ctx.isExtendedBigfont = m_isExtendedBigfont;
        ctx.cursor = QPointF(0, 0);
        ctx.vectorLength = 1.0;
        ctx.penDown = true;

        ctx.render(shapeNum);

            if (!ctx.strokes.isEmpty()) {
                result[ch] = ctx.strokes;
            }
    }

    // Also render space (32) to get advance width info — store as empty glyph
    // The advance width is the final cursor X position after rendering
    (void)above;

    return result;
}
