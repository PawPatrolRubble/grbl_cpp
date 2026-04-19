#include "HersheyFontParser.h"
#include <QTextStream>

QMap<QChar, QList<QList<QPointF>>> HersheyFontParser::parse(QIODevice& device)
{
    QMap<QChar, QList<QList<QPointF>>> glyphs;

    QByteArray rawData = device.readAll();
    QString text = QString::fromLatin1(rawData);
    QStringList lines = text.split('\n');

    // Collect glyph entries: each starts with a line beginning with digits
    struct GlyphEntry { QString data; };
    QList<GlyphEntry> entries;

    for (const QString& line : lines) {
        if (line.trimmed().isEmpty()) continue;

        // A line starting with digits (in columns 1-5) begins a new glyph
        bool isNewGlyph = false;
        if (line.length() >= 5) {
            isNewGlyph = true;
            for (int i = 0; i < 5; ++i) {
                if (!line[i].isDigit() && !line[i].isSpace()) {
                    isNewGlyph = false;
                    break;
                }
            }
            // Must have at least one digit
            if (isNewGlyph) {
                bool hasDigit = false;
                for (int i = 0; i < 5; ++i) {
                    if (line[i].isDigit()) { hasDigit = true; break; }
                }
                isNewGlyph = hasDigit;
            }
        }

        if (isNewGlyph) {
            GlyphEntry entry;
            entry.data = line.mid(5); // everything after the 5-digit number
            entries.append(entry);
        } else if (!entries.isEmpty()) {
            // Continuation of previous glyph
            entries.last().data += line;
        }
    }

    // Parse each glyph: mapping is sequential starting at ASCII 32 (space)
    for (int gi = 0; gi < entries.size(); ++gi) {
        QChar ch = QChar(32 + gi);
        if (ch.unicode() > 126) break;

        const QString& raw = entries[gi].data;

        // Parse vertex count (whitespace-padded integer)
        int pos = 0;
        while (pos < raw.length() && raw[pos].isSpace()) pos++;
        int countStart = pos;
        while (pos < raw.length() && raw[pos].isDigit()) pos++;
        if (pos == countStart) continue;
        int vertexCount = raw.mid(countStart, pos - countStart).toInt();

        if (vertexCount < 1 || pos + 2 > raw.length()) continue;

        // Left and right bearings
        int lb = raw[pos].toLatin1() - 'R';
        pos += 2; // skip both bearing chars

        // Parse coordinate pairs
        QList<QList<QPointF>> strokes;
        QList<QPointF> currentStroke;
        int pairsRead = 1; // bearing counts as one pair

        while (pairsRead < vertexCount && pos + 1 < raw.length()) {
            char c1 = raw[pos].toLatin1();
            char c2 = raw[pos + 1].toLatin1();
            pos += 2;
            pairsRead++;

            if (c1 == ' ' && c2 == 'R') {
                // Pen-up: end current stroke, start new one
                if (!currentStroke.isEmpty()) {
                    strokes.append(currentStroke);
                    currentStroke.clear();
                }
            } else {
                int x = c1 - 'R';
                int y = c2 - 'R';
                // Shift x so character starts at x=0, flip Y for Y-up convention
                currentStroke.append(QPointF(x - lb, -y));
            }
        }

        if (!currentStroke.isEmpty()) {
            strokes.append(currentStroke);
        }

        // Space has no strokes but should be included
        glyphs.insert(ch, strokes);
    }

    return glyphs;
}
