#include "SvgFontParser.h"

#include "SvgFontParser.h"
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QPolygonF>

QMap<QChar, QList<QList<QPointF>>> SvgFontParser::parse(QIODevice& device)
{
    QMap<QChar, QList<QList<QPointF>>> glyphs;
    QXmlStreamReader xml(&device);
    
    QString currentGroupId;

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement) {
            auto name = xml.name();
            
            if (name == QLatin1String("g")) {
                currentGroupId = xml.attributes().value("id").toString();
            }
            else if (name == QLatin1String("glyph")) {
                QString unicode = xml.attributes().value("unicode").toString();
                QString d = xml.attributes().value("d").toString();
                
                if (!unicode.isEmpty() && !d.isEmpty()) {
                    QPainterPath path = parseSvgPath(d);
                    QList<QList<QPointF>> strokes;
                    for (const QPolygonF& poly : path.toSubpathPolygons()) {
                        QList<QPointF> stroke;
                        for (int i=0; i<poly.size(); ++i) stroke.append(poly.at(i));
                        strokes.append(stroke);
                    }
                    glyphs.insert(unicode.at(0), strokes);
                }
            }
            else if (name == QLatin1String("path")) {
                QString id = xml.attributes().value("id").toString();
                if (id.isEmpty()) {
                    id = currentGroupId;
                }
                
                QString d = xml.attributes().value("d").toString();
                if (!id.isEmpty() && !d.isEmpty()) {
                    QPainterPath path = parseSvgPath(d);
                    QList<QList<QPointF>> strokes;
                    
                    for (const QPolygonF& poly : path.toSubpathPolygons()) {
                        QList<QPointF> stroke;
                        for (int i=0; i<poly.size(); ++i) stroke.append(poly.at(i));
                        strokes.append(stroke);
                    }
                    
                    QChar ch = id.at(0);
                    if (glyphs.contains(ch)) {
                        glyphs[ch].append(strokes);
                    } else {
                        glyphs.insert(ch, strokes);
                    }
                }
            }
        }
        else if (token == QXmlStreamReader::EndElement) {
            if (xml.name() == QLatin1String("g")) {
                currentGroupId.clear();
            }
        }
    }
    
    return glyphs;
}

QPainterPath SvgFontParser::parseSvgPath(const QString& pathData)
{
    QPainterPath path;
    
    QRegularExpression re("([a-zA-Z])|([-+]?(?:[0-9]*\\.[0-9]+|[0-9]+)(?:[eE][-+]?[0-9]+)?)");
    QRegularExpressionMatchIterator i = re.globalMatch(pathData);
    
    QChar cmd = 'M';
    QPointF currentPos(0,0);
    QPointF startPos(0,0);
    
    QList<double> args;
    
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        QString op = match.captured(1);
        QString num = match.captured(2);
        
        if (!op.isEmpty()) {
            cmd = op.at(0);
            args.clear();
        } else if (!num.isEmpty()) {
            args.append(num.toDouble());
            
            bool isRelative = cmd.isLower();
            QChar ucmd = cmd.toUpper();
            
            if (ucmd == 'Z') {
                path.closeSubpath();
                currentPos = startPos;
                args.clear();
            }
            else if (ucmd == 'M' && args.size() == 2) {
                QPointF p(args[0], args[1]);
                if (isRelative) p += currentPos;
                
                path.moveTo(p);
                currentPos = p;
                startPos = p;
                args.clear();
                
                cmd = isRelative ? 'l' : 'L';
            }
            else if (ucmd == 'L' && args.size() == 2) {
                QPointF p(args[0], args[1]);
                if (isRelative) p += currentPos;
                path.lineTo(p);
                currentPos = p;
                args.clear();
            }
            else if (ucmd == 'H' && args.size() == 1) {
                double x = args[0];
                if (isRelative) x += currentPos.x();
                QPointF p(x, currentPos.y());
                path.lineTo(p);
                currentPos = p;
                args.clear();
            }
            else if (ucmd == 'V' && args.size() == 1) {
                double y = args[0];
                if (isRelative) y += currentPos.y();
                QPointF p(currentPos.x(), y);
                path.lineTo(p);
                currentPos = p;
                args.clear();
            }
            else if (ucmd == 'C' && args.size() == 6) {
                QPointF c1(args[0], args[1]);
                QPointF c2(args[2], args[3]);
                QPointF endPt(args[4], args[5]);
                
                if (isRelative) {
                    c1 += currentPos;
                    c2 += currentPos;
                    endPt += currentPos;
                }
                
                path.cubicTo(c1, c2, endPt);
                currentPos = endPt;
                args.clear();
            }
            else if (ucmd == 'S' && args.size() == 4) {
                QPointF c2(args[0], args[1]);
                QPointF endPt(args[2], args[3]);
                if (isRelative) {
                    c2 += currentPos;
                    endPt += currentPos;
                }
                path.cubicTo(currentPos, c2, endPt); // simplified S mapping
                currentPos = endPt;
                args.clear();
            }
            else if (ucmd == 'Q' && args.size() == 4) {
                QPointF c(args[0], args[1]);
                QPointF endPt(args[2], args[3]);
                if (isRelative) {
                    c += currentPos;
                    endPt += currentPos;
                }
                path.quadTo(c, endPt);
                currentPos = endPt;
                args.clear();
            }
        }
        
        if (!op.isEmpty() && op.at(0).toUpper() == 'Z') {
            path.closeSubpath();
            currentPos = startPos;
        }
    }
    
    return path;
}
